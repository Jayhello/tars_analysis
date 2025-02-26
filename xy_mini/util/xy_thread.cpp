//
// Created by Administrator on 2023/8/16.
//

#include "xy_thread.h"
#include <sstream>
#include <cerrno>
#include <cassert>

namespace xy {

TC_ThreadControl::TC_ThreadControl(std::thread *th) : _th(th) {
    assert(_th != NULL);
}

void TC_ThreadControl::join() {
    if (std::this_thread::get_id() == _th->get_id()) {
        throw TC_ThreadThreadControl_Exception("[TC_ThreadControl::join] can't be called in the same thread");
    }

    if (_th->joinable()) {
        _th->join();
    }
}

void TC_ThreadControl::detach() {
    _th->detach();
}

std::thread::id TC_ThreadControl::id() const {
    return _th->get_id();
}

void TC_ThreadControl::sleep(int64_t millsecond) {
    std::this_thread::sleep_for(std::chrono::milliseconds(millsecond));
}

void TC_ThreadControl::yield() {
    std::this_thread::yield();
}

TC_Thread::TC_Thread() : _running(false), _th(NULL) {
}

TC_Thread::~TC_Thread() {

    if (_th != NULL) {
        //如果资源没有被detach或者被join，则自己释放
        if (_th->joinable()) {
            _th->detach();
        }

        delete _th;
        _th = NULL;
    }
}

void TC_Thread::threadEntry(TC_Thread *pThread) {
    pThread->_running = true;

    {
        TC_ThreadLock::Lock sync(pThread->_lock);
        pThread->_lock.notifyAll();
    }

    try {
        pThread->run();
    }
    catch (exception &ex) {
        pThread->_running = false;
        throw ex;
    }
    catch (...) {
        pThread->_running = false;
        throw;
    }
    pThread->_running = false;
}

TC_ThreadControl TC_Thread::start() {
    TC_ThreadLock::Lock sync(_lock);

    if (_running) {
        throw TC_ThreadThreadControl_Exception("[TC_Thread::start] thread has start");
    }

    try {
        _th = new std::thread(&TC_Thread::threadEntry, this);

    }
    catch (...) {
        throw TC_ThreadThreadControl_Exception("[TC_Thread::start] thread start error");
    }

    _lock.wait();

    return TC_ThreadControl(_th);
}

TC_ThreadControl TC_Thread::getThreadControl() {
    return TC_ThreadControl(_th);
}

bool TC_Thread::isAlive() const {
    return _running;
}

size_t TC_Thread::CURRENT_THREADID() {
    static thread_local size_t threadId = 0;
    if (threadId == 0) {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        threadId = strtol(ss.str().c_str(), NULL, 0);
    }
    return threadId;
}


} // xy