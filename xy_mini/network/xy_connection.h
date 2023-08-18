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
#include "tc_network_buffer.h"

using namespace std;

namespace xy{

    class Connection
            {
            public:
                enum EnumConnectionType
                        {
                    EM_TCP = 0,
                    EM_UDP = 1,
                    };

                /**
                 * 构造函数
                 * Constructor
                 * @param lfd
                 * @param s
                 * @param ip
                 * @param port
                 */
                Connection(BindAdapter *pBindAdapter, int lfd, int timeout, int fd, const string& ip, uint16_t port);

                /**
                 * udp连接
                 * UDP connection
                 * @param fd
                 */
                Connection(BindAdapter *pBindAdapter, int fd);

                /**
                 * 析构函数
                 * Destructor
                 */
                virtual ~Connection();

                /**
                 * 链接所属的adapter
                 * the adapter of the connection
                 */
                BindAdapterPtr& getBindAdapter()       { return _pBindAdapter; }

                /**
                 * 初始化
                 * Initialization
                 * @param id, 连接的唯一id
                 * @param id, the connection unique id
                 */
                void init(unsigned int uid)         { _uid = uid; }

                /**
                 * 获取连接超时时间
                 * Get connection timeout
                 *
                 * @return int
                 */
                int getTimeout() const              { return _timeout; }

                /**
                 * 获取线程的惟一id
                 * Get thread unique id
                 *
                 * @return unsigned int
                 */
                uint32_t getId() const              { return _uid; }

                /**
                 * 获取监听fd
                 * Get listening id
                 *
                 * @return int
                 */
                int getListenfd() const             { return _lfd; }

                /**
                 * 当前连接fd
                 * Current connection fd
                 *
                 * @return int
                 */
                int getfd() const                   { return _sock.getfd(); }

                /**
                 * 是否有效
                 * Whether it is valid
                 *
                 * @return bool
                 */
                bool isValid() const                { return _sock.isValid();}

                /**
                 * 远程IP
                 * Remote IP
                 *
                 * @return string
                 */
                string getIp() const                { return _ip; }

                /**
                 * 远程端口
                 * Remote Port
                 *
                 * @return uint16_t
                 */
                uint16_t getPort() const            { return _port; }

                /**
                 * 设置首个数据包包头需要过滤的字节数
                 * Set the number of bytes the first packet header needs to filter
                 */
                void setHeaderFilterLen(int iHeaderLen)     { _iHeaderLen = iHeaderLen; }

                /**
                 * 设置关闭,发送完当前数据就关闭连接
                 * Set shutdown to close connection after sending current data
                 */
                bool setClose();

                /**
                 * 获取连接类型
                 * Get the type of the connection
                 */
                EnumConnectionType getType() const          { return _enType; }

                /**
                 * 是否是空连接
                * Whether there's empty connection.
                 */
                bool isEmptyConn() const  {return _bEmptyConn;}

                /**
                 * Init Auth State;
                 */
                void tryInitAuthState(int initState);

                /**
                 * 接收数据buffer
                 * Receive data buffer
                 */
                TC_NetWorkBuffer &getRecvBuffer() { return _recvBuffer; }

                /**
                 * 发送数据buffer
                 * Send data buffer
                 */
                TC_NetWorkBuffer &getSendBuffer() { return _sendBuffer; }

                /**
                 * 发送buffer里面数据
                 * Send the data in the bufer
                 * @return
                 */
                int sendBuffer();

                /**
                 * 直接发送裸得应答数据，业务层一般不直接使用，仅仅tcp支持
                 * send naked response data
                 * @param buffer
                 * @return int, -1:发送出错, 0:无数据, 1:发送完毕, 2:还有数据
                 * @return int, -1: sending error, 0: no data, 1: send completely, 2: data retains
                 * @return
                 */
                int sendBufferDirect(const std::string& buff);

                /**
                 * 直接发送裸得应答数据，业务层一般不直接使用，仅仅tcp支持
                 * send naked response data
                 * @param buffer
                 * @return int, -1:发送出错, 0:无数据, 1:发送完毕, 2:还有数据
                 * @return int, -1: sending error, 0: no data, 1: send completely, 2: data retains
                 * @return
                 */
                int sendBufferDirect(const std::vector<char>& buff);

                /**
                 * 关闭连接
                 * Close the connection
                 * @param fd
                 */
                void close();

                friend class NetThread;

            protected:

                /**
                 * 添加发送buffer
                 * Add sanding buffer
                 * @param buffer
                 * @return int, -1:发送出错, 0:无数据, 1:发送完毕, 2:还有数据
                 * @return int, -1: sending error, 0: no data, 1: send completely, 2: data retains
                 */
                int send(const shared_ptr<SendContext> &data);

                /**
                 * 读取数据
                 * Read data
                 * @param fd
                 * @return int, -1:接收出错, 0:接收不全, 1:接收到一个完整包
                 * @return int, -1: received error, 0: not receive completely, 1: receive a complete package
                 */
                int recv();

                /**
                * 接收TCP
                * Receive TCP
                */
                int recvTcp();

                /**
                * 接收Udp
                * Receive UDP
                */
                int recvUdp();

                /**
                 * 解析协议
                 * Destruct protocol
                 * @param o
                 */
                int parseProtocol(TC_NetWorkBuffer &rbuf);

                /**
                 * 增加数据到队列中
                 * Add data to the queue
                 * @param vtRecvData
                 */
                void insertRecvQueue(const shared_ptr<RecvContext> &recv);

                /**
                 * 对于udp方式的连接，分配指定大小的接收缓冲区
                 * For udp-mode connections, allocate receive buffers of a specified size
                 *@param nSize
                    */
                bool setRecvBuffer(size_t nSize=DEFAULT_RECV_BUFFERSIZE);

                /**
                 * 是否是tcp连接
                 * Whether it is TCP connection.
                 * @return
                 */
                bool isTcp() const { return _lfd != -1; }

            public:
                /**
                 * 最后刷新时间
                 * Last refresh time
                 */
                time_t              _iLastRefreshTime;

            protected:

                /**
                 * 适配器
                 * Adapter
                 */
                BindAdapterPtr      _pBindAdapter;

                /**
                 * TC_Socket
                 */
                TC_Socket           _sock;

                /**
                 * 连接的唯一编号
                 * the unique id of the connection
                 */
                volatile uint32_t   _uid;

                /**
                 * 监听的socket
                 * Listening socket
                 */
                int                 _lfd;

                /**
                 * 超时时间
                 * Timeout
                 */
                int                 _timeout;

                /**
                 * ip
                 */
                string              _ip;

                /**
                 * 端口
                 * Port
                 */
                uint16_t             _port;

                /**
                 * 接收数据buffer
                 * the buffer to receive data
                 */
                TC_NetWorkBuffer     _recvBuffer;

                /**
                 * 发送数据buffer
                 * the buffer to send data
                 */
                TC_NetWorkBuffer    _sendBuffer;

                /**
                 * 发送数据
                 * Send data
                 */
                size_t              _sendBufferSize = 0;

                /**
                 * 检查时间
                 * Check time
                 */
                time_t              _lastCheckTime = 0;

                /**
                 * 发送的检查<已经发送数据, 剩余buffer大小>
                 * Check Sent <Data Sent, Remaining Buffer Size>
                 */
                vector<pair<size_t, size_t>> _checkSend;

                /**
                 * 需要过滤的头部字节数
                 * Number of header bytes to filter
                 */
                int                 _iHeaderLen;

                /**
                 * 发送完当前数据就关闭连接
                 * Close connection after sending current data
                 */
                bool                _bClose;

                /**
                 * 连接类型
                 * Connection Type
                 */
                EnumConnectionType  _enType;

                bool                _bEmptyConn;

                /*
                *接收数据的临时buffer,加这个目的是对udp接收数据包大小进行设置
                *Temporary buffer to receive data, plus this is to set the UDP receive packet size
                */
                char                *_pRecvBuffer = NULL;

                size_t              _nRecvBufferSize;

            public:
                /*
                *该连接的鉴权状态
                *Authentication status of the connection
                */
                int                 _authState;
                /*
                *该连接的鉴权状态是否初始化了
                */
                bool                _authInit;

//                std::shared_ptr<TC_OpenSSL> _openssl;
            };

    /**
     * 带有时间链表的map
     * Map with Time Chain Table
     */
    class ConnectionList
            {
            public:
                /**
                 * 构造函数
                 * Constructor
                 */
                ConnectionList(NetThread *pEpollServer);

                /**
                 * 析够函数
                 * Destructor
                 */
                ~ConnectionList()
                {
                    if(_vConn)
                    {
                        //服务停止时, 主动关闭一下连接, 这样客户端会检测到, 不需要等下一个发送包时, 发送失败才知道连接被关闭
                        for (auto it = _tl.begin(); it != _tl.end(); ++it) {
                            if (_vConn[it->second].first != NULL) {
                                _vConn[it->second].first->close();
                            }
                        }
                        delete[] _vConn;
                    }
                }

                /**
                 * 初始化大小
                 * Initial size
                 * @param size
                 */
                void init(uint32_t size, uint32_t iIndex = 0);

                /**
                 * 获取惟一ID
                 * Get the unique ID
                 *
                 * @return unsigned int
                 */
                uint32_t getUniqId();

                /**
                 * 添加连接
                 * Add Connection
                 * @param cPtr
                 * @param iTimeOutStamp
                 */
                void add(Connection *cPtr, time_t iTimeOutStamp);

                /**
                 * 刷新时间链
                 * Refresh the connectiom
                 * @param uid
                 * @param iTimeOutStamp, 超时时间点
                 * @param iTimeOutStamp, Timeout Point
                 */
                void refresh(uint32_t uid, time_t iTimeOutStamp);

                /**
                 * 检查超时数据
                 * Check Timeout
                 */
                void checkTimeout(time_t iCurTime);

                /**
                 * 获取某个监听端口的连接
                 * Get a connection to a listening port
                 * @param lfd
                 * @return vector<TC_EpollServer::ConnStatus>
                 */
                vector<ConnStatus> getConnStatus(int lfd);

                /**
                 * 获取某一个连接
                 * Get a certain connection
                 * @param p
                 * @return T
                 */
                Connection* get(uint32_t uid);

                /**
                 * 删除连接
                 * Delete connection
                 * @param uid
                 */
                void del(uint32_t uid);

                /**
                 * 大小
                 * Size
                 * @return size_t
                 */
                size_t size();

            protected:
                typedef pair<Connection*, multimap<time_t, uint32_t>::iterator> list_data;

                /**
                 * 内部删除, 不加锁
                 * Internal Delete, No Lock
                 * @param uid
                 */
                void _del(uint32_t uid);

            protected:
                /**
                 * 无锁
                 * No Lock
                 */
                TC_ThreadMutex                 _mutex;

                /**
                 * 服务
                 * Service
                 */
                NetThread                      *_pEpollServer;

                /**
                 * 总计连接数
                 * Total connection amount
                 */
                volatile uint32_t               _total;

                /**
                 * 空闲链表
                 * Empty link list
                 */
                list<uint32_t>                  _free;

                /**
                 * 空闲链元素个数
                 * number of the elements in the empty link
                 */
                volatile size_t                 _free_size;

                /**
                 * 链接
                 * Connection
                 */
                list_data                       *_vConn;

                /**
                 * 超时链表
                 * Timeout link list
                 */
                multimap<time_t, uint32_t>      _tl;

                /**
                 * 上次检查超时时间
                 * Last timeout time
                 */
                time_t                          _lastTimeoutTime;

                /**
                 * 链接ID的魔数
                 * Magic Number of Link IDs
                 */
                uint32_t                        _iConnectionMagic;
            };

} // xy
