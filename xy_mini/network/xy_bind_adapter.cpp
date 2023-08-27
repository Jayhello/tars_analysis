//
// Created by Administrator on 2023/8/18.
//

#include "xy_bind_adapter.h"
#include "xy_epoll_server.h"

namespace xy{

uint32_t SendContext::uid() const { return _context->uid(); }

int SendContext::fd() const { return _context->fd(); }

const string& SendContext::ip() const { return _context->ip(); }

uint16_t SendContext::port() const { return _context->port(); }

BindAdapter::BindAdapter(TC_EpollServer *pEpollServer)
:
// _pReportQueue(NULL)
//, _pReportConRate(NULL)
//, _pReportTimeoutNum(NULL)
_pEpollServer(pEpollServer)
,  _pf(echo_protocol)
, _hf(echo_header_filter)
, _name("")
, _iMaxConns(DEFAULT_MAX_CONN)
, _iCurConns(0)
, _iHandleNum(0)
, _eOrder(ALLOW_DENY)
, _iQueueCapacity(DEFAULT_QUEUE_CAP)
, _iQueueTimeout(DEFAULT_QUEUE_TIMEOUT)
, _iHeaderLen(0)
, _iHeartBeatTime(0)
, _protocolName("tars")
{
}

BindAdapter::~BindAdapter()
{
}

void BindAdapter::setProtocolName(const string &name)
{
    std::lock_guard<std::mutex> lock (_mutex);

    _protocolName = name;
}

const string &BindAdapter::getProtocolName()
{
    std::lock_guard<std::mutex> lock (_mutex);

    return _protocolName;
}

bool BindAdapter::isTarsProtocol()
{
    return (_protocolName == "tars");
}

bool BindAdapter::isIpAllow(const string &ip) const
{
    std::lock_guard<std::mutex> lock (_mutex);

    if (_eOrder == ALLOW_DENY)
    {
        if (TC_Common::matchPeriod(ip, _vtAllow))
        {
            return true;
        }
        if (TC_Common::matchPeriod(ip, _vtDeny))
        {
            return false;
        }
    }
    else
    {
        if (TC_Common::matchPeriod(ip, _vtDeny))
        {
            return false;
        }
        if (TC_Common::matchPeriod(ip, _vtAllow))
        {
            return true;
        }
    }
    return _vtAllow.size() == 0;
}

void BindAdapter::manualListen()
{
    this->getEpollServer()->_epoller.mod(getSocket().getfd(), getSocket().getfd(), EPOLLIN|EPOLLOUT);
}

void BindAdapter::initThreadRecvQueue(uint32_t handeIndex)
{
    if(isQueueMode()) {
        _threadDataQueue[handeIndex + 1] = std::make_shared<BindAdapter::DataQueue>();
    }
}

recv_queue &BindAdapter::getRecvQueue(uint32_t handleIndex)
{
    if(isQueueMode())
    {
        return _threadDataQueue[handleIndex+1]->_rbuffer;
    }

    return _threadDataQueue[0]->_rbuffer;
}

TC_ThreadLock &BindAdapter::getLock(uint32_t handleIndex)
{
    if(isQueueMode())
    {
        return _threadDataQueue[handleIndex+1]->_monitor;
    }

    return _threadDataQueue[0]->_monitor;
}

void BindAdapter::waitAtQueue(uint32_t handleIndex, uint32_t waitTime)
{
    TC_ThreadLock &l = getLock(handleIndex);

    TC_ThreadLock::Lock lock(l);

    l.timedWait(waitTime);
}

void BindAdapter::notifyHandle(uint32_t handleIndex)
{
    if(this->getEpollServer()->isMergeHandleNetThread())
    {
        //业务线程和网络线程合并了, 直接通知网络线程醒过来即可
        NetThread *netThread = getHandle(handleIndex)->getNetThread();

        assert(netThread != NULL);

        netThread->notify();
    }
    else {
        //通知对应的Handle线程队列醒过来
        if (isQueueMode()) {
            handleIndex = handleIndex + 1;
            //相同连接过来的进入同一个buffer, 被Handle的同一个线程处理
            TC_ThreadLock::Lock lock(_threadDataQueue[handleIndex]->_monitor);
            _threadDataQueue[handleIndex]->_monitor.notify();
        }
        else {
            TC_ThreadLock::Lock lock(_threadDataQueue[0]->_monitor);
            _threadDataQueue[0]->_monitor.notifyAll();
        }
    }
}

void BindAdapter::insertRecvQueue(const shared_ptr<RecvContext> &recv)
{
    _iRecvBufferSize++;

    size_t idx = 0;

    if(isQueueMode()) {
        //相同连接过来的进入同一个buffer, 被Handle的同一个线程处理
        idx = recv->fd() % _iHandleNum + 1;
    }
    _threadDataQueue[idx]->_rbuffer.push_back(recv);

    //通知对应的线程队列醒过来
    TC_ThreadLock::Lock lock(_threadDataQueue[idx]->_monitor);
    _threadDataQueue[idx]->_monitor.notify();

}

bool BindAdapter::waitForRecvQueue(uint32_t handleIndex, shared_ptr<RecvContext> &data)
{
    bool bRet = getRecvQueue(handleIndex).pop_front(data);

    if (!bRet)
    {
        return bRet;
    }

    --_iRecvBufferSize;

    return bRet;
}

size_t BindAdapter::getRecvBufferSize() const
{
    return _iRecvBufferSize;
}

size_t BindAdapter::getSendBufferSize() const
{
    return _iSendBufferSize;
}

TC_NetWorkBuffer::PACKET_TYPE BindAdapter::echo_protocol(TC_NetWorkBuffer &r, vector<char> &o)
{
    o = r.getBuffers();

    r.clearBuffers();

    return TC_NetWorkBuffer::PACKET_FULL;
}

TC_NetWorkBuffer::PACKET_TYPE BindAdapter::echo_header_filter(TC_NetWorkBuffer::PACKET_TYPE i, vector<char> &o)
{
    return TC_NetWorkBuffer::PACKET_FULL;
}

void BindAdapter::setName(const string &name)
{
    std::lock_guard<std::mutex> lock (_mutex);
    _name = name;
}

string BindAdapter::getName() const
{
    std::lock_guard<std::mutex> lock (_mutex);
    return _name;
}

int BindAdapter::getHandleNum()
{
    return _iHandleNum;
}

int BindAdapter::getQueueCapacity() const
{
    return _iQueueCapacity;
}

void BindAdapter::setQueueCapacity(int n)
{
    _iQueueCapacity = n;
}

int BindAdapter::isOverloadorDiscard()
{
    int iRecvBufferSize = _iRecvBufferSize;

    if(iRecvBufferSize > (int)(_iQueueCapacity / 5.*4) && (iRecvBufferSize < _iQueueCapacity) && (_iQueueCapacity > 0)) //overload
        {
        //超过队列4/5开始认为过载
        return -1;
        }
    else if(iRecvBufferSize > (int)(_iQueueCapacity) &&  _iQueueCapacity > 0 ) //队列满需要丢弃接受的数据包
        {
        return -2;
        }
    return 0;
}

void BindAdapter::setQueueTimeout(int t)
{
    if (t >= MIN_QUEUE_TIMEOUT)
    {
        _iQueueTimeout = t;
    }
    else
    {
        _iQueueTimeout = MIN_QUEUE_TIMEOUT;
    }
}

int BindAdapter::getQueueTimeout() const
{
    return _iQueueTimeout;
}

void BindAdapter::setEndpoint(const string &str)
{
    std::lock_guard<std::mutex> lock (_mutex);

    _ep.parse(str);
}

TC_Endpoint BindAdapter::getEndpoint() const
{
    std::lock_guard<std::mutex> lock (_mutex);
    return _ep;
}

TC_Socket &BindAdapter::getSocket()
{
    return _s;
}

void BindAdapter::setMaxConns(int iMaxConns)
{
    _iMaxConns = iMaxConns;
}

size_t BindAdapter::getMaxConns() const
{
    return _iMaxConns;
}

void BindAdapter::setHeartBeatTime(time_t t)
{
    _iHeartBeatTime = t;
}

time_t BindAdapter::getHeartBeatTime() const
{
    return _iHeartBeatTime;
}

void BindAdapter::setOrder(EOrder eOrder)
{
    _eOrder = eOrder;
}

void BindAdapter::setAllow(const vector<string> &vtAllow)
{
    std::lock_guard<std::mutex> lock (_mutex);

    _vtAllow = vtAllow;
}

void BindAdapter::setDeny(const vector<string> &vtDeny)
{
    std::lock_guard<std::mutex> lock (_mutex);

    _vtDeny = vtDeny;
}

BindAdapter::EOrder BindAdapter::getOrder() const
{
    return _eOrder;
}

const vector<string> &BindAdapter::getAllow() const
{
    return _vtAllow;
}

const vector<string> &BindAdapter::getDeny() const
{
    return _vtDeny;
}

bool BindAdapter::isLimitMaxConnection() const
{
    return (_iCurConns + 1 > _iMaxConns) || (_iCurConns + 1 > (int)((uint32_t)1 << 22) - 1);
}

void BindAdapter::decreaseNowConnection()
{
    --_iCurConns;
}

void BindAdapter::increaseNowConnection()
{
    ++_iCurConns;
}

int BindAdapter::getNowConnection() const
{
    return _iCurConns;
}

vector<ConnStatus> BindAdapter::getConnStatus()
{
    return _pEpollServer->getConnStatus(_s.getfd());
}

HandlePtr BindAdapter::getHandle(size_t index) {
    assert(index <= _iHandleNum);
    assert(getEpollServer()->isMergeHandleNetThread());
    return _handles[index];
}

NetThread* BindAdapter::getNetThreadOfFd(int fd) const{
    return _pEpollServer->getNetThreadOfFd(fd);
}

void BindAdapter::setProtocol(const TC_NetWorkBuffer::protocol_functor &pf, int iHeaderLen, const header_filter_functor &hf)
{
    _pf = pf;

    _hf = hf;

    _iHeaderLen = iHeaderLen;
}

TC_NetWorkBuffer::protocol_functor &BindAdapter::getProtocol()
{
    return _pf;
}

header_filter_functor &BindAdapter::getHeaderFilterFunctor()
{
    return _hf;
}

int BindAdapter::getHeaderFilterLen()
{
    return _iHeaderLen;
}


} // xy
