//
// Created by Administrator on 2023/8/2.
//

#include "xy_ec.h"
#include <stdlib.h>
#include <cerrno>
#include <cstring>

namespace xy{


TC_Exception::TC_Exception(const string &buffer)
: _code(0), _buffer(buffer)
{
}


TC_Exception::TC_Exception(const string &buffer, int err)
{
    _buffer = buffer + " :" + parseError(err);
    _code   = err;
}

const char* TC_Exception::what() const throw(){
    return _buffer.c_str();
}

TC_Exception::~TC_Exception(){
}

string TC_Exception::parseError(int err)
{
    string errMsg;
    errMsg = strerror(err);
    return errMsg;
}

int TC_Exception::getSystemCode()
{
    return errno;
}

} // xy
