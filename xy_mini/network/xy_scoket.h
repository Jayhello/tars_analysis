//
// Created by Administrator on 2023/8/2.
//

#pragma once

#include <string>
#include "util/xy_ec.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <vector>
#include <string>

typedef int SOCKET_TYPE;
typedef socklen_t SOCKET_LEN_TYPE;

#define INVALID_SOCKET -1

namespace xy {

struct TC_Socket_Exception : public TC_Exception {
    TC_Socket_Exception(const string &buffer) : TC_Exception(buffer) {};

    TC_Socket_Exception(const string &buffer, int err) : TC_Exception(buffer, err) {};

    ~TC_Socket_Exception() throw() {};
};

struct TC_SocketConnect_Exception : public TC_Socket_Exception {
    TC_SocketConnect_Exception(const string &buffer) : TC_Socket_Exception(buffer) {};

    TC_SocketConnect_Exception(const string &buffer, int err) : TC_Socket_Exception(buffer, err) {};

    ~TC_SocketConnect_Exception() throw() {};
};

class TC_Socket {
public:
    TC_Socket();

    virtual ~TC_Socket();

    void init(int fd, bool bOwner, int iDomain = AF_INET);

    void setOwner(bool bOwner) { _bOwner = bOwner; }

    void setDomain(int iDomain) { _iDomain = iDomain; }

    // 生成socket, 如果已经存在以前的socket, 则释放掉, 生成新的.
    void createSocket(int iSocketType = SOCK_STREAM, int iDomain = AF_INET);

    SOCKET_TYPE getfd() const { return _sock; }

    bool isValid() const { return _sock != INVALID_SOCKET; }

    void close();

    // 获取对点的ip和端口,对AF_INET的socket有效.
    void getPeerName(string &sPeerAddress, uint16_t &iPeerPort) const;

    void getPeerName(string &sPathName) const;

    void bind(const char *sPathName);

    void bind(const string &sServerAddr, int port);

    void bind(const struct sockaddr *pstBindAddr, SOCKET_LEN_TYPE iAddrLen);

    void connect(const char *sPathName);

    int connect(const struct sockaddr *pstServerAddr, SOCKET_LEN_TYPE serverLen);

    // 发起连接，连接失败的状态不通过异常返回,通过connect的返回值,在异步连接的时候需要
    int connectNoThrow(const char *sPathName);

    int setSockOpt(int opt, const void *pvOptVal, SOCKET_LEN_TYPE optLen, int level = SOL_SOCKET);

    int getSockOpt(int opt, void *pvOptVal, SOCKET_LEN_TYPE &optLen, int level = SOL_SOCKET) const;

    SOCKET_TYPE accept(TC_Socket &tcSock, struct sockaddr *pstSockAddr, SOCKET_LEN_TYPE &iSockLen);

    void connect(const string &sServerAddr, uint16_t port);

    // 发起连接，连接失败的状态不通过异常返回, 通过connect的返回值,在异步连接的时候需要
    int connectNoThrow(const string &sServerAddr, uint16_t port);

    void listen(int connBackLog);

    int recv(void *pvBuf, size_t iLen, int iFlag = 0);

    int send(const void *pvBuf, size_t iLen, int iFlag = 0);

    int recvfrom(void *pvBuf, size_t iLen, string &sFromAddr, uint16_t &iFromPort, int iFlags = 0);

    int recvfrom(void *pvBuf, size_t iLen, struct sockaddr *pstFromAddr, SOCKET_LEN_TYPE &iFromLen, int iFlags = 0);

    int sendto(const void *pvBuf, size_t iLen, const string &sToAddr, uint16_t iToPort, int iFlags = 0);

    int sendto(const void *pvBuf, size_t iLen, struct sockaddr *pstToAddr, SOCKET_LEN_TYPE iToLen, int iFlags = 0);

    // iHow 关闭方式:SHUT_RD|SHUT_WR|SHUT_RDWR
    void shutdown(int iHow);

    void setblock(bool bBlock = false);

    void setNoCloseWait();

    void setCloseWait(int delay = 30);

    void setCloseWaitDefault();

    void setTcpNoDelay();

    void setKeepAlive();

    static void setblock(SOCKET_TYPE fd, bool bBlock);

    // 判断当前socket是否处于EAGAIN/WSAEWOULDBLOCK(异步send/recv函数返回值时判断)
    static bool isPending();

    // 判断当前socket是否处于EINPROGRESS/WSAEWOULDBLOC(异步connect返回值时判断)
    static bool isInProgress();

    // 解析地址, 从字符串(ip或域名), 解析到in_addr结构.
    static void parseAddr(const string &sAddr, struct in_addr &stAddr);

    // 解析地址, 从字符串(ipv6或域名), 解析到in6_addr结构.
    static void parseAddr(const string &sAddr, struct in6_addr &stAddr);

    // 解析地址, 从字符串(ip或域名)端口, 解析到sockaddr_in结构.
    static void parseAddrWithPort(const string &host, int port, struct sockaddr_in &addr);

    static void parseAddrWithPort(const string &host, int port, struct sockaddr_in6 &addr);

protected:
    SOCKET_TYPE _sock;
    bool _bOwner;
    int _iDomain;  //   socket类型
};

} // xy