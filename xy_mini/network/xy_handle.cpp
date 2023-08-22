//
// Created by wenwen on 2023/8/21.
//

#include "xy_handle.h"

namespace xy {

// handle的实现
Handle::Handle()
        : _pEpollServer(NULL), _iWaitTime(100) {
}

Handle::~Handle() {
}

void Handle::handleClose(const shared_ptr<RecvContext> &data) {
}

void Handle::sendResponse(
        const shared_ptr<SendContext> &data)//uint32_t uid, const vector<char> &sSendBuffer, const string &ip, int port, int fd)
{
    _pEpollServer->send(data);
}

void Handle::close(const shared_ptr<RecvContext> &data) {
    _pEpollServer->close(data);
}

void Handle::handleTimeout(const shared_ptr<RecvContext> &data) {
    _pEpollServer->error(
            "[Handle::handleTimeout] queue timeout, close [" + data->ip() + ":" + TC_Common::tostr(data->port()) +
            "].");

    close(data);
}

void Handle::handleOverload(const shared_ptr<RecvContext> &data) {
    _pEpollServer->error("[Handle::handleOverload] adapter '" + data->adapter()->getName() + "',over load:" +
                         TC_Common::tostr(data->adapter()->getRecvBufferSize()) + ">" +
                         TC_Common::tostr(data->adapter()->getQueueCapacity()) + ".");

    close(data);
}

void Handle::run() {
    initialize();

    handleImp();
}

bool Handle::allAdapterIsEmpty() {
    if (_bindAdapter->getRecvBufferSize() > 0) {
        return false;
    }

    return true;
}

bool Handle::allFilterIsEmpty() {
    return true;
}

void Handle::setEpollServer(TC_EpollServer *pEpollServer) {
    _pEpollServer = pEpollServer;
}

void Handle::setBindAdapter(BindAdapter *bindAdapter) {
    _bindAdapter = bindAdapter;
}

void Handle::setHandleIndex(uint32_t index) {
    _handleIndex = index;
}

void Handle::setNetThread(NetThread *netThread) {
    _netThread = netThread;
}

void Handle::notifyFilter() {
    _bindAdapter->notifyHandle(_handleIndex);
}

void Handle::setWaitTime(uint32_t iWaitTime) {
    _iWaitTime = iWaitTime;
}

void Handle::process(shared_ptr<RecvContext> data) {
    try {
        //上报心跳
        heartbeat();

        //为了实现所有主逻辑的单线程化,在每次循环中给业务处理自有消息的机会
        handleAsyncResponse();

        //数据已超载 overload
        if (data->isOverload()) {
            handleOverload(data);
        }
            //关闭连接的通知消息
        else if (data->isClosed()) {
            handleClose(data);
        }
            //数据在队列中已经超时了
        else if ((TNOWMS - data->recvTimeStamp()) > (int64_t) _bindAdapter->getQueueTimeout()) {
            handleTimeout(data);
        } else {
            handle(data);
        }
        handleCustomMessage(false);
    }
    catch (exception &ex) {
        if (data) {
            close(data);
        }

        getEpollServer()->error("[Handle::handleImp] error:" + string(ex.what()));
    }
    catch (...) {
        if (data) {
            close(data);
        }

        getEpollServer()->error("[Handle::handleImp] unknown error");
    }
}

void Handle::wait() {
    if (allAdapterIsEmpty() && allFilterIsEmpty()) {
        _bindAdapter->waitAtQueue(_handleIndex, _iWaitTime);
    }
}

bool Handle::popRecvQueue(shared_ptr<RecvContext> &recv) {
    return _bindAdapter->waitForRecvQueue(_handleIndex, recv);
}

void Handle::handleImp() {
    //by goodenpei, 判断是否启用顺序模式
    _bindAdapter->initThreadRecvQueue(getHandleIndex());

    startHandle();

    while (!getEpollServer()->isTerminate()) {
        //等待
        wait();

        //上报心跳
        heartbeat();

        //为了实现所有主逻辑的单线程化,在每次循环中给业务处理自有消息的机会
        handleAsyncResponse();
        handleCustomMessage(true);

        shared_ptr<RecvContext> data;

        try {
            while (popRecvQueue(data))//_bindAdapter->waitForRecvQueue(_handleIndex, data))
            {
                //上报心跳
                heartbeat();

                //为了实现所有主逻辑的单线程化,在每次循环中给业务处理自有消息的机会
                handleAsyncResponse();

                //数据已超载 overload
                if (data->isOverload()) {
                    handleOverload(data);
                }
                    //关闭连接的通知消息
                else if (data->isClosed()) {
                    handleClose(data);
                }
                    //数据在队列中已经超时了
                else if ((TNOWMS - data->recvTimeStamp()) > (int64_t) _bindAdapter->getQueueTimeout()) {
                    handleTimeout(data);
                } else {
                    handle(data);
                }
                handleCustomMessage(false);
            }
        }
        catch (exception &ex) {
            if (data) {
                close(data);
            }

            getEpollServer()->error("[Handle::handleImp] error:" + string(ex.what()));
        }
        catch (...) {
            if (data) {
                close(data);
            }

            getEpollServer()->error("[Handle::handleImp] unknown error");
        }
    }

    stopHandle();
}


} // xy
