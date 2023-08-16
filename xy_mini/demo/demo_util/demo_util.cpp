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

int main() {
//    xy::test_exception();
//    xy::test_autoptr();
//    xy::test_monitor();
    xy::test_thread();

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

}


}// xy