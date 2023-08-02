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

    virtual ~TC_Exception();

    virtual const char* what() const throw();

private:
    string  _msg;
};

} // xy
