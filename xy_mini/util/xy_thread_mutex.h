//
// Created by wenwen on 2023/8/15.
//
#pragma once
#include <mutex>
#include <atomic>
#include "xy_noncopyable.h"

namespace xy {

class TC_ThreadCond;

class TC_ThreadMutex : public noncopyable{
public:
    TC_ThreadMutex();
    virtual ~TC_ThreadMutex();

    void lock() const;
    bool tryLock() const;
    void unlock() const;

    friend class TC_ThreadCond;

protected:
    mutable std::mutex _mutex;
};

class TC_ThreadRecMutex{
public:
    TC_ThreadRecMutex();
    virtual ~TC_ThreadRecMutex();
    void lock() const;
    void unlock() const;
    bool tryLock() const;

    friend class TC_ThreadCond;
private:
    mutable std::recursive_mutex _mutex;
};

} // xy
