//
// Created by Administrator on 2023/8/2.
//

#include "xy_scoket.h"

namespace xy{

    TC_Socket::TC_Socket() : _sock(INVALID_SOCKET), _bOwner(true), _iDomain(AF_INET)
    {
    }

    TC_Socket::~TC_Socket()
    {
        if(_bOwner)
        {
            close();
        }
    }

    void TC_Socket::init(int fd, bool bOwner, int iDomain)
    {
        if(_bOwner)
        {
            close();
        }

        _sock       = fd;
        _bOwner     = bOwner;
        _iDomain    = iDomain;
    }

    void TC_Socket::createSocket(int iSocketType, int iDomain)
    {
        // assert(iSocketType == SOCK_STREAM || iSocketType == SOCK_DGRAM);
        close();

        _iDomain    = iDomain;
        _sock       = socket(iDomain, iSocketType, 0);

        if(_sock < 0)
        {
            _sock = INVALID_SOCKET;
            THROW_EXCEPTION_SYSCODE(TC_Socket_Exception, "[TC_Socket::createSocket] create socket error");
            // throw TC_Socket_Exception("[TC_Socket::createSocket] create socket error! :" + string(strerror(errno)));
        }
        else
        {
//            ignoreSigPipe();
        }

    }

}
