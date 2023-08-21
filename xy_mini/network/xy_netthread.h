//
// Created by Administrator on 2023/8/21.
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
#include "tc_network_buffer.h"
#include "util/xy_thread.h"
#include "xy_epoller.h"

using namespace std;

namespace xy {

class NetThread : public TC_Thread, public TC_HandleBase {
public:

    ////////////////////////////////////////////////////////////////////////////
public:
    /**
     * 构造函数
     * Constructor
     */
    NetThread(TC_EpollServer *epollServer, int index);

    /**
     * 析构函数
     * Destructor
     */
    virtual ~NetThread();

    /**
     * 获取网络线程的index
     * Get the index for the network threads
    * @return
    */
    int getIndex() const { return _threadIndex; }

    /**
     * 网络线程执行函数
     * Network thread execution function
     */
    virtual void run();

    /**
     * 停止网络线程
     * Stop the network thread
     */
    void terminate();

    /**
     * 生成epoll
     * Generate epoll
     */
    void createEpoll(uint32_t maxAllConn);

    /**
     * 初始化udp监听
     * Initialize UDP listening
     */
    void initUdp(const unordered_map<int, BindAdapterPtr> &listeners);

    /**
     * 是否服务结束了
     * Whether the service is end.
     *
     * @return bool
     */
    bool isTerminate() const { return _bTerminate; }

    /**
     * 获取Epoller对象
     * Get the Epoller Object
     *
     * @return TC_Epoller*
     */
    TC_Epoller *getEpoller() { return &_epoller; }

    /**
     * 唤醒网络线程
     * Wake up the network thread
     */
    void notify();

    /**
     * 关闭连接
     * Close Connection
     * @param uid
     */
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
     * Get the number of connections for a listening port
     * @param lfd
     *
     * @return vector<TC_EpollServer::ConnStatus>
     */
    vector<TC_EpollServer::ConnStatus> getConnStatus(int lfd);

    /**
     * 获取连接数
     * Get the number of connections
     *
     * @return size_t
     */
    size_t getConnectionCount() { return _list.size(); }

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
     * TARS日志
     * TARS LOG
     * @param s
     */
    void tars(const string &s) const;

    /**
     * 记录错误日志
     * Log errors
     * @param s
     */
    void error(const string &s) const;

    /**
     * 是否启用防止空链接攻击的机制
     * Whether the mechanism to prevent null link attacks is enabled.
     * @param bEnable
     */
    void enAntiEmptyConnAttack(bool bEnable);

    /**
     *设置空连接超时时间
     *Set empty connection timeout
     */
    void setEmptyConnTimeout(int timeout);

    /**
     *设置udp的接收缓存区大小，单位是B,最小值为8192，最大值为DEFAULT_RECV_BUFFERSIZE
     *Sets the size of the receiving buffer in UDP in B, with a minimum of 8192 and a maximum of DEFAULT_RECV_BUFFERSIZE
     */
    void setUdpRecvBufferSize(size_t nSize = DEFAULT_RECV_BUFFERSIZE);


protected:

    /**
     * 获取连接
     * Get connection
     * @param id
     *
     * @return ConnectionPtr
     */
    Connection *getConnectionPtr(uint32_t uid) { return _list.get(uid); }

    /**
     * 添加tcp链接
     * Add TCP connection
     * @param cPtr
     * @param iIndex
     */
    void addTcpConnection(Connection *cPtr);

    /**
     * 添加udp连接
     * Add UDP connection
     * @param cPtr
     * @param index
     */
    void addUdpConnection(Connection *cPtr);

    /**
     * 删除链接
     * Delete connection
     * @param cPtr
     * @param bEraseList 是否是超时连接的删除
     * @param bEraseList Whether it is deletion of timeout connection
     * @param closeType  关闭类型,0:表示客户端主动关闭；1:服务端主动关闭;2:连接超时服务端主动关闭
     * @param closeType  Close type, 0: indicates active closure of client, 1: active closure of server, 2: active closure of connection timeout server
     */
    void delConnection(Connection *cPtr, bool bEraseList = true, EM_CLOSE_T closeType = EM_CLIENT_CLOSE);

    /**
     * 处理管道消息
     * Processing Pipeline Messages
     */
    void processPipe();

    /**
     * 处理网络请求
     * Processing Network Request
     */
    void processNet(const epoll_event &ev);

    /**
     * 空连接超时时间
     * Empty connection timeout
     */
    int getEmptyConnTimeout() const;

    /**
     *是否空连接检测
     *Empty connection detection examination
     */
    bool isEmptyConnCheck() const;

    friend class BindAdapter;

    friend class ConnectionList;

    friend class TC_EpollServer;

private:
    /**
     * 服务
     * Service
     */
    TC_EpollServer *_epollServer;

    /**
     * net线程的id
     * the net thread id
     */
    std::thread::id _threadId;

    /**
     * 线程索引
     * the thread index
     */
    int _threadIndex;

    /**
     * epoll
     */
    TC_Epoller _epoller;

    /**
     * 停止
     * Stop
     */
    bool _bTerminate;

    /**
     * 通知epoll
     * Notify epoll
     */
    TC_Epoller::NotifyInfo _notify;

    /**
     * 管理的连接链表
     * Managed Link List
     */
    ConnectionList _list;

    /**
     * 发送队列
     * Sending Queue
     */
    send_queue _sbuffer;

    /**
     *空连接检测机制开关
     *Switch for empty connection detection mechanism
     */
    bool _bEmptyConnAttackCheck;


    /**
     * 空连接超时时间,单位是毫秒,默认值2s,
     * 该时间必须小于等于adapter自身的超时时间
     * Empty connection timeout in milliseconds, default 2s,
     * The time must be less than or equal to the adapter's own timeout
     */
    int _iEmptyCheckTimeout;

    /**
     * udp连接时接收包缓存大小,针对所有udp接收缓存有效
     * Received packet cache size on UDP connection, valid for all UDP receive caches
     */
    size_t _nUdpRecvBufferSize;

    /**
     * 通知信号
     * Notification signal
     */
    bool _notifySignal = false;
};

} // xy
