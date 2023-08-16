//
// Created by Administrator on 2023/8/16.
//
#pragma once

namespace xy {

class noncopyable{
protected:
    noncopyable()= default;
    ~noncopyable()= default;
private:
    noncopyable(const noncopyable&)=delete;
    noncopyable& operator=(const noncopyable&)= delete;
};

} // xy

