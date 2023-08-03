//
// Created by Administrator on 2023/8/2.
//

#include <iostream>
#include "demo_util.h"
#include "util/xy_ec.h"
#include "util/xy_autoptr.h"

int main(){
//    xy::test_exception();
    xy::test_autoptr();

    return 0;
}

namespace xy{

void test_exception(){
    try{
        throw TC_Exception("abc");
    }catch (const std::exception& ex){
        std::cout << "ex: " << ex.what() << std::endl;
    }
}

class TestAutoPtr : public TC_HandleBase{
public:
    ~TestAutoPtr(){
        std::cout << "dtor ref_cnt: " << getRef() << std::endl;
    }

    int a = 0;
};


void test_autoptr(){
    TC_AutoPtr<TestAutoPtr> ptr = new TestAutoPtr;
    std::cout << "ref_cnt: " << ptr->getRef() << std::endl;

    TC_AutoPtr<TestAutoPtr> ptr2 = ptr;
    std::cout << "after copy ref_cnt: " << ptr->getRef() << std::endl;
}


}// xy