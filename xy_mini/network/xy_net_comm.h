//
// Created by wenwen on 2023/8/27.
//
#pragma once
#include "util/xy_autoptr.h"
#include "util/xy_thread_queue.h"
#include "util/xy_timeprovider.h"

namespace xy{

class TC_EpollServer;
class ConnStatus;

class BindAdapter;
typedef xy::TC_AutoPtr<BindAdapter> BindAdapterPtr;

class RecvContext;
class SendContext;

typedef TC_ThreadQueue<shared_ptr<RecvContext>> recv_queue;
typedef TC_ThreadQueue<shared_ptr<SendContext>> send_queue;

class Connection;
class ConnectionList;
class NetThread;

class Handle;
typedef TC_AutoPtr<Handle> HandlePtr;

enum EM_CLOSE_T {
    /**Client active shutdown*/
    EM_CLIENT_CLOSE = 0,         //客户端主动关闭
    /**The service-side business proactively calls 'close' to close the connection,
     * or the framework proactively closes the connection due to an exception.*/
    EM_SERVER_CLOSE = 1,        //服务端业务主动调用close关闭连接,或者框架因某种异常主动关闭连接
    /**Connection timed out, server actively closed*/
    EM_SERVER_TIMEOUT_CLOSE = 2  //连接超时了，服务端主动关闭
};

enum {
    /**Empty connection timeout (ms)*/
    MIN_EMPTY_CONN_TIMEOUT = 2 * 1000,    /*空链接超时时间(ms)*/
    /**The size of received buffer of the default data*/
    DEFAULT_RECV_BUFFERSIZE = 64 * 1024    /*缺省数据接收buffer的大小*/
};

//定义加入到网络线程的fd类别
//Define the FD categories added to network threads
enum CONN_TYPE {
    TCP_CONNECTION = 0,
    UDP_CONNECTION = 1,
};

/**
 * 定义协议解析接口的操作对象
 * 注意必须是线程安全的或是可以重入的
 * Define the Operating Object of the Protocol Resolution Interface
 * Note that it must be thread safe or reentrant
 */
typedef std::function<TC_NetWorkBuffer::PACKET_TYPE(TC_NetWorkBuffer::PACKET_TYPE,
                                                        vector<char> &)> header_filter_functor;

} //xy
