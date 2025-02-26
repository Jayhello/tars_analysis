//
// Created by Administrator on 2023/8/18.
//

#include "xy_clientsocket.h"
#include <cerrno>
#include <iostream>
#include "util/logging.h"

namespace xy {

TC_Endpoint::TC_Endpoint() {
    _host = "0.0.0.0";
    _port = 0;
    _timeout = 3000;
    _type = TCP;
    _grid = 0;
    _qos = 0;
    _weight = -1;
    _weighttype = 0;
    _authType = 0;
    _isIPv6 = TC_Socket::addressIsIPv6(_host);
}

void TC_Endpoint::init(const string &host, int port, int timeout, EType type, int grid, int qos, int weight,
                       unsigned int weighttype, int authType) {
    _host = host;
    _port = port;
    _timeout = timeout;
    _type = type;
    _grid = grid;
    _qos = qos;
    if (weighttype == 0) {
        _weight = -1;
        _weighttype = 0;
    } else {
        if (weight == -1) {
            weight = 100;
        }
        _weight = (weight > 100 ? 100 : weight);
        _weighttype = weighttype;
    }

    _authType = authType;

    _isIPv6 = TC_Socket::addressIsIPv6(_host);
}

void TC_Endpoint::parse(const string &str) {
    _grid = 0;
    _qos = 0;
    _weight = -1;
    _weighttype = 0;
    _authType = 0;

    const string delim = " \t\n\r";

    string::size_type beg;
    string::size_type end = 0;

    beg = str.find_first_not_of(delim, end);
    if (beg == string::npos) {
        throw TC_EndpointParse_Exception("TC_Endpoint::parse error : " + str);
    }

    end = str.find_first_of(delim, beg);
    if (end == string::npos) {
        end = str.length();
    }

    string desc = str.substr(beg, end - beg);
    if (desc == "tcp") {
        _type = TCP;
    } else if (desc == "ssl") {
        _type = SSL;
    } else if (desc == "udp") {
        _type = UDP;
    } else {
        throw TC_EndpointParse_Exception("TC_Endpoint::parse tcp or udp or ssl error : " + str);
    }

    desc = str.substr(end);
    end = 0;
    while (true) {
        beg = desc.find_first_not_of(delim, end);
        if (beg == string::npos) {
            break;
        }

        end = desc.find_first_of(delim, beg);
        if (end == string::npos) {
            end = desc.length();
        }

        string option = desc.substr(beg, end - beg);
        if (option.length() != 2 || option[0] != '-') {
            throw TC_EndpointParse_Exception("TC_Endpoint::parse error : " + str);
        }

        string argument;
        string::size_type argumentBeg = desc.find_first_not_of(delim, end);
        if (argumentBeg != string::npos && desc[argumentBeg] != '-') {
            beg = argumentBeg;
            end = desc.find_first_of(delim, beg);
            if (end == string::npos) {
                end = desc.length();
            }
            argument = desc.substr(beg, end - beg);
        }

        switch (option[1]) {
            case 'h': {
                if (argument.empty()) {
                    throw TC_EndpointParse_Exception("TC_Endpoint::parse -h error : " + str);
                }
                const_cast<string &>(_host) = argument;
                break;
            }
            case 'p': {
                istringstream p(argument);
                if (!(p >> const_cast<int &>(_port)) || !p.eof() || _port < 0 || _port > 65535) {
                    throw TC_EndpointParse_Exception("TC_Endpoint::parse -p error : " + str);
                }
                break;
            }
            case 't': {
                istringstream t(argument);
                if (!(t >> const_cast<int &>(_timeout)) || !t.eof()) {
                    throw TC_EndpointParse_Exception("TC_Endpoint::parse -t error : " + str);
                }
                break;
            }
            case 'g': {
                istringstream t(argument);
                if (!(t >> const_cast<int &>(_grid)) || !t.eof()) {
                    throw TC_EndpointParse_Exception("TC_Endpoint::parse -g error : " + str);
                }
                break;
            }
            case 'q': {
                istringstream t(argument);
                if (!(t >> const_cast<int &>(_qos)) || !t.eof()) {
                    throw TC_EndpointParse_Exception("TC_Endpoint::parse -q error : " + str);
                }
                break;
            }
            case 'w': {
                istringstream t(argument);
                if (!(t >> const_cast<int &>(_weight)) || !t.eof()) {
                    throw TC_EndpointParse_Exception("TC_Endpoint::parse -w error : " + str);
                }
                break;
            }
            case 'v': {
                istringstream t(argument);
                if (!(t >> const_cast<unsigned int &>(_weighttype)) || !t.eof()) {
                    throw TC_EndpointParse_Exception("TC_Endpoint::parse -v error : " + str);
                }
                break;
            }
                // auth type
            case 'e': {
                istringstream p(argument);
                if (!(p >> const_cast<int &>(_authType)) || !p.eof() || _authType < 0 || _authType > 1) {
                    throw TC_EndpointParse_Exception("TC_Endpoint::parse -e error : " + str);
                }
                break;
            }
            default: {
                ///throw TC_EndpointParse_Exception("TC_Endpoint::parse error : " + str);
            }
        }
    }

    if (_weighttype != 0) {
        if (_weight == -1) {
            _weight = 100;
        }

        _weight = (_weight > 100 ? 100 : _weight);
    }

    if (_host.empty()) {
        throw TC_EndpointParse_Exception("TC_Endpoint::parse error : host must not be empty: " + str);
    } else if (_host == "*") {
        const_cast<string &>(_host) = "0.0.0.0";
    }
    _isIPv6 = TC_Socket::addressIsIPv6(_host);

    if (_authType < 0)
        _authType = 0;
    else if (_authType > 0)
        _authType = 1;
}

/*************************************TC_TCPClient**************************************/

TC_ClientSocket::TC_ClientSocket() : _port(0), _timeout(3000) {
}

TC_ClientSocket::~TC_ClientSocket() {
    if (_epoller) {
        _epoller->close();
        delete _epoller;
        _epoller = NULL;
    }
}

void TC_ClientSocket::init(const string &sIp, int iPort, int iTimeout) {
    if (!_epoller) {
        _epoller = new TC_Epoller();
        _epoller->create(10);
        _epoller->enableET(false);
    }

    _socket.close();
    _ip = sIp;
    _port = iPort;
    _timeout = iTimeout;
    _isIPv6 = TC_Socket::addressIsIPv6(sIp);
}

void TC_ClientSocket::close() {
    _socket.close();
}

/*************************************TC_TCPClient**************************************/

#define LEN_MAXRECV 8196

int TC_TCPClient::checkSocket() {
    if (!_socket.isValid()) {
        ScopeLog Log;
        try {
#if TARGET_PLATFORM_LINUX || TARGET_PLATFORM_IOS
            _socket.createSocket(SOCK_STREAM, _port ? (_isIPv6 ? AF_INET6 : AF_INET) : AF_LOCAL);
            Log << "create_socket_fd" << _socket.getfd();
#else
            _socket.createSocket(SOCK_STREAM, _isIPv6 ? AF_INET6 : AF_INET);
#endif

            _epoller->add(_socket.getfd(), 0, EPOLLOUT | EPOLLIN);

            //设置非阻塞模式
            _socket.setblock(false);
            _socket.setNoCloseWait();

            int iRet;

#if TARGET_PLATFORM_LINUX

            if (_port == 0) {
                iRet = _socket.connectNoThrow(_ip.c_str());
            } else
#endif
            {
                iRet = _socket.connectNoThrow(_ip, _port);
            }

            Log << "connect_ret" << iRet;
            if (iRet < 0 && !TC_Socket::isInProgress()) {
                Log << "fail";
                _socket.close();
                return EM_CONNECT;
            }
            int iRetCode = _epoller->wait(_timeout);
            Log << "epoll_wait_ret" << iRetCode;
            if (iRetCode < 0) {
                Log << "fail_close";
                _socket.close();
                return EM_SELECT;
            } else if (iRetCode == 0) {
                Log << "timeout_close";
                _socket.close();
                return EM_TIMEOUT;
            } else {
#if !TARGET_PLATFORM_WINDOWS
                for (int i = 0; i < iRetCode; ++i) {
                    const epoll_event &ev = _epoller->get(i);
                    if (TC_Epoller::errorEvent(ev)) {
                        Log << "err_event_close";
                        _socket.close();
                        return EM_CONNECT;
                    } else {
                        int iVal = 0;
                        socklen_t iLen = static_cast<socklen_t>(sizeof(int));
                        if (_socket.getSockOpt(SO_ERROR, reinterpret_cast<char *>(&iVal), iLen) == -1 || iVal) {
                            _socket.close();
                            return EM_CONNECT;
                        }
                    }
                }
#endif

            }
            //设置为阻塞模式
            _socket.setblock(true);
            Log << "succ";
        }
        catch (TC_Socket_Exception &ex) {
            _socket.close();
            return EM_SOCKET;
        }
    }
    return EM_SUCCESS;
}

int TC_TCPClient::send(const char *sSendBuffer, size_t iSendLen) {
    int iRet = checkSocket();
    if (iRet < 0) {
        return iRet;
    }

    iRet = _socket.send(sSendBuffer, iSendLen);
    if (iRet < 0) {
        _socket.close();
        return EM_SEND;
    }

    return EM_SUCCESS;
}

int TC_TCPClient::recv(char *sRecvBuffer, size_t &iRecvLen) {
    ScopeLog Log;
    int iRet = checkSocket();
    Log << "check_ret" << iRet;
    if (iRet < 0) {
        return iRet;
    }

    _epoller->mod(_socket.getfd(), 0, EPOLLIN);

    int iRetCode = _epoller->wait(_timeout);
    Log << "wait_ret" << iRetCode;

    if (iRetCode < 0) {
        Log << "fail_close";
        _socket.close();
        return EM_SELECT;
    } else if (iRetCode == 0) {
        Log << "timeout_close";
        _socket.close();
        return EM_TIMEOUT;
    }

    const epoll_event &ev = _epoller->get(0);
    if (TC_Epoller::errorEvent(ev)) {
        Log << "err_event_close";
        _socket.close();
        return EM_CLOSE;
    }
#if TARGET_PLATFORM_IOS || TARGET_PLATFORM_LINUX
    else
#else
        else if(TC_Epoller::readEvent(ev))
#endif
    {
        int iLen = _socket.recv((void *) sRecvBuffer, iRecvLen);
        Log << "recv_len" << iLen;
        if (iLen < 0) {
            Log << "fail_close";
            _socket.close();
            return EM_RECV;
        } else if (iLen == 0) {
            Log << "close";
            _socket.close();
            return EM_CLOSE;
        }

        iRecvLen = iLen;
        return EM_SUCCESS;
    }

    return EM_SELECT;
}

int TC_TCPClient::recvBySep(string &sRecvBuffer, const string &sSep) {
    sRecvBuffer.clear();

    int iRet = checkSocket();
    if (iRet < 0) {
        return iRet;
    }

    _epoller->mod(_socket.getfd(), 0, EPOLLIN);

    while (true) {
        int iRetCode = _epoller->wait(_timeout);
        if (iRetCode < 0) {
            _socket.close();
            return EM_SELECT;
        } else if (iRetCode == 0) {
            _socket.close();
            return EM_TIMEOUT;
        }

        const epoll_event &ev = _epoller->get(0);
        if (TC_Epoller::errorEvent(ev)) {
            _socket.close();
            return EM_CLOSE;
        }
#if TARGET_PLATFORM_IOS
            else
#else
        else if (TC_Epoller::readEvent(ev))
#endif
        {
            char buffer[LEN_MAXRECV] = "\0";

            int len = _socket.recv((void *) &buffer, sizeof(buffer));
            if (len < 0) {
                _socket.close();
                return EM_RECV;
            } else if (len == 0) {
                _socket.close();
                return EM_CLOSE;
            }

            sRecvBuffer.append(buffer, len);

            if (sRecvBuffer.length() >= sSep.length()
                && sRecvBuffer.compare(sRecvBuffer.length() - sSep.length(), sSep.length(), sSep) == 0) {
                break;
            }
        }
    }

    return EM_SUCCESS;
}

int TC_TCPClient::recvAll(string &sRecvBuffer) {
    sRecvBuffer.clear();

    int iRet = checkSocket();
    if (iRet < 0) {
        return iRet;
    }

    _epoller->mod(_socket.getfd(), 0, EPOLLIN);

    while (true) {
        int iRetCode = _epoller->wait(_timeout);
        if (iRetCode < 0) {
            _socket.close();
            return EM_SELECT;
        } else if (iRetCode == 0) {
            _socket.close();
            return EM_TIMEOUT;
        }

        const epoll_event &ev = _epoller->get(0);
        if (TC_Epoller::errorEvent(ev)) {
            _socket.close();
            return EM_CLOSE;
        }
#if TARGET_PLATFORM_IOS
            else
#else
        else if (TC_Epoller::readEvent(ev))
#endif
        {
            char sTmpBuffer[LEN_MAXRECV] = "\0";

            int len = _socket.recv((void *) sTmpBuffer, LEN_MAXRECV);
            if (len < 0) {
                _socket.close();
                return EM_RECV;
            } else if (len == 0) {
                _socket.close();
                return EM_SUCCESS;
            }

            sRecvBuffer.append(sTmpBuffer, len);
        }
    }

    return EM_SUCCESS;
}

int TC_TCPClient::recvLength(char *sRecvBuffer, size_t iRecvLen) {
    int iRet = checkSocket();
    if (iRet < 0) {
        return iRet;
    }

    size_t iRecvLeft = iRecvLen;
    iRecvLen = 0;

    _epoller->mod(_socket.getfd(), 0, EPOLLIN);

    while (iRecvLeft != 0) {
        int iRetCode = _epoller->wait(_timeout);
        if (iRetCode < 0) {
            _socket.close();
            return EM_SELECT;
        } else if (iRetCode == 0) {
            _socket.close();
            return EM_TIMEOUT;
        }

        const epoll_event &ev = _epoller->get(0);
        if (TC_Epoller::errorEvent(ev)) {
            _socket.close();
            return EM_CLOSE;
        }
#if TARGET_PLATFORM_IOS
            else
#else
        else if (TC_Epoller::readEvent(ev))
#endif
        {
            int len = _socket.recv((void *) (sRecvBuffer + iRecvLen), iRecvLeft);
            if (len < 0) {
                _socket.close();
                return EM_RECV;
            } else if (len == 0) {
                _socket.close();
                return EM_CLOSE;
            }

            iRecvLeft -= len;
            iRecvLen += len;
        }
    }

    return EM_SUCCESS;
}

int TC_TCPClient::sendRecv(const char *sSendBuffer, size_t iSendLen, char *sRecvBuffer, size_t &iRecvLen) {
    int iRet = send(sSendBuffer, iSendLen);
    if (iRet != EM_SUCCESS) {
        return iRet;
    }

    return recv(sRecvBuffer, iRecvLen);
}

int TC_TCPClient::sendRecvBySep(const char *sSendBuffer, size_t iSendLen, string &sRecvBuffer, const string &sSep) {
    int iRet = send(sSendBuffer, iSendLen);
    if (iRet != EM_SUCCESS) {
        return iRet;
    }

    return recvBySep(sRecvBuffer, sSep);
}

int TC_TCPClient::sendRecvLine(const char *sSendBuffer, size_t iSendLen, string &sRecvBuffer) {
    return sendRecvBySep(sSendBuffer, iSendLen, sRecvBuffer, "\r\n");
}


int TC_TCPClient::sendRecvAll(const char *sSendBuffer, size_t iSendLen, string &sRecvBuffer) {
    int iRet = send(sSendBuffer, iSendLen);
    if (iRet != EM_SUCCESS) {
        return iRet;
    }

    return recvAll(sRecvBuffer);
}

/*************************************TC_UDPClient**************************************/

int TC_UDPClient::checkSocket() {
    if (!_socket.isValid()) {
        try {
#if TARGET_PLATFORM_LINUX || TARGET_PLATFORM_IOS
            _socket.createSocket(SOCK_DGRAM, _port ? (_isIPv6 ? AF_INET6 : AF_INET) : AF_LOCAL);
#else
            _socket.createSocket(SOCK_DGRAM, _isIPv6 ? AF_INET6 : AF_INET);
#endif
        }
        catch (TC_Socket_Exception &ex) {
            _socket.close();
            return EM_SOCKET;
        }

        _epoller->add(_socket.getfd(), 0, EPOLLIN | EPOLLOUT);

        try {
#if TARGET_PLATFORM_LINUX

            if (_port == 0) {
                _socket.connect(_ip.c_str());
                if (_port == 0) {
                    _socket.bind(_ip.c_str());
                }
            } else
#endif
            {
                _socket.connect(_ip, _port);
            }
        }
        catch (TC_SocketConnect_Exception &ex) {
            _socket.close();
            return EM_CONNECT;
        }
        catch (TC_Socket_Exception &ex) {
            _socket.close();
            return EM_SOCKET;
        }
    }
    return EM_SUCCESS;
}

int TC_UDPClient::send(const char *sSendBuffer, size_t iSendLen) {
    int iRet = checkSocket();
    if (iRet < 0) {
        return iRet;
    }

    iRet = _socket.send(sSendBuffer, iSendLen);
    if (iRet < 0) {
        return EM_SEND;
    }

    return EM_SUCCESS;
}

int TC_UDPClient::recv(char *sRecvBuffer, size_t &iRecvLen) {
    string sTmpIp;
    uint16_t iTmpPort;

    return recv(sRecvBuffer, iRecvLen, sTmpIp, iTmpPort);
}

int TC_UDPClient::recv(char *sRecvBuffer, size_t &iRecvLen, string &sRemoteIp, uint16_t &iRemotePort) {
    int iRet = checkSocket();
    if (iRet < 0) {
        return iRet;
    }

    int iRetCode = _epoller->wait(_timeout);
    if (iRetCode < 0) {
        return EM_SELECT;
    } else if (iRetCode == 0) {
        return EM_TIMEOUT;
    }

    const epoll_event &ev = _epoller->get(0);
    if (TC_Epoller::errorEvent(ev)) {
        _socket.close();
        return EM_CLOSE;
    }
#if TARGET_PLATFORM_IOS
        else
#else
    else if (TC_Epoller::readEvent(ev))
#endif
    {
        iRet = _socket.recvfrom(sRecvBuffer, iRecvLen, sRemoteIp, iRemotePort);
        if (iRet < 0) {
            return EM_SEND;
        }

        iRecvLen = iRet;
        return EM_SUCCESS;
    }

    return EM_SELECT;
}

int TC_UDPClient::sendRecv(const char *sSendBuffer, size_t iSendLen, char *sRecvBuffer, size_t &iRecvLen) {
    int iRet = send(sSendBuffer, iSendLen);
    if (iRet != EM_SUCCESS) {
        return iRet;
    }

    return recv(sRecvBuffer, iRecvLen);
}

int TC_UDPClient::sendRecv(const char *sSendBuffer, size_t iSendLen, char *sRecvBuffer, size_t &iRecvLen,
                           string &sRemoteIp, uint16_t &iRemotePort) {
    int iRet = send(sSendBuffer, iSendLen);
    if (iRet != EM_SUCCESS) {
        return iRet;
    }

    return recv(sRecvBuffer, iRecvLen, sRemoteIp, iRemotePort);
}

} // xy
