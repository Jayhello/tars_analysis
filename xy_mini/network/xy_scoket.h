//
// Created by Administrator on 2023/8/2.
//

#pragma once

#include <string>
#include "util/xy_ec.h"

namespace xy{

struct TC_Socket_Exception : public TC_Exception{
    TC_Socket_Exception(const string &buffer) : TC_Exception(buffer){};
//    TC_Socket_Exception(const string &buffer, int err) : TC_Exception(buffer, err){};
    ~TC_Socket_Exception() throw() {};
};

class TC_Socket{
public:
    TC_Socket();

    int fd_;
};

} // xy