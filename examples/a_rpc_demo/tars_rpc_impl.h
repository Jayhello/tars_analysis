//
// Created by wenwen on 2021/7/11.
//

#pragma once

#include "servant/Application.h"
#include "tars_rpc.h"


class HelloImp : public TestApp::Hello
{
public:

    virtual ~HelloImp() {}

    virtual void initialize();

    virtual void destroy();

    virtual int hello(const std::string &req, std::string &resp, tars::TarsCurrentPtr current);

    virtual int search(const TestApp::SearchReq & req,TestApp::SearchResp &resp,tars::TarsCurrentPtr current);
};