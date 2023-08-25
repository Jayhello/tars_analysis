//
// Created by Administrator on 2023/8/18.
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
#include "xy_clientsocket.h"
#include "xy_handle.h"
#include "util/xy_thread_queue.h"
#include "util/xy_monitor.h"
#include "xy_epoll_server.h"

using namespace std;

namespace xy {

using ConnStatus = TC_EpollServer::ConnStatus;

enum EM_CLOSE_T {
    /**Client active shutdown*/
    EM_CLIENT_CLOSE = 0,         //客户端主动关闭
    /**The service-side business proactively calls 'close' to close the connection,
     * or the framework proactively closes the connection due to an exception.*/
    EM_SERVER_CLOSE = 1,        //服务端业务主动调用close关闭连接,或者框架因某种异常主动关闭连接
    /**Connection timed out, server actively closed*/
    EM_SERVER_TIMEOUT_CLOSE = 2  //连接超时了，服务端主动关闭
};

using close_functor = std::function<void(void *, EM_CLOSE_T)>;

class BindAdapter;

typedef TC_AutoPtr<BindAdapter> BindAdapterPtr;

class RecvContext;

/**
 * 发送包的上下文
 * 由RecvContext创建出来
 * Context of sending packets
 * Created by RecvContext
 */
class SendContext {
public:
    SendContext(const shared_ptr<RecvContext> &context, char cmd) : _context(context), _cmd(cmd) {
        _sbuffer = std::make_shared<TC_NetWorkBuffer::Buffer>();
    }

    const shared_ptr<RecvContext> &getRecvContext() { return _context; }

    const shared_ptr<TC_NetWorkBuffer::Buffer> &buffer() { return _sbuffer; }

    char cmd() const { return _cmd; }

    uint32_t uid() const { return _context->uid(); }

    int fd() const { return _context->fd(); }

    const string &ip() const { return _context->ip(); }

    uint16_t port() const { return _context->port(); }

    friend class RecvContext;

protected:
    shared_ptr<RecvContext> _context;
    /**Send package is valid. Command: 'c', close FD; 's', data need to be sent*/
    char _cmd;            /**send包才有效, 命令:'c',关闭fd; 's',有数据需要发送*/
    /**Sent context*/
    shared_ptr<TC_NetWorkBuffer::Buffer> _sbuffer;        /**发送的内容*/
};

////////////////////////////////////////////////////////////////////////////
/**
 * 接收包的上下文
 * Context of receiving package
 */
class RecvContext : public std::enable_shared_from_this<RecvContext> {
public:
    RecvContext(uint32_t uid, const string &ip, int64_t port, int fd, const BindAdapterPtr &adapter,
                bool isClosed = false, int closeType = EM_CLIENT_CLOSE)
            : _uid(uid), _ip(ip), _port(port), _fd(fd), _adapter(adapter), _isClosed(isClosed),
              _closeType(closeType), _recvTimeStamp(TNOWMS) {
    }

    uint32_t uid() const { return _uid; }

    const string &ip() const { return _ip; }

    uint16_t port() const { return _port; }

    vector<char> &buffer() { return _rbuffer; }

    const vector<char> &buffer() const { return _rbuffer; }

    int64_t recvTimeStamp() const { return _recvTimeStamp; }

    bool isOverload() const { return _isOverload; }

    void setOverload() { _isOverload = true; }

    bool isClosed() const { return _isClosed; }

    int fd() const { return _fd; }

    BindAdapterPtr &adapter() { return _adapter; }

    int closeType() const { return _closeType; }

    void setCloseType(int closeType) { _closeType = closeType; }

    shared_ptr<SendContext> createSendContext() { return std::make_shared<SendContext>(shared_from_this(), 's'); }

    shared_ptr<SendContext> createCloseContext() { return std::make_shared<SendContext>(shared_from_this(), 'c'); }

protected:
    /**Connection Label*/
    uint32_t _uid;            /**连接标示*/
    /**IP for remote connection*/
    string _ip;             /**远程连接的ip*/
    /**Port for remote connection*/
    uint16_t _port;           /**远程连接的端口*/
    /**Save the FD that generated the message to select the network thread when returning the package*/
    int _fd;                /*保存产生该消息的fd，用于回包时选择网络线程*/
    /**Message identifying which adapter*/
    BindAdapterPtr _adapter;        /**标识哪一个adapter的消息*/
    /**Received message*/
    vector<char> _rbuffer;        /**接收的内容*/
    /**Is overloaded*/
    bool _isOverload = false;     /**是否已过载 */
    /**Is closed*/
    bool _isClosed = false;       /**是否已关闭*/
    /**If the message package is closed, the type of closure is identified,
     * 0: the client actively closes,
     * 1: the server actively closes,
     * 2: the connection timeout server actively closes.
     * */
    int _closeType;     /*如果是关闭消息包，则标识关闭类型,0:表示客户端主动关闭；1:服务端主动关闭;2:连接超时服务端主动关闭*/
    /**Time to receive data*/
    int64_t _recvTimeStamp;  /**接收到数据的时间*/
};

//	typedef TC_CasQueue<shared_ptr<RecvContext>> recv_queue;
typedef TC_ThreadQueue<shared_ptr<RecvContext>> recv_queue;
//	typedef TC_CasQueue<shared_ptr<SendContext>> send_queue;
typedef TC_ThreadQueue<shared_ptr<SendContext>> send_queue;
typedef recv_queue::queue_type recv_queue_type;

using auth_process_wrapper_functor = std::function<bool (Connection *c, const shared_ptr<RecvContext> &recv )>;

// 服务端口管理,监听socket信息
// Service port management, listening for socket information
class BindAdapter : public TC_HandleBase {
public:
    /**
     * 缺省的一些定义
     * Defualt definitions
     */
    enum {
        /**Flow*/
        DEFAULT_QUEUE_CAP = 10 * 1024,    /**流量*/
        /**Queue minimum timeout (ms)*/
        MIN_QUEUE_TIMEOUT = 3 * 1000,     /**队列最小超时时间(ms)*/
        /**Default maximum connections*/
        DEFAULT_MAX_CONN = 1024,       /**缺省最大连接数*/
        /**Default queue timeout (ms)*/
        DEFAULT_QUEUE_TIMEOUT = 60 * 1000,    /**缺省的队列超时时间(ms)*/
    };
    /**
     * 顺序
     * Order
     */
    enum EOrder {
        ALLOW_DENY,
        DENY_ALLOW
    };

    /**
     * 数据队列
     * Data Queue
     */
    struct DataQueue {
        /**
         * 接收的数据队列
         * Received data queue
         */
        recv_queue _rbuffer;

        /**
         * 锁
         * Lock
         */
        TC_ThreadLock _monitor;
    };

    /**
     * 构造函数
     * Constructor
     */
    BindAdapter(TC_EpollServer *pEpollServer);

    /**
     * 析够函数
     * Destructor
     */
    ~BindAdapter();

    /**
     * 设置需要手工监听
     * Set requires manual listening
     */
    void enableManualListen() { _manualListen = true; }

    /**
     * 是否手工监听端口
     * Whether to manual listen the port or not
     * @return
     */
    bool isManualListen() const { return _manualListen; }

    /**
     * 手工绑定端口
     * Manual port binding
     */
    void manualListen();

    /**
     * 设置adapter name
     * Set up adapter name
     * @param name
     */
    void setName(const string &name);

    /**
     * 获取adapter name
     * Get adapter name
     * @return string
     */
    string getName() const;

    /**
     * 增加处理线程对应的接收队列
     * Add the corresponding receiving queue for processing threads
     * @return string
     */
    void initThreadRecvQueue(uint32_t handeIndex);

    /**
     * 获取queue capacity
     * Get queue capacity
     * @return int
     */
    int getQueueCapacity() const;

    /**
     * 设置queue capacity
     * Set up queue capacity
     * @param n
     */
    void setQueueCapacity(int n);

    /**
     * 设置协议名称
     * Set up the protocol name
     * @param name
     */
    void setProtocolName(const string &name);

    /**
     * 获取协议名称
     * Get the protocol name
     * @return const string&
     */
    const string &getProtocolName();

    /**
     * 是否tars协议
     * Whether it is the tars protocol
     * @return bool
     */
    bool isTarsProtocol();

    /**
     * 判断是否需要过载保护
     * Determine whether it needs overload protection
     * @return bool
     */
    int isOverloadorDiscard();

    /**
     * 设置消息在队列中的超时时间, t为毫秒
     * (超时时间精度只能是s)
     * Set the timeout time of the message in the queue, t is milliseconds
     * (timeout precision can only be s)
     *
     * @param t
     */
    void setQueueTimeout(int t);

    /**
     * 获取消息在队列中的超时时间, 毫秒
     * Get timeout of message in queue, MS
     * @return int
     */
    int getQueueTimeout() const;

    /**
     * 设置endpoint
     * Set up endpoint
     * @param str
     */
    void setEndpoint(const string &str);

    /**
     * 获取ip
     * Get ip
     * @return const string&
     */
    TC_Endpoint getEndpoint() const;

    /**
     * 监听socket
     * Listen socket
     * @return TC_Socket
     */
    TC_Socket &getSocket();

    /**
     * 设置最大连接数
     * Set the maximum connection number
     * @param iMaxConns
     */
    void setMaxConns(int iMaxConns);

    /**
     * 获取最大连接数
     * Get the maximum connection number
     * @return size_t
     */
    size_t getMaxConns() const;

    /**
     * 设置HeartBeat时间
     * Set the HeartBeat time
     * @param n
     */
    void setHeartBeatTime(time_t t);

    /**
     * 获取HeartBeat时间
     * Get the HeartBeat time
     * @return size_t
     */
    time_t getHeartBeatTime() const;

    /**
     * 设置allow deny次序
     * Set the allow deny order
     * @param eOrder
     */
    void setOrder(EOrder eOrder);

    /**
     * 设置允许ip
     * Set allowed ip
     * @param vtAllow
     */
    void setAllow(const vector<string> &vtAllow);

    /**
     * 设置禁止ip
     * Set the disabled ip
     * @param vtDeny
     */
    void setDeny(const vector<string> &vtDeny);

    /**
     * 获取允许ip
     * Get the allowed ip
     * @return vector<string>: ip列表
     * @return vector<string>: ip list
     */
    const vector<string> &getAllow() const;

    /**
    * 获取禁止ip
    * Get the disable ip
    * @return vector<string>: ip列表
    * @return vector<string>: ip list
    */
    const vector<string> &getDeny() const;

    /**
    * 获取allow deny次序
    * Get the allow deny order
    * @return EOrder
    */
    EOrder getOrder() const;

    /**
     * 是否Ip被允许
     * Whether the ip is allowed or not
     * @param ip
     * @return bool
     */
    bool isIpAllow(const string &ip) const;

    /**
     * 是否超过了最大连接数
     * Whether it exceeds the maximum connection number
     * @return bool
     */
    bool isLimitMaxConnection() const;

    /**
     * 减少当前连接数
     * Reduce current connections
     */
    void decreaseNowConnection();

    /**
     * 增加当前连接数
     * Increase current connections
     */
    void increaseNowConnection();

    /**
     * 获取所有链接状态
     * Get all connection states
     * @return ConnStatus
     */
    vector<ConnStatus> getConnStatus();

    /**
     * 获取当前连接数
     * Get current connections
     * @return int
     */
    int getNowConnection() const;

    /**
     * 获取服务
     * Get service
     * @return TC_EpollServer*
     */
    TC_EpollServer *getEpollServer() const { return _pEpollServer; };

    /**
     * 获取对应的网络线程
     * Get the corresponding network thread
     * @param fd
     * @return
     */
    inline NetThread *getNetThreadOfFd(int fd) const { return _pEpollServer->getNetThreadOfFd(fd); }

    /**
     * 注册协议解析器
     * Registration Protocol parser
     * @param pp
     */
    void setProtocol(const TC_NetWorkBuffer::protocol_functor &pf, int iHeaderLen = 0,
                     const header_filter_functor &hf = echo_header_filter);

    /**
     * 获取协议解析器
     * Get Registration Protocol parser
     * @return protocol_functor&
     */
    TC_NetWorkBuffer::protocol_functor &getProtocol();

    /**
     * 解析包头处理对象
     * Resolve Package Header Processing Objects
     * @return protocol_functor&
     */
    header_filter_functor &getHeaderFilterFunctor();

    /**
     * 增加数据到队列中
     * Add data to the queue
     * @param vtRecvData
     * @param bPushBack 后端插入
     * @param bPushBack Backend insert
     * @param sBuffer
     */
    void insertRecvQueue(const shared_ptr<RecvContext> &recv);//, bool bPushBack = true);

    /**
     * 等待数据
     * Wait for data
     * @return bool
     */
    bool waitForRecvQueue(uint32_t handleIndex, shared_ptr<RecvContext> &recv);

    /**
     * 接收队列的大小
     * Size of the received queue
     * @return size_t
     */
    size_t getRecvBufferSize() const;

    /**
     * 发送队列的大小
     * Size of the sent queue
     * @return size_t
     */
    size_t getSendBufferSize() const;

    /**
     * add send buffer size
     */
    inline void increaseSendBufferSize() { ++_iSendBufferSize; }

    /**
     * increase send buffer size
     */
    inline void decreaseSendBufferSize(size_t s = 1) { _iSendBufferSize.fetch_sub(s); }

    /**
     * 默认的协议解析类, 直接echo
     * Default protocol resolution class, direct echo
     * @param r
     * @param o
     * @return int
     */
    static TC_NetWorkBuffer::PACKET_TYPE echo_protocol(TC_NetWorkBuffer &r, vector<char> &o);

    /**
     * 默认的包头处理
     * Default header handling
     * @param i
     * @param o
     * @return int
     */
    static TC_NetWorkBuffer::PACKET_TYPE echo_header_filter(TC_NetWorkBuffer::PACKET_TYPE i, vector<char> &o);

    /**
     * 获取需要过滤的包头长度
     * Get the header length that needs to be filtered
     */
    int getHeaderFilterLen();

    /**
     * 所属handle组的handle数(每个handle一个对象)
     * Number of handles belonging to the handle group (one object per handle)
     * @return int
     */
    int getHandleNum();

    /**
     * 初始化处理线程,线程将会启动
     * Initialize the processing thread, which will start
     */
    template<typename T, typename ...Args>
    void setHandle(size_t n, Args &&... args) {
        if (!_handles.empty()) {
            getEpollServer()->error("[BindAdapter::setHandle] handle is not empty!");
            return;
        }

        _iHandleNum = n;

        _threadDataQueue.resize(_iHandleNum + 1);
        _threadDataQueue[0] = std::make_shared<BindAdapter::DataQueue>();

        if (_pEpollServer->isMergeHandleNetThread()) {
            _iHandleNum = _pEpollServer->_netThreadNum;
        }

        for (int32_t i = 0; i < _iHandleNum; ++i) {
            HandlePtr handle = new T(args...);

            handle->setHandleIndex(i);

            handle->setEpollServer(this->getEpollServer());

            handle->setBindAdapter(this);

            _handles.push_back(handle);
        }
    }

    /**
     * 获取第几个句柄
     * Get the index of the handle
     * @param index
     * @return
     */
    HandlePtr getHandle(size_t index) {
        assert(index <= _iHandleNum);
        assert(getEpollServer()->isMergeHandleNetThread());
        return _handles[index];
    }

    /*
     * 设置服务端积压缓存的大小限制(超过大小启用)
     * Set the size limit of the server's backlog cache (exceeding the size enabled)
     */
    void setBackPacketBuffLimit(size_t iLimitSize) { _iBackPacketBuffLimit = iLimitSize; }

    /**
     * 获取服务端回包缓存的大小限制(超过大小启用)
     * Get the size limit of the server-side packet back cache (exceeding the size enabled)
     */
    size_t getBackPacketBuffLimit() const { return _iBackPacketBuffLimit; }

    /*
     * 设置服务端5/s最低发送字节
     * Set the Server 5/s Minimum Sending Bytes
     */
    void setBackPacketBuffMin(size_t iMinLimit) { _iBackPacketBuffMin = iMinLimit; }

    /**
     * 获取服务端5/s最低发送字节
     * Get the Server 5/s Minimum Sending Bytes
     */
    size_t getBackPacketBuffMin() const { return _iBackPacketBuffMin; }

    /**
     * 获取服务端接收队列(如果_rnbuffer有多个, 则根据调用者的线程id来hash获取)
     * Get the server receive queue (if there's more than one _rnbuffer, get from the hash based on the caller's thread id)
     *
     */
    recv_queue &getRecvQueue(uint32_t handleIndex);

    /**
     * 获取handles
     * Get handles
     */
    const vector<HandlePtr> &getHandles() { return _handles; }

    /**
     * 是否是队列模式(默认是False的)
     * Whether it is the queue mode (Defualt false)
     */
    bool isQueueMode() const { return _queueMode; }

    /**
     * 开启队列模式(同一个连接的请求, 落在同一个handle处理线程中)
     * Open queue mode (The requests from the same connecion will fall in the same handle processing thread )
     */
    void enableQueueMode() { _queueMode = true; }

    /**
     * 等待队列数据
     * Wait for the queue data
     */
    void waitAtQueue(uint32_t handleIndex, uint32_t waitTime);

    /**
     * 通知某个具体handle醒过来
     * Notify a specific handle to wake up
     * @param handleIndex
     */
    void notifyHandle(uint32_t handleIndex);

    /**
     * 设置close回调函数
     * Set close callback function
     */
    void setOnClose(const close_functor &f) { _closeFunc = f; }

    /**
     * 注册鉴权包裹函数
     * Regist Authentication Package Function
     * @param apwf
     */
    void setAuthProcessWrapper(const auth_process_wrapper_functor &apwf) { _authWrapper = apwf; }

    void setAkSk(const std::string &ak, const std::string &sk) {
        _accessKey = ak;
        _secretKey = sk;
    }

    bool checkAkSk(const std::string &ak, const std::string &sk) { return ak == _accessKey && sk == _secretKey; }

    std::string getSk(const std::string &ak) const { return (_accessKey == ak) ? _secretKey : ""; }

    void setSSLCtx(const shared_ptr<TC_OpenSSL::CTX> &ctx) { _ctx = ctx; }

    shared_ptr<TC_OpenSSL::CTX> getSSLCtx() { return _ctx; };

private:
    /**
     * 获取等待的队列锁
     * Get the waiting queue lock
     * @return
     */
    TC_ThreadLock &getLock(uint32_t handleIndex);

public:

    //统计上报的对象
    //Count reporting objects
//    PropertyReport *_pReportQueue = NULL;
//    PropertyReport *_pReportConRate = NULL;
//    PropertyReport *_pReportTimeoutNum = NULL;

protected:
    friend class TC_EpollServer;

    /**
     * 加锁
     * Add lock
     */
    mutable std::mutex _mutex;

    /**
     * 服务
     * Service
     */
    TC_EpollServer *_pEpollServer = NULL;

    /**
     * Adapter所用的HandleGroup
     * the HandleGroup used by Adapter
     */
    vector<HandlePtr> _handles;

    /**
     * 协议解析
     * Destruct the protocol
     */
    TC_NetWorkBuffer::protocol_functor _pf;

    /**
     * 首个数据包包头过滤
     * First packet header filtering
     */
    header_filter_functor _hf;

    /**
     * adapter的名字
     * adapter name
     */
    string _name;

    /**
     * 监听fd
     * listen fd
     */
    TC_Socket _s;

    /**
     * 绑定的IP
     * binded ip
     */
    TC_Endpoint _ep;

    /**
     * 最大连接数
     * the maximum number of connections
     */
    int _iMaxConns;

    /**
     * 当前连接数
     * the current number of connections
     */
    std::atomic<int> _iCurConns;

    /**
     * Handle个数
     * the number of Handle
     */
    size_t _iHandleNum;

    /**
     * 允许的Order
     * the Allowed Order
     */
    volatile EOrder _eOrder;

    /**
     * 允许的ip
     * the Allowed IP
     */
    vector<string> _vtAllow;

    /**
     * 禁止的ip
     * the Disabled IP
     */
    vector<string> _vtDeny;

    /**
     * 每个线程都有自己的队列
     * 0: 给共享队列模式时使用
     * 1~handle个数: 队列模式时使用
     * Every thread has its own queue.
     * 0: Use when sharing queue mode
     * 1~handle count: Use when queue mode
     */
    vector<shared_ptr<DataQueue>> _threadDataQueue;

    /**
     * 接收队列数据总个数
     * the total amount of the received queue data
     */
    atomic<size_t> _iRecvBufferSize{0};

    /**
     * 发送队列数据总个数
     * the total amount of the sent queue data
     */
    atomic<size_t> _iSendBufferSize{0};

    /**
     * 队列最大容量
     * the maximum capacity of the queue
     */
    int _iQueueCapacity;

    /**
     * 消息超时时间（从入队列到出队列间隔)(毫秒）
     * Message timeout (from queue entry to queue exit interval) (milliseconds)
     */
    int _iQueueTimeout;

    /**
     * 首个数据包包头长度
     * First packet header length
     */
    int _iHeaderLen;

    /**
     * 上次心跳发送时间
     * Last heartbeat sent time
     */
    volatile time_t _iHeartBeatTime;

    /**
     * 协议名称,缺省为"tars"
     * Protocol name, default is "tars"
     */
    string _protocolName;

    /**
     * 回包缓存限制大小
     * Packet Back Cache Limit Size
     */
    size_t _iBackPacketBuffLimit = 0;

    /**
     * 回包速度最低限制(5/s), 默认1K
     * Minimum Packet Return Speed Limit (5/s), default 1K
     */
    size_t _iBackPacketBuffMin = 1024;

    //队列模式
    //Queue Mode
    bool _queueMode = false;

    //listen模式
    //Listen Mode
    bool _manualListen = false;

    /**
     * 包裹认证函数,不能为空
     * Package authentication function, cannot be empty
     */
    auth_process_wrapper_functor _authWrapper;

    /**
     * 该obj的AK SK
     * the AK  SK of the object
     */
    std::string _accessKey;
    std::string _secretKey;

    /**
     * ssl ctx
     */
    shared_ptr<TC_OpenSSL::CTX> _ctx;

    //连接关闭的回调函数
    //Callback function with connection closed
    close_functor _closeFunc;

};

} // xy
