//
// Created by wenwen on 2023/8/21.
//

#pragma once
#include <string>
#include <memory>
#include <map>
#include <unordered_map>
#include <vector>
#include <list>
#include <algorithm>
#include <functional>
#include "xy_network_buffer.h"
#include "util/xy_thread.h"
#include "xy_epoller.h"
#include "xy_bind_adapter.h"
#include "xy_epoll_server.h"


namespace xy{

/**
 * @brief 定义服务逻辑处理的接口
 * @brief Define interfaces for logical processing of services
 *
 */
/**
 * 服务的逻辑处理代码
 * Logical Processing Code for Services
 */
class Handle : public TC_Thread, public TC_HandleBase
{
public:
    /**
     * 构造, 默认没有请求, 等待10s
     * Constructor, default no request, wait 10s
     */
    Handle();

    /**
     * 析构函数
     * Destructor
     */
    virtual ~Handle();

    /**
     * 获取服务
     * Get Service
     * @return TC_EpollServer*
     */
    TC_EpollServer* getEpollServer() const { return _pEpollServer; };

    /**
     * 获取adapter
     * Get adapter
     * @return
     */
    BindAdapter *getBindAdapter() const { return _bindAdapter; }

    /**
     * 获取Handle的索引(0~handle个数-1)
     * Get the index of Handle(0~handle count-1)
     * @return
     */
    uint32_t getHandleIndex() const { return _handleIndex; }

    /**
     * 设置网络线程
     * Set up network thread
     */
    void setNetThread(NetThread *netThread);

    /**
     * 获取网络线程
     * Get network thread
     * @return
     */
    NetThread *getNetThread() { return _netThread; }

    /**
     * 处理
     * Process
     */
    void process(shared_ptr<RecvContext> data);

    /**
     * 线程处理方法
     * Thread processing method
     */
    virtual void run();

public:
    /**
     * 发送数据
     * Send data
     * @param stRecvData
     * @param sSendBuffer
     */
    void sendResponse(const shared_ptr<SendContext> &data);

    /**
     * 关闭链接
     * Close connection
     * @param stRecvData
     */
    void close(const shared_ptr<RecvContext> &data);

    /**
     * 设置等待时间
     * Set up waiting time
     * @param iWaitTime
     */
    void setWaitTime(uint32_t iWaitTime);

    /**
     * 对象初始化
     * Object initialization
     */
    virtual void initialize() {};

    /**
     * 唤醒handle对应的处理线程
     * Wake up the process thread corresponding to the handle
     */
    virtual void notifyFilter();

    /**
     * 心跳(每处理完一个请求或者等待请求超时都会调用一次)
     * Heartbeat(Called every time a request has been processed or the waiting request has timed out)
     */
    virtual void heartbeat() {}

protected:
    /**
     * 具体的处理逻辑
     * Specific processing logic
     */
    virtual void handleImp();

    /**
     * 处理函数
     * Processing Function
     * @param stRecvData: 接收到的数据
     * @param stRecvData: received data
     */
    virtual void handle(const shared_ptr<RecvContext> &data) = 0;

    /**
     * 处理超时数据, 即数据在队列中的时间已经超过
     * 默认直接关闭连接
     * Processing timeout data, i.e. data has been queued longer than
     * Close connection directly by default
     * @param stRecvData: 接收到的数据
     * @param stRecvData: received data
     */
    virtual void handleTimeout(const shared_ptr<RecvContext> &data);

    /**
     * 处理连接关闭通知，包括
     * Handle connection shutdown notifications, including:
     * 1.close by peer
     * 2.recv/send fail
     * 3.close by timeout or overload
     * @param stRecvData:
     */
    virtual void handleClose(const shared_ptr<RecvContext> &data);

    /**
     * 处理overload数据 即数据队列中长度已经超过允许值
     * 默认直接关闭连接
     * Processing overload data means that the length of the data queue has exceeded the allowable value
     * Close connection directly by default
     * @param stRecvData: 接收到的数据
     * @param stRecvData: received data
     */
    virtual void handleOverload(const shared_ptr<RecvContext> &data);

    /**
     * 处理异步回调队列
     * Handle asynchronous callback queues

     */
    virtual void handleAsyncResponse() {}

    /**
  * handleFilter拆分的第二部分，处理用户自有数据
  * 非游戏逻辑可忽略bExpectIdle参数
  * The second part of handleFilter splitting, dealing with user-owned data
  * Non-game logic ignores bExpectIdle parameter
  */
    virtual void handleCustomMessage(bool bExpectIdle = false) {}

    /**
     * 线程已经启动, 进入具体处理前调用
     * Thread has been started and called before entering specific processing
     */
    virtual void startHandle() {}

    /**
     * 线程马上要退出时调用
     * Call when the thread is about to exit
     */
    virtual void stopHandle() {}

    /**
     * 是否所有的Adpater队列都为空
     * Whether all adapter queues are empty.
     * @return bool
     */
    virtual bool allAdapterIsEmpty();

    /**
     * 是否所有的servant都没有resp消息待处理
     * Whether all servant don't have resp message to deal.
     * @return bool
     */
    virtual bool allFilterIsEmpty();

    /**
     * 设置服务
     * Set up service
     * @param pEpollServer
     */
    void setEpollServer(TC_EpollServer *pEpollServer);

    /**
     * 设置Adapter
     * Set up Apdater
     * @param pEpollServer
     */
    void setBindAdapter(BindAdapter*  bindAdapter);

    /**
     * 设置index
     * Set up index
     * @param index
     */
    void setHandleIndex(uint32_t index);

    /**
     * 等待在队列上
     * On the waiting queue
     */
    void wait();

    /**
     * 从队列中获取数据
     * Receive data from the queue
     * @param recv
     * @return
     */
    bool popRecvQueue(shared_ptr<RecvContext> &recv);

    /**
     * 友元类
     * Friend Class
     */
    friend class BindAdapter;
protected:
    /**
     * 服务
     * Service
     */
    TC_EpollServer  *_pEpollServer;

    /**
     * handle对应的网路线程(网络线程和handle线程合并的情况下有效)
     * Network threads corresponding to handle (valid when network threads and handle threads are merged)
     */
    NetThread       *_netThread = NULL;

    /**
     * 所属handle组
     * The handle group to which one belongs
     */
    BindAdapter*    _bindAdapter;

    /**
     * 等待时间
     * Waiting time
     */
    uint32_t        _iWaitTime;

    /**
     * Handle的索引
     * Index of the Handle
     */
    uint32_t        _handleIndex;

};

typedef TC_AutoPtr<Handle> HandlePtr;

} // xy
