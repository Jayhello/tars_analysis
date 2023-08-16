//
// Created by wenwen on 2023/8/15.
//

#include "xy_thread_cond.h"
namespace xy {

TC_ThreadCond::TC_ThreadCond()
{
}

TC_ThreadCond::~TC_ThreadCond()
{
}

void TC_ThreadCond::signal()
{
    _cond.notify_one();
}

void TC_ThreadCond::broadcast()
{
    _cond.notify_all();
}

} // xy
