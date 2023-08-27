//
// Created by Administrator on 2023/8/18.
//

#include "xy_connection.h"
#include "xy_bind_adapter.h"
#include "xy_handle.h"
#include "xy_epoll_server.h"

namespace xy {

static const int BUFFER_SIZE = 8 * 1024;

// 服务连接
Connection::Connection(BindAdapter *pBindAdapter, int lfd, int timeout, int fd,
                                       const string &ip, uint16_t port)
        : _pBindAdapter(pBindAdapter), _uid(0), _lfd(lfd), _timeout(timeout), _ip(ip), _port(port),
          _recvBuffer(this), _sendBuffer(this), _iHeaderLen(0), _bClose(false), _enType(EM_TCP), _bEmptyConn(true),
          _pRecvBuffer(NULL), _nRecvBufferSize(DEFAULT_RECV_BUFFERSIZE), _authInit(false) {
    assert(fd != -1);

    _iLastRefreshTime = TNOW;

    _sock.init(fd, true, pBindAdapter->_ep.isIPv6() ? AF_INET6 : AF_INET);
}

Connection::Connection(BindAdapter *pBindAdapter, int fd)
        : _pBindAdapter(pBindAdapter), _uid(0), _lfd(-1), _timeout(2), _port(0), _recvBuffer(this),
          _sendBuffer(this), _iHeaderLen(0), _bClose(false), _enType(EM_UDP),
          _bEmptyConn(false) /*udp is always false*/
        , _pRecvBuffer(NULL), _nRecvBufferSize(DEFAULT_RECV_BUFFERSIZE) {
    _iLastRefreshTime = TNOW;

    _sock.init(fd, false, pBindAdapter->_ep.isIPv6() ? AF_INET6 : AF_INET);
}

Connection::~Connection() {
    if (_pRecvBuffer) {
        delete _pRecvBuffer;
        _pRecvBuffer = NULL;
    }

    if (isTcp()) {
        assert(!_sock.isValid());
    }
}

void Connection::tryInitAuthState(int initState) {
    if (!_authInit) {
        _authState = initState;
        _authInit = true;
    }
}

void Connection::close() {
#if TARS_SSL
    if (_openssl)
    {
        _openssl->release();
        _openssl.reset();
    }
#endif

    if (isTcp() && _sock.isValid()) {
        _pBindAdapter->decreaseSendBufferSize(_sendBuffer.size());

        _sock.close();
    }
}

void Connection::insertRecvQueue(const shared_ptr<RecvContext> &recv) {
    if (_pBindAdapter->getEpollServer()->isMergeHandleNetThread()) {
        int index = _pBindAdapter->getEpollServer()->getNetThreadOfFd(recv->fd())->getIndex();

        //直接在网络线程中调用handle的process
        _pBindAdapter->getHandle(index)->process(recv);
    } else {
        int iRet = _pBindAdapter->isOverloadorDiscard();

        if (iRet == 0) //未过载
        {
            _pBindAdapter->insertRecvQueue(recv);
        } else if (iRet == -1) //超过接受队列长度的一半，需要进行overload处理
        {
            recv->setOverload();

            _pBindAdapter->insertRecvQueue(recv);//, false);
        } else //接受队列满，需要丢弃
        {
            _pBindAdapter->getEpollServer()->error("[Connection::insertRecvQueue] overload discard package");
        }
    }
}

int Connection::parseProtocol(TC_NetWorkBuffer &rbuf) {
    try {
        while (!rbuf.empty()) {
            //需要过滤首包包头
            if (_iHeaderLen > 0) {
                if (rbuf.getBufferLength() >= (unsigned) _iHeaderLen) {
                    vector<char> header;
                    rbuf.getHeader(_iHeaderLen, header);
                    _pBindAdapter->getHeaderFilterFunctor()(TC_NetWorkBuffer::PACKET_FULL, header);
                    rbuf.moveHeader(_iHeaderLen);
                    _iHeaderLen = 0;
                } else {
                    vector<char> header = rbuf.getBuffers();
                    _pBindAdapter->getHeaderFilterFunctor()(TC_NetWorkBuffer::PACKET_LESS, header);
                    _iHeaderLen -= (int) rbuf.getBufferLength();
                    rbuf.clearBuffers();
                    break;
                }
            }

            vector<char> ro;

            TC_NetWorkBuffer::PACKET_TYPE b = _pBindAdapter->getProtocol()(rbuf, ro);

            if (b == TC_NetWorkBuffer::PACKET_LESS) {
                break;
            } else if (b == TC_NetWorkBuffer::PACKET_FULL) {
                shared_ptr<RecvContext> recv = std::make_shared<RecvContext>(getId(), _ip, _port, getfd(),
                                                                             _pBindAdapter);

                recv->buffer().swap(ro);

                if (_pBindAdapter->getEndpoint().isTcp() && _pBindAdapter->_authWrapper &&
                    _pBindAdapter->_authWrapper(this, recv))
                    continue;

                //收到完整的包才算
                this->_bEmptyConn = false;

                //收到完整包
                insertRecvQueue(recv);
            } else {
                _pBindAdapter->getEpollServer()->error(
                        "recv [" + _ip + ":" + TC_Common::tostr(_port) + "], packet parse error.");
                return -1; //协议解析错误
            }
        }
    }
    catch (exception &ex) {
        _pBindAdapter->getEpollServer()->error("recv protocol error:" + string(ex.what()));
        return -1;
    }
    catch (...) {
        _pBindAdapter->getEpollServer()->error("recv protocol error");
        return -1;
    }

    return 0;
}

int Connection::recvTcp() {
    int recvCount = 0;

    TC_NetWorkBuffer *rbuf = &_recvBuffer;

    while (true) {
        char buffer[BUFFER_SIZE] = {0x00};

        int iBytesReceived = _sock.recv((void *) buffer, BUFFER_SIZE);

        if (iBytesReceived < 0) {
            if (TC_Socket::isPending()) {
                //没有数据了
                break;
            } else {
                //客户端主动关闭
                _pBindAdapter->getEpollServer()->debug(
                        "recv [" + _ip + ":" + TC_Common::tostr(_port) + "] close connection");
                return -1;
            }
        } else if (iBytesReceived == 0) {
            //客户端主动关闭
            _pBindAdapter->getEpollServer()->debug(
                    "recv [" + _ip + ":" + TC_Common::tostr(_port) + "] close connection");
            return -1;
        } else {

#if TARS_SSL
            if (_pBindAdapter->getEndpoint().isSSL())
            {
                int ret = _openssl->read(buffer, iBytesReceived, _sendBuffer);
                if (ret != 0)
                {
                    _pBindAdapter->getEpollServer()->error("[SSL_read failed: " + _openssl->getErrMsg());
                    return -1;
                }
                else
                {
                    if (!_sendBuffer.empty())
                    {
                        sendBuffer();
                    }

                    rbuf = _openssl->recvBuffer();
                }
            }
            else
            {
                rbuf->addBuffer(buffer, iBytesReceived);
            }

#else
            rbuf->addBuffer(buffer, iBytesReceived);
#endif

            //字符串太长时, 强制解析协议
            if (rbuf->getBufferLength() > 8192) {
                parseProtocol(*rbuf);
            }

            //接收到数据不超过buffer,没有数据了(如果有数据,内核会再通知你)
            if ((size_t) iBytesReceived < BUFFER_SIZE) {
                break;
            }

            if (++recvCount > 100) {
                //太多数据要接收,避免网络线程饥饿
                _pBindAdapter->getNetThreadOfFd(_sock.getfd())->getEpoller()->mod(_sock.getfd(), getId(),
                                                                                  EPOLLIN | EPOLLOUT);
                break;
            }
        }
    }

    return parseProtocol(*rbuf);
}

int Connection::recvUdp() {
    assert(_pRecvBuffer != NULL);

    int recvCount = 0;
    while (true) {
        int iBytesReceived = _sock.recvfrom((void *) _pRecvBuffer, _nRecvBufferSize, _ip, _port, 0);

        if (iBytesReceived < 0) {
            if (TC_Socket::isPending())//errno == EAGAIN)
            {
                //没有数据了
                break;
            } else {
                //客户端主动关闭
                _pBindAdapter->getEpollServer()->debug(
                        "recv [" + _ip + ":" + TC_Common::tostr(_port) + "] close connection");
                return -1;
            }
        } else if (iBytesReceived == 0) {
            //客户端主动关闭
            _pBindAdapter->getEpollServer()->debug(
                    "recv [" + _ip + ":" + TC_Common::tostr(_port) + "] close connection");
            return -1;
        } else {

            if (_pBindAdapter->isIpAllow(_ip) == true) {
                //保存接收到数据
                _recvBuffer.addBuffer(_pRecvBuffer, iBytesReceived);

                parseProtocol(_recvBuffer);
            } else {
                //udp ip无权限
                _pBindAdapter->getEpollServer()->debug(
                        "accept [" + _ip + ":" + TC_Common::tostr(_port) + "] [" + TC_Common::tostr(_lfd) +
                        "] not allowed");
            }
            _recvBuffer.clearBuffers();

            if (++recvCount > 100) {
                //太多数据要接收,避免网络线程饥饿
                _pBindAdapter->getNetThreadOfFd(_sock.getfd())->getEpoller()->mod(_sock.getfd(), getId(),
                                                                                  EPOLLIN | EPOLLOUT);
                break;
            }
        }
    }

    return 0;
}

int Connection::recv() {
    return isTcp() ? recvTcp() : recvUdp();
}

int Connection::sendBuffer() {
    if (!isTcp()) {
        return 0;
    }

    size_t nowSendBufferSize = 0;
    size_t nowLeftBufferSize = _sendBuffer.getBufferLength();

    while (!_sendBuffer.empty()) {
        pair<const char *, size_t> data = _sendBuffer.getBufferPointer();

        int iBytesSent = _sock.send((const void *) data.first, data.second);
        // }
        // else
        // {
        // 	iBytesSent = _sock.sendto((const void *) data.first, data.second, _ip, _port, 0);
        // }

        if (iBytesSent < 0) {
            if (TC_Socket::isPending()) {
#if TARGET_PLATFORM_WINDOWS
                _pBindAdapter->getNetThreadOfFd(_sock.getfd())->getEpoller()->mod(_sock.getfd(), getId(), EPOLLIN|EPOLLOUT);
#endif
                break;
            } else {
                _pBindAdapter->getEpollServer()->debug(
                        "send [" + _ip + ":" + TC_Common::tostr(_port) + "] close connection by peer.");
                return -1;
            }
        }

        if (iBytesSent > 0) {
            nowSendBufferSize += iBytesSent;

            if (isTcp()) {
                _sendBuffer.moveHeader(iBytesSent);

                if (iBytesSent == (int) data.second) {
                    _pBindAdapter->decreaseSendBufferSize();
                }
            } else {
                _sendBuffer.moveHeader(data.second);

                _pBindAdapter->decreaseSendBufferSize();

            }
        }

        //发送的数据小于需要发送的,break, 内核会再通知你的
        if (iBytesSent < (int) data.second) {
            break;
        }
    }

    //需要关闭链接
    if (_bClose && _sendBuffer.empty()) {
        _pBindAdapter->getEpollServer()->debug(
                "send [" + _ip + ":" + TC_Common::tostr(_port) + "] close connection by user.");
        return -2;
    }

    //	当出现队列积压的前提下, 且积压超过一定大小
    //	每5秒检查一下积压情况, 连续12次(一分钟), 都是积压
    //	且每个检查点, 积压长度都增加或者连续3次发送buffer字节小于1k, 就关闭连接, 主要避免极端情况

    size_t iBackPacketBuffLimit = _pBindAdapter->getBackPacketBuffLimit();
    if (_sendBuffer.getBufferLength() > iBackPacketBuffLimit) {
        if (_sendBufferSize == 0) {
            //开始积压
            _lastCheckTime = TNOW;
        }
        _sendBufferSize += nowSendBufferSize;

        if (TNOW - _lastCheckTime >= 5) {
            //如果持续有积压, 则每5秒检查一次
            _lastCheckTime = TNOW;

            _checkSend.push_back(make_pair(_sendBufferSize, nowLeftBufferSize));

            _sendBufferSize = 0;

            size_t iBackPacketBuffMin = _pBindAdapter->getBackPacketBuffMin();

            //连续3个5秒, 发送速度都极慢, 每5秒发送 < iBackPacketBuffMin, 认为连接有问题, 关闭之
            int left = 3;
            if ((int) _checkSend.size() >= left) {
                bool slow = true;
                for (int i = (int) _checkSend.size() - 1; i >= (int) (_checkSend.size() - left); i--) {
                    //发送速度
                    if (_checkSend[i].first > iBackPacketBuffMin) {
                        slow = false;
                        continue;
                    }
                }

                if (slow) {
                    ostringstream os;
                    os << "send [" << _ip << ":" << _port << "] buffer queue send to slow, send size:";

                    for (int i = (int) _checkSend.size() - 1; i >= (int) (_checkSend.size() - left); i--) {
                        os << ", " << _checkSend[i].first;
                    }

                    _pBindAdapter->getEpollServer()->error(os.str());
                    _sendBuffer.clearBuffers();
                    return -5;
                }
            }

            //连续12个5秒, 都有积压现象, 检查
            if (_checkSend.size() >= 12) {
                bool accumulate = true;
                for (size_t i = _checkSend.size() - 1; i >= 1; i--) {
                    //发送buffer 持续增加
                    if (_checkSend[i].second < _checkSend[i - 1].second) {
                        accumulate = false;
                        break;
                    }
                }

                //持续积压
                if (accumulate) {
                    ostringstream os;
                    os << "send [" << _ip << ":" << _port
                       << "] buffer queue continues to accumulate data, queue size:";

                    for (size_t i = 0; i < _checkSend.size(); i++) {
                        os << ", " << _checkSend[i].second;
                    }

                    _pBindAdapter->getEpollServer()->error(os.str());
                    _sendBuffer.clearBuffers();
                    return -4;
                }

                _checkSend.erase(_checkSend.begin());
            }
        }
    } else {
        //无积压
        _sendBufferSize = 0;
        _lastCheckTime = TNOW;
        _checkSend.clear();
    }

    return 0;

}


int Connection::sendBufferDirect(const std::string &buff) {
    _pBindAdapter->increaseSendBufferSize();

    if (getBindAdapter()->getEndpoint().isTcp()) {
#if TAF_SSL
        if (getBindAdapter()->getEndpoint().isSSL())
        {
            //assert(_openssl->isHandshaked());
        
            int ret = _openssl->write(buff.c_str(), buff.length(), _sendBuffer);
            if (ret != 0)
            {
                _pBindAdapter->getEpollServer()->error("[Connection] send direct error! " + TC_Common::tostr(ret));
                return -1; // should not happen
            }

        }
        else
#endif
        {
            _sendBuffer.addBuffer(buff);
        }

        return sendBuffer();
    } else {
        _pBindAdapter->getEpollServer()->error("[Connection] send direct not support udp! ");
        return -2;
    }
}

int Connection::sendBufferDirect(const std::vector<char> &buff) {
    _pBindAdapter->increaseSendBufferSize();

    if (getBindAdapter()->getEndpoint().isTcp()) {
#if TAF_SSL
        if (getBindAdapter()->getEndpoint().isSSL())
        {
            //assert(_openssl->isHandshaked());
        
            int ret = _openssl->write(&buff[0], buff.size(), _sendBuffer);
            if (ret != 0)
            {
                _pBindAdapter->getEpollServer()->error("[Connection] send direct error! " + TC_Common::tostr(ret));
                return -1; // should not happen
            }

        }
        else
#endif
        {
            _sendBuffer.addBuffer(buff);
        }

        return sendBuffer();
    } else {
        _pBindAdapter->getEpollServer()->error("[Connection] send direct not support udp! ");
        return -2;
    }
}

int Connection::send(const shared_ptr<SendContext> &sc) {
    assert(sc);

    _pBindAdapter->increaseSendBufferSize();

    if (getBindAdapter()->getEndpoint().isTcp()) {
#if TARS_SSL
        if (getBindAdapter()->getEndpoint().isSSL())
        {
            assert(_openssl->isHandshaked());

            int ret = _openssl->write(sc->buffer()->buffer(), sc->buffer()->length(), _sendBuffer);
            if (ret != 0) 
            {
                _pBindAdapter->getEpollServer()->error("[Connection] send [" + _ip + ":" + TC_Common::tostr(_port) + "] error:" + _openssl->getErrMsg());

                return -1; // should not happen
            }
        }
        else
#endif
        {
            _sendBuffer.addBuffer(sc->buffer());
        }

        return sendBuffer();
    } else {
        //注意udp, 回包时需要带上请求包的ip, port的
        int iRet = _sock.sendto((const void *) sc->buffer()->buffer(), sc->buffer()->length(), sc->ip(), sc->port(),
                                0);
        if (iRet < 0) {
            _pBindAdapter->getEpollServer()->error(
                    "[Connection] send udp [" + _ip + ":" + TC_Common::tostr(_port) + "] error");
            return -1;
        }
    }
    return 0;
}

bool Connection::setRecvBuffer(size_t nSize) {
    //only udp type needs to malloc
    if (!isTcp() && !_pRecvBuffer) {
        _nRecvBufferSize = nSize;

        _pRecvBuffer = new char[_nRecvBufferSize];
        if (!_pRecvBuffer) {
            throw TC_Exception("adapter '" + _pBindAdapter->getName() + "' malloc udp receive buffer fail");
        }
    }
    return true;
}

bool Connection::setClose() {
    _bClose = true;
    if (_sendBuffer.empty())
        return true;
    else
        return false;
}

////////////////////////////////////////////////////////////////
//
ConnectionList::ConnectionList(NetThread *pEpollServer)
        : _pEpollServer(pEpollServer), _total(0), _free_size(0), _vConn(NULL), _lastTimeoutTime(0),
          _iConnectionMagic(0) {
}

void ConnectionList::init(uint32_t size, uint32_t iIndex) {
    _lastTimeoutTime = TNOW;

    _total = size;

    _free_size = 0;

    //初始化链接链表
    if (_vConn) delete[] _vConn;

    //分配total+1个空间(多分配一个空间, 第一个空间其实无效)
    _vConn = new list_data[_total + 1];

    _iConnectionMagic = ((((uint32_t) _lastTimeoutTime) << 26) & (0xFFFFFFFF << 26)) +
                        ((iIndex << 22) & (0xFFFFFFFF << 22));//((uint32_t)_lastTimeoutTime) << 20;

    //free从1开始分配, 这个值为uid, 0保留为管道用, epollwait根据0判断是否是管道消息
    for (uint32_t i = 1; i <= _total; i++) {
        _vConn[i].first = NULL;

        _free.push_back(i);

        ++_free_size;
    }
}

uint32_t ConnectionList::getUniqId() {
    TC_LockT<TC_ThreadMutex> lock(_mutex);

    uint32_t uid = _free.front();

    assert(uid > 0 && uid <= _total);

    _free.pop_front();

    --_free_size;

    return _iConnectionMagic | uid;
}

Connection *ConnectionList::get(uint32_t uid) {
    uint32_t magi = uid & (0xFFFFFFFF << 22);
    uid = uid & (0x7FFFFFFF >> 9);

    if (magi != _iConnectionMagic) return NULL;

    return _vConn[uid].first;
}

void ConnectionList::add(Connection *cPtr, time_t iTimeOutStamp) {
    TC_LockT<TC_ThreadMutex> lock(_mutex);

    uint32_t muid = cPtr->getId();
    uint32_t magi = muid & (0xFFFFFFFF << 22);
    uint32_t uid = muid & (0x7FFFFFFF >> 9);

    assert(magi == _iConnectionMagic && uid > 0 && uid <= _total && !_vConn[uid].first);

    _vConn[uid] = make_pair(cPtr, _tl.insert(make_pair(iTimeOutStamp, uid)));
}

void ConnectionList::refresh(uint32_t uid, time_t iTimeOutStamp) {
    TC_LockT<TC_ThreadMutex> lock(_mutex);

    uint32_t magi = uid & (0xFFFFFFFF << 22);
    uid = uid & (0x7FFFFFFF >> 9);

    assert(magi == _iConnectionMagic && uid > 0 && uid <= _total && _vConn[uid].first);

    if (iTimeOutStamp - _vConn[uid].first->_iLastRefreshTime < 1) {
        return;
    }

    _vConn[uid].first->_iLastRefreshTime = iTimeOutStamp;

    //删除超时链表
    _tl.erase(_vConn[uid].second);

    _vConn[uid].second = _tl.insert(make_pair(iTimeOutStamp, uid));
}

void ConnectionList::checkTimeout(time_t iCurTime) {
    //至少1s才能检查一次
    if (iCurTime - _lastTimeoutTime < 1) {
        return;
    }

    _lastTimeoutTime = iCurTime;

    TC_LockT<TC_ThreadMutex> lock(_mutex);

    multimap<time_t, uint32_t>::iterator it = _tl.begin();

    while (it != _tl.end()) {
        //已经检查到当前时间点了, 后续不用在检查了
        if (it->first > iCurTime) {
            break;
        }

        uint32_t uid = it->second;

        ++it;

        //udp的监听端口, 不做处理
        if (_vConn[uid].first->getListenfd() == -1) {
            continue;
        }

        //超时关闭
        _pEpollServer->delConnection(_vConn[uid].first, false, EM_SERVER_TIMEOUT_CLOSE);

        //从链表中删除
        _del(uid);
    }

    if (_pEpollServer->isEmptyConnCheck()) {
        it = _tl.begin();
        while (it != _tl.end()) {
            uint32_t uid = it->second;

            //遍历所有的空连接
            if (_vConn[uid].first->isEmptyConn()) {
                //获取空连接的超时时间点
                time_t iEmptyTimeout = (it->first - _vConn[uid].first->getTimeout()) +
                                       (_pEpollServer->getEmptyConnTimeout() / 1000);

                //已经检查到当前时间点了, 后续不用在检查了
                if (iEmptyTimeout > iCurTime) {
                    break;
                }

                //udp的监听端口, 不做处理
                if (_vConn[uid].first->getListenfd() == -1) {
                    ++it;
                    continue;
                }

                //超时关闭
                _pEpollServer->delConnection(_vConn[uid].first, false, EM_SERVER_TIMEOUT_CLOSE);

                //从链表中删除
                _del(uid);
            }

            ++it;
        }
    }
}

vector<ConnStatus> ConnectionList::getConnStatus(int lfd) {
    vector<ConnStatus> v;

    TC_LockT<TC_ThreadMutex> lock(_mutex);

    for (size_t i = 1; i <= _total; i++) {
        //是当前监听端口的连接
        if (_vConn[i].first != NULL && _vConn[i].first->getListenfd() == lfd) {
            ConnStatus cs;

            cs.iLastRefreshTime = _vConn[i].first->_iLastRefreshTime;
            cs.ip = _vConn[i].first->getIp();
            cs.port = _vConn[i].first->getPort();
            cs.timeout = _vConn[i].first->getTimeout();
            cs.uid = _vConn[i].first->getId();
            cs.recvBufferSize = _vConn[i].first->getRecvBuffer().getBufferLength();
            cs.sendBufferSize = _vConn[i].first->getSendBuffer().getBufferLength();

            v.push_back(cs);
        }
    }

    return v;
}

void ConnectionList::del(uint32_t uid) {
    TC_LockT<TC_ThreadMutex> lock(_mutex);

    uint32_t magi = uid & (0xFFFFFFFF << 22);
    uid = uid & (0x7FFFFFFF >> 9);

    assert(magi == _iConnectionMagic && uid > 0 && uid <= _total && _vConn[uid].first);

    _del(uid);
}

void ConnectionList::_del(uint32_t uid) {
    assert(uid > 0 && uid <= _total && _vConn[uid].first);

    _tl.erase(_vConn[uid].second);

    delete _vConn[uid].first;

    _vConn[uid].first = NULL;

    _free.push_back(uid);

    ++_free_size;
}

size_t ConnectionList::size() {
    TC_LockT<TC_ThreadMutex> lock(_mutex);

    return _total - _free_size;
}


} // xy

