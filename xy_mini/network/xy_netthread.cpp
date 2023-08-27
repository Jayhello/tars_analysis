//
// Created by wenwen on 2023/8/18.
//

#include "xy_netthread.h"
#include "xy_bind_adapter.h"
#include "xy_handle.h"
#include "xy_epoll_server.h"

namespace xy {

NetThread::NetThread(TC_EpollServer *epollServer, int threadIndex)
        : _epollServer(epollServer), _threadIndex(threadIndex), _bTerminate(false), _list(this),
          _bEmptyConnAttackCheck(false), _iEmptyCheckTimeout(MIN_EMPTY_CONN_TIMEOUT),
          _nUdpRecvBufferSize(DEFAULT_RECV_BUFFERSIZE) {
    _epoller.create(10240);

    _notify.init(&_epoller);
    _notify.add(_notify.notifyFd());
}

NetThread::~NetThread() {
}

void NetThread::debug(const string &s) const {
    _epollServer->debug(s);
}

void NetThread::info(const string &s) const {
    _epollServer->info(s);
}

void NetThread::tars(const string &s) const {
    _epollServer->tars(s);
}

void NetThread::error(const string &s) const {
    _epollServer->error(s);
}

void NetThread::enAntiEmptyConnAttack(bool bEnable) {
    _bEmptyConnAttackCheck = bEnable;
}

void NetThread::setEmptyConnTimeout(int timeout) {
    _iEmptyCheckTimeout = (timeout >= MIN_EMPTY_CONN_TIMEOUT) ? timeout : MIN_EMPTY_CONN_TIMEOUT;
}

void NetThread::setUdpRecvBufferSize(size_t nSize) {
    _nUdpRecvBufferSize = (nSize >= 8192 && nSize <= DEFAULT_RECV_BUFFERSIZE) ? nSize : DEFAULT_RECV_BUFFERSIZE;
}

bool NetThread::isEmptyConnCheck() const {
    return _bEmptyConnAttackCheck;
}

int NetThread::getEmptyConnTimeout() const {
    return _iEmptyCheckTimeout;
}

void NetThread::createEpoll(uint32_t maxAllConn) {
    _list.init((uint32_t) maxAllConn, _threadIndex + 1);
}

void NetThread::initUdp(const unordered_map<int, BindAdapterPtr> &listeners) {
    //监听socket
    auto it = listeners.begin();

    while (it != listeners.end()) {
        if (!it->second->getEndpoint().isTcp()) {
            Connection *cPtr = new Connection(it->second.get(), it->first);
            //udp分配接收buffer
            cPtr->setRecvBuffer(_nUdpRecvBufferSize);

            //addUdpConnection(cPtr);
            _epollServer->addConnection(cPtr, it->first, UDP_CONNECTION);
        }

        ++it;
    }
}

void NetThread::terminate() {
    _bTerminate = true;

    _notify.notify();
}

void NetThread::addTcpConnection(Connection *cPtr) {
    uint32_t uid = _list.getUniqId();

    cPtr->init(uid);

    _list.add(cPtr, cPtr->getTimeout() + TNOW);

    cPtr->getBindAdapter()->increaseNowConnection();

    /*
#if TARS_SSL
    if (cPtr->getBindAdapter()->getEndpoint().isSSL())
    {
        cPtr->getBindAdapter()->getEpollServer()->info("[TARS][addTcpConnection ssl connection");

        // 分配ssl对象, ctxName 放在obj proxy里
        cPtr->_openssl = TC_OpenSSL::newSSL(cPtr->getBindAdapter()->_ctx);
        if (!cPtr->_openssl)
        {
            cPtr->getBindAdapter()->getEpollServer()->error("[TARS][SSL_accept not find server cert");
            cPtr->close();
            return;
        }

        cPtr->_openssl->recvBuffer()->setConnection(cPtr);
        cPtr->_openssl->init(true);
        cPtr->_openssl->setReadBufferSize(1024 * 8);
        cPtr->_openssl->setWriteBufferSize(1024 * 8);

        int ret = cPtr->_openssl->doHandshake(cPtr->_sendBuffer);
        if (ret != 0)
        {
            cPtr->getBindAdapter()->getEpollServer()->error("[TARS][SSL_accept " + cPtr->getBindAdapter()->getEndpoint().toString() + " error: " + cPtr->_openssl->getErrMsg());
            cPtr->close();
            return;
        }

        // send the encrypt data from write buffer
        if (!cPtr->_sendBuffer.empty())
        {
            cPtr->sendBuffer();
        }
    }
#endif
    */
//注意epoll add必须放在最后, 否则可能导致执行完, 才调用上面语句
    _epoller.add(cPtr->getfd(), cPtr->getId(), EPOLLIN | EPOLLOUT);
}

void NetThread::addUdpConnection(Connection *cPtr) {
    uint32_t uid = _list.getUniqId();

    cPtr->init(uid);

    _list.add(cPtr, cPtr->getTimeout() + TNOW);

    _epoller.add(cPtr->getfd(), cPtr->getId(), EPOLLIN | EPOLLOUT);
}

vector<ConnStatus> NetThread::getConnStatus(int lfd) {
    return _list.getConnStatus(lfd);
}

void
NetThread::delConnection(Connection *cPtr, bool bEraseList, EM_CLOSE_T closeType) {
    //如果是TCP的连接才真正的关闭连接
    if (cPtr->getListenfd() != -1) {
        BindAdapterPtr adapter = cPtr->getBindAdapter();

        //false的情况,是超时被主动删除
        if (!bEraseList) {
            tars("timeout [" + cPtr->getIp() + ":" + TC_Common::tostr(cPtr->getPort()) + "] del from list");
        }

        uint32_t uid = cPtr->getId();

        //构造一个tagRecvData，通知业务该连接的关闭事件
        shared_ptr<RecvContext> recv = std::make_shared<RecvContext>(uid, cPtr->getIp(), cPtr->getPort(),
                                                                     cPtr->getfd(), adapter, true, (int) closeType);

        //如果是merge模式，则close直接交给网络线程处理
        if (_epollServer->isMergeHandleNetThread()) {
            cPtr->insertRecvQueue(recv);
        } else {
            cPtr->getBindAdapter()->insertRecvQueue(recv);
        }

        //从epoller删除句柄放在close之前, 否则重用socket时会有问题
        _epoller.del(cPtr->getfd(), uid, 0);

        cPtr->close();

        //对于超时检查, 由于锁的原因, 在这里不从链表中删除
        if (bEraseList) {
            _list.del(uid);
        }

        adapter->decreaseNowConnection();
    }
}

void NetThread::notify() {
    _notify.notify();
}

void NetThread::close(const shared_ptr<RecvContext> &data) {
    shared_ptr<SendContext> send = data->createCloseContext();

    _sbuffer.push_back(send);

    //	通知epoll响应, 关闭连接
    _notify.notify();
}

void NetThread::send(const shared_ptr<SendContext> &data) {
    if (_threadId == std::this_thread::get_id()) {
        //发送包线程和网络线程是同一个线程,直接发送即可
        Connection *cPtr = getConnectionPtr(data->uid());
        if (cPtr) {
            cPtr->send(data);
        }
    } else {
        //发送包线程和网络线程不是同一个线程, 需要先放队列, 再唤醒网络线程去发送
        _sbuffer.push_back(data);

        //通知epoll响应, 有数据要发送
        if (!_notifySignal) {
            _notifySignal = true;
            _notify.notify();
        }
    }
}

void NetThread::processPipe() {
    _notifySignal = false;

    while (!_sbuffer.empty()) {
        shared_ptr<SendContext> sc = _sbuffer.front();
        Connection *cPtr = getConnectionPtr(sc->uid());

        if (!cPtr) {
            _sbuffer.pop_front();
            continue;
        }
        switch (sc->cmd()) {
            case 'c': {
                if (cPtr->setClose()) {
                    delConnection(cPtr, true, EM_SERVER_CLOSE);
                }
                break;
            }
            case 's': {
                int ret = 0;
#if TARS_SSL
                if (cPtr->getBindAdapter()->getEndpoint().isSSL()) {
                    if (!cPtr->_openssl->isHandshaked()) {
                        return;
                    }
                }
                ret = cPtr->send(sc);
#else
                ret = cPtr->send(sc);
#endif
                if (ret < 0) {
                    delConnection(cPtr, true, (ret == -1) ? EM_CLIENT_CLOSE : EM_SERVER_CLOSE);
                } else {
                    _list.refresh(sc->uid(), cPtr->getTimeout() + TNOW);
                }
                break;
            }
            default:
                assert(false);
        }
        _sbuffer.pop_front();
    }
}

void NetThread::processNet(const epoll_event &ev) {
    uint32_t uid = TC_Epoller::getU32(ev, false);

    Connection *cPtr = getConnectionPtr(uid);

    if (!cPtr) {
        debug("NetThread::processNet connection[" + TC_Common::tostr(uid) + "] not exists.");
        return;
    }

    if (TC_Epoller::errorEvent(ev)) {
        delConnection(cPtr, true, EM_SERVER_CLOSE);
        return;
    }

    if (TC_Epoller::readEvent(ev)) {
        int ret = cPtr->recv();
        if (ret < 0) {
            delConnection(cPtr, true, EM_CLIENT_CLOSE);

            return;
        }
    }

    if (TC_Epoller::writeEvent(ev)) {
        int ret = cPtr->sendBuffer();
        if (ret < 0) {
            delConnection(cPtr, true, (ret == -1) ? EM_CLIENT_CLOSE : EM_SERVER_CLOSE);

            return;
        }
    }

    _list.refresh(uid, cPtr->getTimeout() + TNOW);
}

void NetThread::run() {
    _threadId = std::this_thread::get_id();

    if (_epollServer->isMergeHandleNetThread()) {

        vector<BindAdapterPtr> adapters = _epollServer->getBindAdapters();

        for (auto adapter : adapters) {
            adapter->getHandle(_threadIndex)->setNetThread(this);
            adapter->getHandle(_threadIndex)->initialize();
        }
    }

    //循环监听网路连接请求
    while (!_bTerminate) {
        _list.checkTimeout(TNOW);

        int iEvNum = _epoller.wait(1000);
        //没有网络事件
        if (iEvNum == 0) {
            //在这里加上心跳逻辑，获取所有的bindAdpator,然后发心跳
            if (_epollServer->isMergeHandleNetThread()) {
                vector<BindAdapterPtr> adapters = _epollServer->getBindAdapters();
                for (auto adapter : adapters) {
                    adapter->getHandle(_threadIndex)->heartbeat();
                }
            }
        }

        if (_bTerminate)
            break;

        for (int i = 0; i < iEvNum; ++i) {
            try {
                const epoll_event &ev = _epoller.get(i);

                uint32_t fd = TC_Epoller::getU32(ev, false);

                if (fd == (uint32_t) _notify.notifyFd()) {
                    //检查是否是通知消息
                    processPipe();
                } else {
                    processNet(ev);
                }
            }
            catch (exception &ex) {
                error("run exception:" + string(ex.what()));
            }
        }
    }
}

} // xy
