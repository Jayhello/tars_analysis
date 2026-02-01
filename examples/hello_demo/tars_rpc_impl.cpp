//
// Created by wenwen on 2021/7/11.
//

#include "tars_rpc_impl.h"

void HelloImp::initialize()
{
    //initialize servant here:
    //...
}

void HelloImp::destroy()
{
    //destroy servant here:
    //...
}

int HelloImp::hello(const std::string &sReq, std::string &sRsp, tars::TarsCurrentPtr current)
{
    TLOGDEBUG("HelloImp::testHellosReq:"<<sReq<<endl);
    sRsp = sReq;
    return 0;
}

int HelloImp::search(const TestApp::SearchReq & req,TestApp::SearchResp &resp,tars::TarsCurrentPtr current){
    TLOGINFO("search req: "<<req.writeToJsonString()<<endl);

    resp.result = req.query + " result";

    return -1;
}