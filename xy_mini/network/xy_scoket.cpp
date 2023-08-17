//
// Created by Administrator on 2023/8/2.
//

#include "xy_scoket.h"
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <cerrno>
#include <cassert>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

namespace xy {

TC_Socket::TC_Socket() : _sock(INVALID_SOCKET), _bOwner(true), _iDomain(AF_INET) {
}

TC_Socket::~TC_Socket() {
    if (_bOwner) {
        close();
    }
}

void TC_Socket::init(int fd, bool bOwner, int iDomain) {
    if (_bOwner) {
        close();
    }

    _sock = fd;
    _bOwner = bOwner;
    _iDomain = iDomain;
}

void TC_Socket::createSocket(int iSocketType, int iDomain) {
    // assert(iSocketType == SOCK_STREAM || iSocketType == SOCK_DGRAM);
    close();

    _iDomain = iDomain;
    _sock = socket(iDomain, iSocketType, 0);

    if (_sock < 0) {
        _sock = INVALID_SOCKET;
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::createSocket] create socket error");
        // throw TC_Socket_Exception("[TC_Socket::createSocket] create socket error! :" + string(strerror(errno)));
    } else {
//            ignoreSigPipe();
    }

}

void TC_Socket::close() {
    if (_sock != INVALID_SOCKET) {
        ::close(_sock);
        _sock = INVALID_SOCKET;
    }
}

void TC_Socket::bind(const char *sPathName) {
    assert(_iDomain == AF_LOCAL);

    unlink(sPathName);

    struct sockaddr_un stBindAddr;
    memset(&stBindAddr, 0x00, sizeof(stBindAddr));
    stBindAddr.sun_family = _iDomain;
    strncpy(stBindAddr.sun_path, sPathName, sizeof(stBindAddr.sun_path));

    bind((struct sockaddr *) &stBindAddr, sizeof(stBindAddr));
}

void TC_Socket::bind(const struct sockaddr *pstBindAddr, SOCKET_LEN_TYPE iAddrLen) {
    //如果服务器终止后,服务器可以第二次快速启动而不用等待一段时间
    int iReuseAddr = 1;

    setSockOpt(SO_REUSEADDR, (const void *) &iReuseAddr, sizeof(int), SOL_SOCKET);

    if (::bind(_sock, pstBindAddr, iAddrLen) < 0) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::bind] bind error");
    }
}

void TC_Socket::bind(const string &sServerAddr, int port) {
    assert(_iDomain == AF_INET || _iDomain == AF_INET6);

    struct sockaddr_in6 bindAddr6;
    struct sockaddr_in bindAddr4;
    struct sockaddr *bindAddr = (AF_INET6 == _iDomain) ? (struct sockaddr *) &bindAddr6
                                                       : (struct sockaddr *) &bindAddr4;
    socklen_t len = (AF_INET6 == _iDomain) ? sizeof(bindAddr6) : sizeof(bindAddr4);

    bzero(bindAddr, len);

    if (AF_INET6 == _iDomain) {
        parseAddrWithPort(sServerAddr, port, bindAddr6);
    } else {
        parseAddrWithPort(sServerAddr, port, bindAddr4);
    }

    bind(bindAddr, len);
}

void TC_Socket::connect(const char *sPathName) {
    int ret = connectNoThrow(sPathName);
    if (ret < 0) {
        THROW_EXCEPTION_SYSCODE(TC_SocketConnect_Exception, "[TC_Socket::connect] connect error");
    }
}

int TC_Socket::connectNoThrow(const char *sPathName) {
    assert(_iDomain == AF_LOCAL);

    struct sockaddr_un stServerAddr;
    memset(&stServerAddr, 0x00, sizeof(stServerAddr));
    stServerAddr.sun_family = _iDomain;
    strncpy(stServerAddr.sun_path, sPathName, sizeof(stServerAddr.sun_path));

    return connect((struct sockaddr *) &stServerAddr, sizeof(stServerAddr));
}

void TC_Socket::connect(const string &sServerAddr, uint16_t port) {
    int ret = connectNoThrow(sServerAddr, port);

    if (ret < 0) {
        THROW_EXCEPTION_SYSCODE(TC_SocketConnect_Exception, "[TC_Socket::connect] connect error");
    }
}

int TC_Socket::connectNoThrow(const string &sServerAddr, uint16_t port) {
    assert(_iDomain == AF_INET || _iDomain == AF_INET6);

    if (sServerAddr == "") {
        throw TC_Socket_Exception("[TC_Socket::connect] server address is empty error!");
    }

    struct sockaddr_in6 serverAddr6;
    struct sockaddr_in serverAddr4;
    struct sockaddr *serverAddr = (AF_INET6 == _iDomain) ? (struct sockaddr *) &serverAddr6
                                                         : (struct sockaddr *) &serverAddr4;
    socklen_t len = (AF_INET6 == _iDomain) ? sizeof(serverAddr6) : sizeof(serverAddr4);

    bzero(serverAddr, len);

    if (AF_INET6 == _iDomain) {
        serverAddr6.sin6_family = _iDomain;
        parseAddr(sServerAddr, serverAddr6.sin6_addr);
        serverAddr6.sin6_port = htons(port);
    } else {
        serverAddr4.sin_family = _iDomain;
        parseAddr(sServerAddr, serverAddr4.sin_addr);
        serverAddr4.sin_port = htons(port);
    }

    return connect(serverAddr, len);
}


int TC_Socket::connect(const struct sockaddr *pstServerAddr, SOCKET_LEN_TYPE serverLen) {
    return ::connect(_sock, pstServerAddr, serverLen);
}

SOCKET_TYPE TC_Socket::accept(TC_Socket &tcSock, struct sockaddr *pstSockAddr, SOCKET_LEN_TYPE &iSockLen) {
    assert(tcSock._sock == INVALID_SOCKET);
    SOCKET_TYPE ifd;
    while ((ifd = ::accept(_sock, pstSockAddr, &iSockLen)) < 0 && errno == EINTR);
    tcSock._sock = ifd;
    tcSock._iDomain = _iDomain;

    return tcSock._sock;
}

void TC_Socket::listen(int iConnBackLog) {
    if (::listen(_sock, iConnBackLog) < 0) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::listen] listen error");
    }
}

int TC_Socket::recv(void *pvBuf, size_t iLen, int iFlag) {
    return ::recv(_sock, (char *) pvBuf, (int) iLen, iFlag);
}

int TC_Socket::send(const void *pvBuf, size_t iLen, int iFlag) {
    return ::send(_sock, (char *) pvBuf, (int) iLen, iFlag);
}

int TC_Socket::recvfrom(void *pvBuf, size_t iLen, string &sFromAddr, uint16_t &iFromPort, int iFlags) {
    int iBytes;
    struct sockaddr_in6 stFromAddr6;
    struct sockaddr_in stFromAddr4;
    struct sockaddr *stFromAddr = (AF_INET6 == _iDomain) ? (struct sockaddr *) &stFromAddr6
                                                         : (struct sockaddr *) &stFromAddr4;
    socklen_t iFromLen = (AF_INET6 == _iDomain) ? sizeof(stFromAddr6) : sizeof(stFromAddr4);

    bzero(stFromAddr, iFromLen);
    iBytes = recvfrom(pvBuf, iLen, stFromAddr, iFromLen, iFlags);
    if (iBytes >= 0) {
        char sAddr[INET6_ADDRSTRLEN] = "\0";
        inet_ntop(_iDomain,
                  (AF_INET6 == _iDomain) ? (void *) &stFromAddr6.sin6_addr : (void *) &stFromAddr4.sin_addr, sAddr,
                  sizeof(sAddr));
        sFromAddr = sAddr;
        iFromPort = (AF_INET6 == _iDomain) ? ntohs(stFromAddr6.sin6_port) : ntohs(stFromAddr4.sin_port);
    }
    return iBytes;
}

int
TC_Socket::recvfrom(void *pvBuf, size_t iLen, struct sockaddr *pstFromAddr, SOCKET_LEN_TYPE &iFromLen, int iFlags) {
    return ::recvfrom(_sock, (char *) pvBuf, (int) iLen, iFlags, pstFromAddr, &iFromLen);
}

int TC_Socket::sendto(const void *pvBuf, size_t iLen, const string &sToAddr, uint16_t port, int iFlags) {
    struct sockaddr_in6 toAddr6;
    struct sockaddr_in toAddr4;
    struct sockaddr *toAddr = (AF_INET6 == _iDomain) ? (struct sockaddr *) &toAddr6 : (struct sockaddr *) &toAddr4;
    socklen_t len = (AF_INET6 == _iDomain) ? sizeof(toAddr6) : sizeof(toAddr4);

    bzero(toAddr, len);
    if (AF_INET6 == _iDomain) {
        toAddr6.sin6_family = _iDomain;

        if (sToAddr == "") {
            //toAddr.sin6_addr = in6addr_linklocal_allrouters;
        } else {
            parseAddr(sToAddr, toAddr6.sin6_addr);
        }
        toAddr6.sin6_port = htons(port);
    } else {
        toAddr4.sin_family = _iDomain;

        if (sToAddr == "") {
            toAddr4.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        } else {
            parseAddr(sToAddr, toAddr4.sin_addr);
        }

        toAddr4.sin_port = htons(port);
    }

    return sendto(pvBuf, iLen, toAddr, len, iFlags);
}

int
TC_Socket::sendto(const void *pvBuf, size_t iLen, struct sockaddr *pstToAddr, SOCKET_LEN_TYPE iToLen, int iFlags) {
    return ::sendto(_sock, (char *) pvBuf, (int) iLen, iFlags, pstToAddr, iToLen);
}

void TC_Socket::shutdown(int iHow) {
    if (::shutdown(_sock, iHow) < 0) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::shutdown] shutdown error");
    }
}

void TC_Socket::setblock(bool bBlock) {
    assert(_sock != INVALID_SOCKET);

    setblock(_sock, bBlock);
}

int TC_Socket::setSockOpt(int opt, const void *pvOptVal, SOCKET_LEN_TYPE optLen, int level) {
    return setsockopt(_sock, level, opt, (const char *) pvOptVal, optLen);
}

int TC_Socket::getSockOpt(int opt, void *pvOptVal, SOCKET_LEN_TYPE &optLen, int level) const {
    return getsockopt(_sock, level, opt, (char *) pvOptVal, &optLen);
}

void TC_Socket::setNoCloseWait() {
    linger stLinger;
    stLinger.l_onoff = 1;  //在close socket调用后, 但是还有数据没发送完毕的时候容许逗留
    stLinger.l_linger = 0; //容许逗留的时间为0秒

    if (setSockOpt(SO_LINGER, (const void *) &stLinger, sizeof(linger), SOL_SOCKET) == -1) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::setNoCloseWait] error");
    }
}

void TC_Socket::setCloseWait(int delay) {
    linger stLinger;
    stLinger.l_onoff = 1;  //在close socket调用后, 但是还有数据没发送完毕的时候容许逗留
    stLinger.l_linger = delay; //容许逗留的时间为delay秒

    if (setSockOpt(SO_LINGER, (const void *) &stLinger, sizeof(linger), SOL_SOCKET) == -1) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::setCloseWait] error");

        // throw TC_Socket_Exception("[TC_Socket::setCloseWait] error", TC_Exception::getSystemCode());
    }
}

void TC_Socket::setCloseWaitDefault() {
    linger stLinger;
    stLinger.l_onoff = 0;
    stLinger.l_linger = 0;

    if (setSockOpt(SO_LINGER, (const void *) &stLinger, sizeof(linger), SOL_SOCKET) == -1) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::setCloseWaitDefault] error");
    }
}

void TC_Socket::setTcpNoDelay() {
    int flag = 1;

    if (setSockOpt(TCP_NODELAY, (char *) &flag, int(sizeof(int)), IPPROTO_TCP) == -1) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::setTcpNoDelay] error");
    }
}

void TC_Socket::setKeepAlive() {
    int flag = 1;
    if (setSockOpt(SO_KEEPALIVE, (char *) &flag, int(sizeof(int)), SOL_SOCKET) == -1) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::setKeepAlive] error");
    }
}

void TC_Socket::setblock(SOCKET_TYPE fd, bool bBlock) {
    int val = 0;

    if ((val = fcntl(fd, F_GETFL, 0)) == -1) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::setblock] fcntl [F_GETFL] error");
    }

    if (!bBlock) {
        val |= O_NONBLOCK;
    } else {
        val &= ~O_NONBLOCK;
    }

    if (fcntl(fd, F_SETFL, val) == -1) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::setblock] fcntl [F_SETFL] error");
    }
}

bool TC_Socket::isPending() {
    return TC_Exception::getSystemCode() == EAGAIN;
}

bool TC_Socket::isInProgress() {
    return TC_Exception::getSystemCode() == EINPROGRESS;
}

void TC_Socket::parseAddr(const string &host, struct in6_addr &stSinAddr) {
    int iRet = inet_pton(AF_INET6, host.c_str(), &stSinAddr);
    if (iRet < 0) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::parseAddr] inet_pton(" + host + ") error");
    } else if (iRet == 0) {

        struct addrinfo *info = 0;
        int retry = 5;

        struct addrinfo hints = {0};
        hints.ai_family = AF_INET6;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
        hints.ai_protocol = IPPROTO_TCP;

        int rs = 0;
        do {
            rs = getaddrinfo(host.c_str(), 0, &hints, &info);
        } while (info == 0 && rs == EAI_AGAIN && --retry >= 0);

        if (rs != 0) {
            std::ostringstream os;
            os << "DNSException ex:(" << strerror(errno) << ")" << rs << ":" << host << ":" << __FILE__ << ":"
               << __LINE__;
            if (info != NULL) {
                freeaddrinfo(info);
            }
            throw TC_Socket_Exception(os.str());
        }

        assert(info != NULL);

        memcpy(&stSinAddr, info->ai_addr, sizeof(stSinAddr));

        freeaddrinfo(info);
    }
}

void TC_Socket::parseAddr(const string &sAddr, struct in_addr &stSinAddr) {
    int iRet = inet_pton(AF_INET, sAddr.c_str(), &stSinAddr);
    if (iRet < 0) {
        THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::parseAddr] inet_pton(" + sAddr + ") error");
    } else if (iRet == 0) {
        struct hostent stHostent;
        struct hostent *pstHostent;
        char buf[2048] = "\0";
        int iError;

        gethostbyname_r(sAddr.c_str(), &stHostent, buf, sizeof(buf), &pstHostent, &iError);

        if (pstHostent == NULL) {
            THROW_EXCEPTION_SYSCODE(TC_Socket_Exception,
                                    "[TC_Socket::parseAddr] gethostbyname_r(" + sAddr + ") error");

            // throw TC_Socket_Exception("[TC_Socket::parseAddr] gethostbyname_r(" + sAddr + ") error", TC_Exception::getSystemCode());
        } else {
            stSinAddr = *(struct in_addr *) pstHostent->h_addr;
        }
    }
}

void TC_Socket::parseAddrWithPort(const string &host, int port, struct sockaddr_in &addr) {
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (host == "" || host == "0.0.0.0" || host == "*") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        parseAddr(host, addr.sin_addr);
    }
}


void TC_Socket::parseAddrWithPort(const string &host, int port, struct sockaddr_in6 &addr) {
    memset(&addr, 0, sizeof(struct sockaddr_in6));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    if (host == "") {
        addr.sin6_addr = in6addr_any;
    } else {
        parseAddr(host, addr.sin6_addr);
    }
}


} // xy
