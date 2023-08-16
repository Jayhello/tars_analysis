//
// Created by wenwen on 2023/8/15.
//

#include "xy_thread_mutex.h"

namespace xy {

TC_ThreadMutex::TC_ThreadMutex()
{
}

TC_ThreadMutex::~TC_ThreadMutex()
{
}

void TC_ThreadMutex::lock() const
{
    _mutex.lock();
}

bool TC_ThreadMutex::tryLock() const
{
    return _mutex.try_lock();
}

void TC_ThreadMutex::unlock() const
{
    _mutex.unlock();
}

TC_ThreadRecMutex::TC_ThreadRecMutex()
{
}

TC_ThreadRecMutex::~TC_ThreadRecMutex()
{
}

void TC_ThreadRecMutex::lock() const
{
    _mutex.lock();
}

void TC_ThreadRecMutex::unlock() const
{
    _mutex.unlock();
}

bool TC_ThreadRecMutex::tryLock() const
{
    return _mutex.try_lock();
}

} // xy
