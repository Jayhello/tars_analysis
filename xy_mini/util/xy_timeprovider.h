//
// Created by Administrator on 2023/8/25.
//
#pragma once

#include <string.h>
#include "xy_platform.h"
#include "xy_common.h"
#include "xy_thread.h"
#include "xy_autoptr.h"

namespace xy {

#define TNOW     TC_TimeProvider::getInstance()->getNow()
#define TNOWMS   TC_TimeProvider::getInstance()->getNowMs()

class TC_TimeProvider {
public:

    /**
     * @brief 获取实例.
     *
     * @return TimeProvider&
     */
    static TC_TimeProvider *getInstance();

    /**
     * @brief 构造函数
     */
    TC_TimeProvider() : _use_tsc(true), _cpu_cycle(0), _buf_idx(0) {
        memset(_t, 0, sizeof(_t));
        memset(_tsc, 0, sizeof(_tsc));

        struct timeval tv;
        TC_Common::gettimeofday(tv);
        _t[0] = tv;
        _t[1] = tv;
    }

    /**
     * @brief 析构，停止线程
     */
    ~TC_TimeProvider();

    /**
     * @brief 获取时间.
     *
     * @return time_t 当前时间
     */
    time_t getNow() { return _t[_buf_idx].tv_sec; }

    /**
     * @brief 获取时间.
     *
     * @para timeval
     * @return void
     */
    void getNow(timeval *tv);

    /**
     * @brief 获取ms时间.
     *
     * @para timeval
     * @return void
     */
    int64_t getNowMs();

protected:
    static TC_TimeProvider *g_tp;

protected:

    /**
    * @brief 运行
    */
    virtual void run();

    uint64_t GetCycleCount();

    /**
    * @brief 获取cpu主频.
    *
    * @return float cpu主频
    */
    // double cpuMHz();

    void setTsc(timeval &tt);

    void addTimeOffset(timeval &tt, const int &idx);

private:
    bool _use_tsc;

    double _cpu_cycle;

    volatile int _buf_idx;

    timeval _t[2];

    uint64_t _tsc[2];
};

} // xy
