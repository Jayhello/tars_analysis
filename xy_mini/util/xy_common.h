//
// Created by Administrator on 2023/8/16.
//
#pragma once

#include <stdexcept>
#include <string>

namespace xy {

class TC_Common {
public:

    static const float _EPSILON_FLOAT;
    static const double _EPSILON_DOUBLE;
    static const int64_t ONE_DAY_MS = 24 * 3600 * 1000L;
    static const int64_t ONE_HOUR_MS = 1 * 3600 * 1000L;
    static const int64_t ONE_MIN_MS = 60 * 1000L;
    static const int64_t ONE_DAY_SEC = 86400;

    static void sleep(uint32_t sec);

    static void msleep(uint32_t ms);
};

}// xy