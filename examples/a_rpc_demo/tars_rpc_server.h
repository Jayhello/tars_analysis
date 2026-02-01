//
// Created by wenwen on 2021/7/11.
//

#pragma once

#include "servant/Application.h"
#include <iostream>

using namespace tars;

class HelloServer : public Application
{
public:

    virtual ~HelloServer() {};

    virtual void initialize();

    virtual void destroyApp();
};

extern HelloServer g_app;