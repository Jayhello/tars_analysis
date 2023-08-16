//
// Created by Administrator on 2023/8/16.
//

#include "xy_common.h"
#include <thread>

namespace xy{

void TC_Common::sleep(uint32_t sec)
{
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

void TC_Common::msleep(uint32_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

}// xy