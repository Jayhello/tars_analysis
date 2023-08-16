//
// Created by wenwen on 2023/8/15.
//
#pragma once
#include "xy_ec.h"

namespace xy {

struct TC_Lock_Exception : public TC_Exception{
    TC_Lock_Exception(const string &buffer) : TC_Exception(buffer){};
    ~TC_Lock_Exception() throw() {};
};



} // xy
