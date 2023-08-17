//
// Created by Administrator on 2023/8/2.
//

#include <iostream>
#include "demo_network.h"
#include "network/xy_scoket.h"

int main(){
    xy::test_scoket();

    return 0;
}

namespace xy{

    void test_scoket(){
    try{
        TC_Socket ts;
        ts.createSocket();
        ts.connect("www.baidu.com");

    }catch (const std::exception& ex){
        std::cout << "ex: " << ex.what() << std::endl;
    }
}

}// xy