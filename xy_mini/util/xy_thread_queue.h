//
// Created by Administrator on 2023/8/17.
//
#pragma once

#include <deque>
#include <vector>
#include <cassert>
#include <mutex>
#include <condition_variable>

using namespace std;

namespace xy {

/**
 * @brief 线程安全队列
 */
template<typename T, typename D = deque<T> >
class TC_ThreadQueue {
public:
    TC_ThreadQueue() : _size(0) {}

public:

    typedef D queue_type;

    /**
     * @brief 从头部获取数据, 没有数据抛异常
     *
     * @param t
     * @return bool: true, 获取了数据, false, 无数据
     */
    T front();

    /**
     * @brief 从头部获取数据, 没有数据则等待.
     *
     * @param t
     * @param millsecond(wait = true时才生效)  阻塞等待时间(ms)
     *                    0 表示不阻塞
     * 					 -1 永久等待
     * @param wait, 是否wait
     * @return bool: true, 获取了数据, false, 无数据
     */
    bool pop_front(T &t, size_t millsecond = 0, bool wait = true);

    /**
     * @brief 从头部获取数据.
     *
     * @return bool: true, 获取了数据, false, 无数据
     */
    bool pop_front();

    /**
     * @brief 通知等待在队列上面的线程都醒过来
     */
    void notifyT();

    /**
     * @brief 放数据到队列后端.
     *
     * @param t
     */
    void push_back(const T &t, bool notify = true);

    /**
     * @brief  放数据到队列后端.
     *
     * @param vt
     */
    void push_back(const queue_type &qt, bool notify = true);

    /**
     * @brief  放数据到队列前端.
     *
     * @param t
     */
    void push_front(const T &t, bool notify = true);

    /**
     * @brief  放数据到队列前端.
     *
     * @param vt
     */
    void push_front(const queue_type &qt, bool notify = true);

    /**
     * @brief  交换数据
     *
     * @param q
     * @param millsecond(wait = true时才生效)  阻塞等待时间(ms)
     *                   0 表示不阻塞
     * 					 -1 如果为则永久等待
     * @param 是否等待有数据
     * @return 有数据返回true, 无数据返回false
     */
    bool swap(queue_type &q, size_t millsecond = 0, bool wait = true);

    /**
     * @brief  队列大小.
     *
     * @return size_t 队列大小
     */
    size_t size() const;

    /**
     * @brief  清空队列
     */
    void clear();

    /**
     * @brief  是否数据为空.
     *
     * @return bool 为空返回true，否则返回false
     */
    bool empty() const;

protected:
    TC_ThreadQueue(const TC_ThreadQueue &) = delete;

    TC_ThreadQueue(TC_ThreadQueue &&) = delete;

    TC_ThreadQueue &operator=(const TC_ThreadQueue &) = delete;

    TC_ThreadQueue &operator=(TC_ThreadQueue &&) = delete;

    bool hasNotify(size_t lockId) const {
        return lockId != _lockId;
    }

protected:
    /**
     * 队列
     */
    queue_type _queue;

    /**
     * 队列长度
     */
    size_t _size;

    //条件变量
    std::condition_variable _cond;

    //锁
    mutable std::mutex _mutex;

    //lockId, 判断请求是否唤醒过
    size_t _lockId = 0;
};

template<typename T, typename D>
T TC_ThreadQueue<T, D>::front() {
    std::unique_lock<std::mutex> lock(_mutex);

    return _queue.front();
}

template<typename T, typename D>
bool TC_ThreadQueue<T, D>::pop_front(T &t, size_t millsecond, bool wait) {
    if (wait) {

        size_t lockId = _lockId;

        std::unique_lock<std::mutex> lock(_mutex);

        // 此处等待两个条件： 1.来数据了; 2.有人唤醒了.
        // 任一条件满足都将打破等待立即返回
        if (millsecond == (size_t) -1) {
            _cond.wait(lock, [&] { return !_queue.empty() || hasNotify(lockId); });
        } else if (millsecond > 0) {
            _cond.wait_for(lock, std::chrono::milliseconds(millsecond),
                           [&] { return !_queue.empty() || hasNotify(lockId); });
        }

        // 超时了数据还没到 或 还没超时就被notify打破了, 直接返回
        if (_queue.empty() || hasNotify(lockId)) {
            return false;
        }

        t = _queue.front();
        _queue.pop_front();
        assert(_size > 0);
        --_size;

        return true;
    } else {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_queue.empty()) {
            return false;
        }

        t = _queue.front();
        _queue.pop_front();
        assert(_size > 0);
        --_size;

        return true;
    }
}


template<typename T, typename D>
bool TC_ThreadQueue<T, D>::pop_front() {
    std::unique_lock<std::mutex> lock(_mutex);
    if (_queue.empty()) {
        return false;
    }

    _queue.pop_front();
    assert(_size > 0);
    --_size;

    return true;
}

template<typename T, typename D>
void TC_ThreadQueue<T, D>::notifyT() {
    std::unique_lock<std::mutex> lock(_mutex);
    ++_lockId;
    _cond.notify_all();
}

template<typename T, typename D>
void TC_ThreadQueue<T, D>::push_back(const T &t, bool notify) {
    if (notify) {
        std::unique_lock<std::mutex> lock(_mutex);

        _queue.push_back(t);
        ++_size;

        _cond.notify_one();
    } else {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push_back(t);
        ++_size;
    }
}

template<typename T, typename D>
void TC_ThreadQueue<T, D>::push_back(const queue_type &qt, bool notify) {
    if (notify) {
        std::unique_lock<std::mutex> lock(_mutex);

        typename queue_type::const_iterator it = qt.begin();
        typename queue_type::const_iterator itEnd = qt.end();
        while (it != itEnd) {
            _queue.push_back(*it);
            ++it;
            ++_size;
        }
        _cond.notify_all();
    } else {
        std::lock_guard<std::mutex> lock(_mutex);

        typename queue_type::const_iterator it = qt.begin();
        typename queue_type::const_iterator itEnd = qt.end();
        while (it != itEnd) {
            _queue.push_back(*it);
            ++it;
            ++_size;
        }
    }
}

template<typename T, typename D>
void TC_ThreadQueue<T, D>::push_front(const T &t, bool notify) {
    if (notify) {
        std::unique_lock<std::mutex> lock(_mutex);

        _cond.notify_one();

        _queue.push_front(t);

        ++_size;
    } else {
        std::lock_guard<std::mutex> lock(_mutex);

        _queue.push_front(t);

        ++_size;
    }
}

template<typename T, typename D>
void TC_ThreadQueue<T, D>::push_front(const queue_type &qt, bool notify) {
    if (notify) {
        std::unique_lock<std::mutex> lock(_mutex);

        typename queue_type::const_iterator it = qt.begin();
        typename queue_type::const_iterator itEnd = qt.end();
        while (it != itEnd) {
            _queue.push_front(*it);
            ++it;
            ++_size;

        }

        _cond.notify_all();
    } else {
        std::lock_guard<std::mutex> lock(_mutex);

        typename queue_type::const_iterator it = qt.begin();
        typename queue_type::const_iterator itEnd = qt.end();
        while (it != itEnd) {
            _queue.push_front(*it);
            ++it;
            ++_size;

        }
    }
}

template<typename T, typename D>
bool TC_ThreadQueue<T, D>::swap(queue_type &q, size_t millsecond, bool wait) {
    if (wait) {

        size_t lockId = _lockId;

        std::unique_lock<std::mutex> lock(_mutex);

        // 此处等待两个条件： 1.来数据了; 2.notify来了
        // 任一条件满足都将打破等待立即返回
        if (millsecond == (size_t) -1) {
            _cond.wait(lock, [&] { return !_queue.empty() || hasNotify(lockId); });
        } else if (millsecond > 0) {
            _cond.wait_for(lock, std::chrono::milliseconds(millsecond),
                           [&] { return !_queue.empty() || hasNotify(lockId); });
        }

        // 超时了数据还没到 或 还没超时就被notify唤醒了, 直接返回
        if (_queue.empty() || hasNotify(lockId)) {
            return false;
        }

        q.swap(_queue);
        _size = _queue.size();

        return true;
    } else {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_queue.empty()) {
            return false;
        }

        q.swap(_queue);

        _size = _queue.size();

        return true;
    }
}

template<typename T, typename D>
size_t TC_ThreadQueue<T, D>::size() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _size;
}

template<typename T, typename D>
void TC_ThreadQueue<T, D>::clear() {
    std::lock_guard<std::mutex> lock(_mutex);
    _queue.clear();
    _size = 0;
}

template<typename T, typename D>
bool TC_ThreadQueue<T, D>::empty() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.empty();
}

}// xy
