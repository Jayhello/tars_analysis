//
// Created by Administrator on 2023/8/2.
//

#include <iostream>
#include "demo_util.h"
#include "util/xy_ec.h"

int main(){
    xy::test_exception();

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

}// xy