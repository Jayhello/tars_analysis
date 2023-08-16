//
// Created by Administrator on 2023/8/2.
//
#pragma once

#include <stdexcept>
#include <string>

namespace xy{

using namespace std;

class TC_Exception : public exception{
public:
    explicit TC_Exception(const string &msg);

    TC_Exception(const string &buffer, int err);

    virtual ~TC_Exception();

    virtual const char* what() const throw();

    int getErrCode() { return _code; }

    // 获取错误字符串(linux是errno, windows是GetLastError())
    static string parseError(int err);

    // 获取系统错误码(linux是errno, windows是GetLastError)
    static int getSystemCode();
private:
    int     _code;

    string  _buffer;
};

} // xy

#define THROW_EXCEPTION_SYSCODE(EX_CLASS, buffer) \
{   \
int ret = TC_Exception::getSystemCode(); \
throw EX_CLASS(buffer, ret);              \
}
