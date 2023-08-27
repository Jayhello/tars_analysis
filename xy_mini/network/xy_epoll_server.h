//
// Created by Administrator on 2023/8/22.
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
#include "xy_epoller.h"
#include "xy_connection.h"
#include "xy_netthread.h"
#include "xy_bind_adapter.h"
#include "xy_net_comm.h"
#include "util/xy_thread_queue.h"
#include "util/xy_monitor.h"
#include "util/xy_logger.h"

using namespace std;

namespace xy {

/**
 * 链接状态
 * Connection state
 */
struct ConnStatus{
    string          ip;
    int32_t         uid;
    uint16_t        port;
    int             timeout;
    int             iLastRefreshTime;
    size_t          recvBufferSize;
    size_t          sendBufferSize;
};

class TC_EpollServer : public TC_HandleBase {
public:

    enum EM_CLOSE_T {
        /**Client active shutdown*/
        EM_CLIENT_CLOSE = 0,         //客户端主动关闭
        /**The service-side business proactively calls 'close' to close the connection,
         * or the framework proactively closes the connection due to an exception.*/
        EM_SERVER_CLOSE = 1,        //服务端业务主动调用close关闭连接,或者框架因某种异常主动关闭连接
        /**Connection timed out, server actively closed*/
        EM_SERVER_TIMEOUT_CLOSE = 2  //连接超时了，服务端主动关闭
    };

public:
    /**
     * 构造函数
     * Constructor
     */
    TC_EpollServer(unsigned int iNetThreadNum = 1);

    /**
     * 析构函数
     * Destructor
     */
    ~TC_EpollServer();

    /**
     * 是否启用防止空链接攻击的机制
     * Whether mechanisms to prevent empty link attacks are enabled
     * @param bEnable
     */
    void enAntiEmptyConnAttack(bool bEnable);

    /**
     *设置空连接超时时间
     *Set empty connection timeout
     */
    void setEmptyConnTimeout(int timeout);

    /**
     * 设置本地日志
     * Set local log
     * @param plocalLogger
     */
    void setLocalLogger(RollWrapperInterface *pLocalLogger) { _pLocalLogger = pLocalLogger; }

    /**
     * 选择网络线程
     * Select network threads
     * @param fd
     */
    inline NetThread *getNetThreadOfFd(int fd) { return _netThreads[fd % _netThreads.size()]; }

    /**
     * 合并handle线程和网络线程
     * Merge handle and network threads
     * @param merge
     */
    void setMergeHandleNetThread(bool merge) { _mergeHandleNetThread = merge; }

    /**
     * 是否合并handle线程网络线程
     * Whether to merge handle thread network threads
     * @return
     */
    inline bool isMergeHandleNetThread() const { return _mergeHandleNetThread; }

    /**
     * 绑定监听socket
     * Bind listening socket
     * @param ls
     */
    int bind(BindAdapterPtr &lsPtr);

    /**
     * 启动业务处理线程
     * Start Business Processing Thread
     */
    void startHandle();

    /**
     * 生成epoll
     * Generate epoll
     */
    void createEpoll();

    /**
     * 运行
     * Run
     */
    void waitForShutdown();

    /**
     * 停止服务
     * Stop Service
     */
    void terminate();

    /**
     * 是否服务结束了
     * Whether the service is over
     *
     * @return bool
     */
    bool isTerminate() const { return _bTerminate; }

    /**
     * 根据名称获取BindAdapter
     * Get BindAdapter according to the name
     * @param sName
     * @return BindAdapterPtr
     */
    BindAdapterPtr getBindAdapter(const string &sName);

    /**
     * 获取所有adatapters
     * Get all adapters
     * @return
     */
    vector<BindAdapterPtr> getBindAdapters();

    /**
     * 向网络线程添加连接
     * Add remote connection to the network thread
     */
    void addConnection(Connection *cPtr, int fd, CONN_TYPE iType);

    /**
     * 关闭连接
     * Close connection
     * @param uid
     */
//    void close(const shared_ptr<TC_EpollServer::RecvContext> &data);
    void close(const shared_ptr<RecvContext> &data);

    /**
     * 发送数据
     * Send data
     * @param uid
     * @param s
     */
    void send(const shared_ptr<SendContext> &data);

    /**
     * 获取某一监听端口的连接数
     * Get the connection amount of a certain listening port
     * @param lfd
     *
     * @return vector<TC_EpollServer::ConnStatus>
     */
    vector<ConnStatus> getConnStatus(int lfd);

    /**
     * 获取监听socket信息
     * Get the information of the listening socket
     *
     * @return map<int,ListenSocket>
     */
    unordered_map<int, BindAdapterPtr> getListenSocketInfo();

    /**
     * 获取所有连接的数目
     * Get the amount of all connections
     *
     * @return size_t
     */
    size_t getConnectionCount();

    /**
     * 记录日志
     * Logging
     * @param s
     */
    void debug(const string &s) const;

    /**
     * INFO日志
     * INFO LOG
     * @param s
     */
    void info(const string &s) const;

    /**
    * 记录错误日志
    * Log errors
    * @param s
    */
    void error(const string &s) const;

    /**
     * tars日志
     * tars log
     * @param s
     */
    void tars(const string &s) const;

    /**
     * 获取网络线程的数目
     * Get the amount of the network threads
     */
    unsigned int getNetThreadNum() { return _netThreadNum; }

    /**
     * 获取网络线程的指针集合
     * Get the collection of pointers for a network thread
     */
//    vector<TC_EpollServer::NetThread *> getNetThread() { return _netThreads; }
    vector<NetThread *> getNetThread() { return _netThreads; }

    /**
     * 停止线程
     * Stop the thread
     */
    void stopThread();

    /**
     * 获取所有业务线程的数目
     * Get the amount of all the bussiness threads
     */
    size_t getLogicThreadNum();

    // 接收新的客户端链接时的回调
//    typedef std::function<void(TC_EpollServer::Connection *)> accept_callback_functor;
    typedef std::function<void(Connection *)> accept_callback_functor;

    /*
     * 设置接收链接的回调
     */
    void setOnAccept(const accept_callback_functor &f) { _acceptFunc = f; }

    //回调给应用服务
    //Callback to application service
    typedef std::function<void(TC_EpollServer *)> application_callback_functor;

    /**
     * 设置waitForShutdown线程回调的心跳
     * Set the heartbeat of the thread callback of the waitForShutdown
     * @param hf [description]
     */
    void setCallbackFunctor(const application_callback_functor &hf) { _hf = hf; }

    //网络线程发送心跳的函数
    //Function for network threads to send heartbeats
    typedef std::function<void(const string &)> heartbeat_callback_functor;

    /**
     * 设置netthread网络线程发送心跳的函数
     * Function for setting netthreaded network threads to send heartbeats
     * @param hf [description]
     */
    void setHeartBeatFunctor(const heartbeat_callback_functor &heartFunc) { _heartFunc = heartFunc; }

    heartbeat_callback_functor &getHeartBeatFunctor() { return _heartFunc; }

protected:

    friend class BindAdapter;

    /**
     * 接收句柄
     * Receiving handle
     * @param fd
     * @return
     */
    bool accept(int fd, int domain = AF_INET);

    /**
     * 绑定端口
     * Bind Port
     * @param ep
     * @param s
     * @param manualListen
     */
    void bind(const TC_Endpoint &ep, TC_Socket &s, bool manualListen);

    static void applicationCallback(TC_EpollServer *epollServer);

private:
    /**
     * 网络线程
     * Network Thread
     */
    std::vector<NetThread *> _netThreads;

    /*
     * 网络线程数目
     * Network Thread Amount
     */
    unsigned int _netThreadNum;

    /**
     * epoll
     */
    TC_Epoller _epoller;

    /**
     * 通知epoll
     * Notify epoll
     */
    TC_Epoller::NotifyInfo _notify;

    /*
     * 服务是否停止
     * Whether the service is stopped
     */
    bool _bTerminate;

    /*
     * 业务线程是否启动
     * Whether the bussiness thread is started.
     */
    bool _handleStarted;

    /**
     * 合并网络和业务线程
     * Merge network and business threads
     */
    bool _mergeHandleNetThread = false;

    /**
     * 本地循环日志
     * Local Loop Log
     */
    RollWrapperInterface *_pLocalLogger;

    /**
     *
     */
    vector<BindAdapterPtr> _bindAdapters;

    /**
     * 监听socket
    * Listening socket
     */
    unordered_map<int, BindAdapterPtr> _listeners;

    /**
     * 应用回调
     * Application callback
     */
    application_callback_functor _hf;

    /**
     * 发送心跳的函数
     * Heartbeat Sending Function
     */
    heartbeat_callback_functor _heartFunc;

    /**
     * 接收链接的回调函数
     */
    accept_callback_functor _acceptFunc;

};

typedef TC_AutoPtr<TC_EpollServer> TC_EpollServerPtr;
} // xy

