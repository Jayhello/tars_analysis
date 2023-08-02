//
// Created by Administrator on 2023/8/2.
//

#include "xy_ec.h"

namespace xy{

TC_Exception::TC_Exception(const string &msg):_msg(msg){}

const char* TC_Exception::what() const throw(){
    return _msg.c_str();
}

TC_Exception::~TC_Exception(){
}


} // xy
