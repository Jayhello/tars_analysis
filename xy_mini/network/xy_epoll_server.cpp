//
// Created by Administrator on 2023/8/22.
//

#include "xy_epoll_server.h"

namespace xy {

TC_EpollServer::TC_EpollServer(unsigned int iNetThreadNum)
        : _netThreadNum(iNetThreadNum), _bTerminate(false), _handleStarted(false), _pLocalLogger(NULL),
          _acceptFunc(NULL) {
#if TARGET_PLATFORM_WINDOWS
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);
#endif

    if (_netThreadNum < 1) {
        _netThreadNum = 1;
    }

    //网络线程的配置数目不能15个
    if (_netThreadNum > 15) {
        _netThreadNum = 15;
    }

    //创建epoll
    _epoller.create(10240);

    _notify.init(&_epoller);
    _notify.add(_notify.notifyFd());

    for (size_t i = 0; i < _netThreadNum; ++i) {
        TC_EpollServer::NetThread *netThreads = new TC_EpollServer::NetThread(this, i);
        _netThreads.push_back(netThreads);
    }
}

TC_EpollServer::~TC_EpollServer() {
    terminate();

    for (size_t i = 0; i < _netThreadNum; ++i) {
        delete _netThreads[i];
    }

    _netThreads.clear();

    _netThreadNum = 0;

    auto it = _listeners.begin();

    while (it != _listeners.end()) {
        TC_Port::closeSocket(it->first);
        ++it;
    }
    _listeners.clear();

#if TARGET_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

void TC_EpollServer::applicationCallback(TC_EpollServer *epollServer) {
}

bool TC_EpollServer::accept(int fd, int domain) {
    struct sockaddr_in stSockAddr4;
    struct ::sockaddr_in6 stSockAddr6;

    socklen_t iSockAddrSize = (AF_INET6 == domain) ? sizeof(::sockaddr_in6) : sizeof(sockaddr_in);
    struct sockaddr *stSockAddr = (AF_INET6 == domain) ? (struct sockaddr *) &stSockAddr6
                                                       : (struct sockaddr *) &stSockAddr4;

    TC_Socket cs;

    cs.setOwner(false);

    //接收连接
    TC_Socket s;

    s.init(fd, false, domain);
    int iRetCode = s.accept(cs, (struct sockaddr *) stSockAddr, iSockAddrSize);
    if (iRetCode > 0) {
        string ip;
        uint16_t port;

        char sAddr[INET6_ADDRSTRLEN] = "\0";

        inet_ntop(domain, (AF_INET6 == domain) ? (void *) &stSockAddr6.sin6_addr : (void *) &stSockAddr4.sin_addr,
                  sAddr, sizeof(sAddr));
        port = (AF_INET6 == domain) ? ntohs(stSockAddr6.sin6_port) : ntohs(stSockAddr4.sin_port);
        ip = sAddr;

        debug("accept [" + ip + ":" + TC_Common::tostr(port) + "] [" + TC_Common::tostr(cs.getfd()) +
              "] incomming");

        if (!_listeners[fd]->isIpAllow(ip)) {
            debug("accept [" + ip + ":" + TC_Common::tostr(port) + "] [" + TC_Common::tostr(cs.getfd()) +
                  "] not allowed");

            cs.close();

            return true;
        }

        if (_listeners[fd]->isLimitMaxConnection()) {
            error("accept [" + ip + ":" + TC_Common::tostr(port) + "][" + TC_Common::tostr(cs.getfd()) +
                  "] beyond max connection:" + TC_Common::tostr(_listeners[fd]->getMaxConns()));
            cs.close();

            return true;
        }

        cs.setblock(false);
        cs.setKeepAlive();
        cs.setTcpNoDelay();
        cs.setCloseWaitDefault();

        int timeout = _listeners[fd]->getEndpoint().getTimeout() / 1000;

        Connection *cPtr = new Connection(_listeners[fd].get(), fd, (timeout < 2 ? 2 : timeout), cs.getfd(), ip,
                                          port);

        //过滤连接首个数据包包头
        cPtr->setHeaderFilterLen((int) _listeners[fd]->getHeaderFilterLen());

        addConnection(cPtr, cs.getfd(), TCP_CONNECTION);

        return true;
    } else {
        // //直到发生EAGAIN才不继续accept
        // if (TC_Socket::isPending())
        // {
        //     return false;
        // }

        return false;
    }
    return true;
}

void TC_EpollServer::waitForShutdown() {

    if (!isMergeHandleNetThread())
        startHandle();

    createEpoll();

    for (size_t i = 0; i < _netThreadNum; ++i) {
        _netThreads[i]->start();
    }

    int64_t iLastCheckTime = TNOWMS;

    while (!_bTerminate) {
        int iEvNum = _epoller.wait(300);

        if (_bTerminate)
            break;

        if (TNOWMS - iLastCheckTime > 1000) {
            try { _hf(this); } catch (...) {}
            iLastCheckTime = TNOWMS;
        }

        for (int i = 0; i < iEvNum; ++i) {
            try {
                const epoll_event &ev = _epoller.get(i);

                uint32_t fd = TC_Epoller::getU32(ev, false);

                auto it = _listeners.find(fd);

                if (it != _listeners.end()) {
                    //manualListen 会进入这个场景
                    if (TC_Epoller::writeEvent(ev)) {
                        TC_Socket s;
                        s.init(fd, false);
                        s.listen(1024);

                        debug("run listen fd: " + TC_Common::tostr(fd));
                    }

                    //监听端口有请求
                    if (TC_Epoller::readEvent(ev)) {
#if TARGET_PLATFORM_LINUX || TARGET_PLATFORM_IOS

                        bool ret;
                        do {

                            ret = accept(fd, it->second->_ep.isIPv6() ? AF_INET6 : AF_INET);
                        } while (ret);
#else

                        accept(fd, it->second->_ep.isIPv6() ? AF_INET6 : AF_INET);
#endif
                    }
                }
            }
            catch (exception &ex) {
                error("run exception:" + string(ex.what()));
            }
            catch (...) {
                error("TC_EpollServer::waitForShutdown unknown error");
            }
        }
    }

    for (size_t i = 0; i < _netThreads.size(); ++i) {
        if (_netThreads[i]->isAlive()) {
            _netThreads[i]->terminate();

            _netThreads[i]->getThreadControl().join();
        }
    }

}

void TC_EpollServer::terminate() {
    if (!_bTerminate) {
        _bTerminate = true;

        //停掉处理线程
        stopThread();

        //停掉主线程(waitForShutdown)
        _notify.notify();
    }
}

void TC_EpollServer::enAntiEmptyConnAttack(bool bEnable) {
    for (size_t i = 0; i < _netThreads.size(); ++i) {
        _netThreads[i]->enAntiEmptyConnAttack(bEnable);
    }
}

void TC_EpollServer::setEmptyConnTimeout(int timeout) {
    for (size_t i = 0; i < _netThreads.size(); ++i) {
        _netThreads[i]->setEmptyConnTimeout(timeout);
    }
}

void TC_EpollServer::bind(const TC_Endpoint &ep, TC_Socket &s, bool manualListen) {
#if TARGET_PLATFORM_WINDOWS
    int type = ep.isIPv6() ? AF_INET6 : AF_INET;
#else
    int type = ep.isUnixLocal() ? AF_LOCAL : ep.isIPv6() ? AF_INET6 : AF_INET;
#endif

#if TARGET_PLATFORM_LINUX
    if (ep.isTcp()) {
        s.createSocket(SOCK_STREAM | SOCK_CLOEXEC, type);
    } else {
        s.createSocket(SOCK_DGRAM | SOCK_CLOEXEC, type);
    }
#else
    if (ep.isTcp())
    {
        s.createSocket(SOCK_STREAM, type);
    }
    else
    {
        s.createSocket(SOCK_DGRAM, type);
    }
#endif

#if TARGET_PLATFORM_WINDOWS
    s.bind(ep.getHost(), ep.getPort());
    if (ep.isTcp())
#else
    if (ep.isUnixLocal()) {
        s.bind(ep.getHost().c_str());
    } else {
        s.bind(ep.getHost(), ep.getPort());
    }
    if (ep.isTcp() && !ep.isUnixLocal())

#endif
    {
        if (!manualListen) {
            //手工监听
            s.listen(10240);
        }
        s.setKeepAlive();
        s.setTcpNoDelay();

        //不要设置close wait否则http服务回包主动关闭连接会有问题
        s.setNoCloseWait();
    }

    s.setblock(false);
}

int TC_EpollServer::bind(BindAdapterPtr &lsPtr) {
    auto it = _listeners.begin();

    while (it != _listeners.end()) {
        if (it->second->getName() == lsPtr->getName()) {
            throw TC_Exception("bind name '" + lsPtr->getName() + "' conflicts.");
        }
        ++it;
    }

    const TC_Endpoint &ep = lsPtr->getEndpoint();

    TC_Socket &s = lsPtr->getSocket();

    bind(ep, s, lsPtr->isManualListen());

    _listeners[s.getfd()] = lsPtr;

    _bindAdapters.push_back(lsPtr);

    return s.getfd();
}

void TC_EpollServer::addConnection(TC_EpollServer::Connection *cPtr, int fd, TC_EpollServer::CONN_TYPE iType) {
    TC_EpollServer::NetThread *netThread = getNetThreadOfFd(fd);

    if (iType == TCP_CONNECTION) {
        netThread->addTcpConnection(cPtr);
    } else {
        netThread->addUdpConnection(cPtr);
    }
    // 回调
    if (_acceptFunc != NULL) {
        _acceptFunc(cPtr);
    }
}

void TC_EpollServer::startHandle() {
    if (!this->isMergeHandleNetThread()) {
        if (!_handleStarted) {
            _handleStarted = true;

            for (auto &bindAdapter : _bindAdapters) {
                const vector<TC_EpollServer::HandlePtr> &hds = bindAdapter->getHandles();

                for (uint32_t i = 0; i < hds.size(); ++i) {
                    if (!hds[i]->isAlive()) {
                        hds[i]->start();
                    }
                }
            }
        }
    }
}

void TC_EpollServer::stopThread() {
    if (!this->isMergeHandleNetThread()) {
        for (auto &bindAdapter : _bindAdapters) {
            const vector<TC_EpollServer::HandlePtr> &hds = bindAdapter->getHandles();

            //把处理线程都唤醒
            if (!bindAdapter->isQueueMode()) {
                bindAdapter->notifyHandle(0);
            } else {
                for (uint32_t i = 0; i < hds.size(); i++) {
                    bindAdapter->notifyHandle(i);
                }
            }

            for (uint32_t i = 0; i < hds.size(); ++i) {
                if (hds[i]->isAlive()) {
                    hds[i]->getThreadControl().join();
                }
            }
        }
    }
}

void TC_EpollServer::createEpoll() {

    uint32_t maxAllConn = 0;

    //监听socket
    auto it = _listeners.begin();

    while (it != _listeners.end()) {
        if (it->second->getEndpoint().isTcp()) {
            //获取最大连接数
            maxAllConn += it->second->getMaxConns();

            _epoller.add(it->first, it->first, EPOLLIN);
        } else {
            maxAllConn++;
        }

        ++it;
    }

    if (maxAllConn >= (1 << 22)) {
        error("createEpoll connection num: " + TC_Common::tostr(maxAllConn) + " >= " + TC_Common::tostr(1 << 22));
        maxAllConn = (1 << 22) - 1;
    }

    for (size_t i = 0; i < _netThreads.size(); ++i) {
        _netThreads[i]->createEpoll(maxAllConn);
    }

    //必须先等所有网络线程调用createEpoll()，初始化list后，才能调用initUdp()
    for (size_t i = 0; i < _netThreads.size(); ++i) {
        _netThreads[i]->initUdp(_listeners);
    }
}

TC_EpollServer::BindAdapterPtr TC_EpollServer::getBindAdapter(const string &sName) {
    auto it = _listeners.begin();

    while (it != _listeners.end()) {
        if (it->second->getName() == sName) {
            return it->second;
        }

        ++it;
    }
    return NULL;
}

vector<TC_EpollServer::BindAdapterPtr> TC_EpollServer::getBindAdapters() {
    return this->_bindAdapters;
}

void TC_EpollServer::close(const shared_ptr<TC_EpollServer::RecvContext> &data) {
    TC_EpollServer::NetThread *netThread = getNetThreadOfFd(data->fd());

    netThread->close(data);
}

void TC_EpollServer::send(const shared_ptr<SendContext> &data) {
    TC_EpollServer::NetThread *netThread = getNetThreadOfFd(data->fd());

    netThread->send(data);
}

void TC_EpollServer::debug(const string &s) const {
    if (_pLocalLogger) {
        _pLocalLogger->debug() << "[TARS]" << s << endl;
    }
}

void TC_EpollServer::info(const string &s) const {
    if (_pLocalLogger) {
        _pLocalLogger->info() << "[TARS]" << s << endl;
    }
}

void TC_EpollServer::tars(const string &s) const {
    if (_pLocalLogger) {
        _pLocalLogger->tars() << "[TARS]" << s << endl;
    }
}

void TC_EpollServer::error(const string &s) const {
    if (_pLocalLogger) {
        _pLocalLogger->error() << "[TARS]" << s << endl;
    }
}

vector<TC_EpollServer::ConnStatus> TC_EpollServer::getConnStatus(int lfd) {
    vector<TC_EpollServer::ConnStatus> vConnStatus;
    for (size_t i = 0; i < _netThreads.size(); ++i) {
        vector<TC_EpollServer::ConnStatus> tmp = _netThreads[i]->getConnStatus(lfd);
        for (size_t k = 0; k < tmp.size(); ++k) {
            vConnStatus.push_back(tmp[k]);
        }
    }
    return vConnStatus;
}

unordered_map<int, BindAdapterPtr> TC_EpollServer::getListenSocketInfo() {
    return _listeners;
}

size_t TC_EpollServer::getConnectionCount() {
    size_t iConnTotal = 0;
    for (size_t i = 0; i < _netThreads.size(); ++i) {
        iConnTotal += _netThreads[i]->getConnectionCount();
    }
    return iConnTotal;
}

size_t TC_EpollServer::getLogicThreadNum() {
    if (this->isMergeHandleNetThread()) {
        return this->_netThreadNum;
    } else {

        size_t iNum = 0;

        for (auto &bindAdapter : _bindAdapters) {
            iNum += bindAdapter->getHandles().size();
        }
        return iNum;
    }
}

} // xy
