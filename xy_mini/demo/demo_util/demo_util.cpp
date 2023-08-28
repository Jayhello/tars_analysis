//
// Created by Administrator on 2023/8/2.
//

#include <iostream>
#include "demo_util.h"
#include "util/xy_ec.h"
#include "util/xy_autoptr.h"
#include "util/xy_monitor.h"
#include "util/xy_thread.h"
#include "util/xy_common.h"
#include "util/xy_port.h"
#include "util/xy_timeprovider.h"
#include "util/xy_spin_lock.h"
#include "util/xy_file.h"
#include "util/xy_cas_queue.h"
#include "util/xy_logger.h"
#include "util/logging.h"

int main() {
//    xy::test_exception();
//    xy::test_autoptr();
//    xy::test_monitor();
//    xy::test_thread();
//    xy::test_time_provider();
//    xy::test_spin_lock();
//    xy::test_file();
//    xy::test_cas_queue();
//    xy::test_logger();
    xy::test_logging();

    return 0;
}

namespace xy {

void test_exception() {
    try {
        throw TC_Exception("abc");
    } catch (const std::exception &ex) {
        std::cout << "ex: " << ex.what() << std::endl;
    }
}

class TestAutoPtr : public TC_HandleBase {
public:
    ~TestAutoPtr() {
        std::cout << "dtor ref_cnt: " << getRef() << std::endl;
    }

    int a = 0;
};


void test_autoptr() {
    TC_AutoPtr<TestAutoPtr> ptr = new TestAutoPtr;
    std::cout << "ref_cnt: " << ptr->getRef() << std::endl;

    TC_AutoPtr<TestAutoPtr> ptr2 = ptr;
    std::cout << "after copy ref_cnt: " << ptr->getRef() << std::endl;
}

void test_monitor() {
    TC_ThreadLock lk;
    lk.lock();
}

class MyThread : public TC_Thread, public TC_ThreadLock {
public:
    MyThread() {
        bTerminate = false;
    }

    void terminate() {
        bTerminate = true;

        {
            TC_ThreadLock::Lock sync(*this);
            notifyAll();
        }
    }

    void doSomething() {
        cout << "doSomething" << endl;
    }

protected:
    virtual void run() {
        while (!bTerminate) {
            //TODO: your business
            doSomething();

            {
                TC_ThreadLock::Lock sync(*this);
                timedWait(1000);
            }
        }
    }

protected:
    bool bTerminate;
};

void test_thread() {

    MyThread mt;
    mt.start();

    TC_Common::sleep(5);

    mt.terminate();
    mt.getThreadControl().join();

    TC_Port::getpid();
}

void test_time_provider(){
    std::cout << TNOW << std::endl;
}

void test_spin_lock(){
    TC_SpinLock lk;
    std::cout << "spin_lock_ending..." << std::endl;
}

void test_file(){
    string path = "/mnt/e/work_note/item_order_w/gen_pid_gift/static_pid_gift.sh";
    cout << "size: " << TC_File::getFileSize(path) << endl;
}

void test_cas_queue(){
    TC_CasQueue<int> que;
    std::cout << "cas_queue_ending..." << std::endl;
}

void test_logger(){
    TC_LoggerThreadGroup g_group;
    TC_RollLogger        g_logger;
    TC_DayLogger         g_dlogger;
    g_group.start(1);

    g_logger.init("./debug", 1024 * 1024, 10);
    g_logger.modFlag(TC_RollLogger::HAS_LEVEL | TC_RollLogger::HAS_PID, true);
    g_logger.setLogLevel(1);
    g_logger.setupThread(&g_group);

    g_logger.debug() << "start test: " << std::this_thread::get_id() << endl;
}

void test_logging(){
    Info("this is test...");

    ScopeLog Log;
    Log << "first";
    Log << 1;
}

}// xy