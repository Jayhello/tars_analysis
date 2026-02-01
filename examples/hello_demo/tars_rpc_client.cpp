//
// Created by wenwen on 2021/7/11.
//

#include <iostream>
#include "servant/Communicator.h"
#include "tars_rpc.h"
#include "util/tc_option.h"
#include "util/tc_common.h"
#include "util/tc_clientsocket.h"
#include "servant/Application.h"
#include "tup/tup.h"

using namespace std;
using namespace tars;
using namespace TestApp;

HelloPrx pPrx;
Communicator* _comm;

static string helloObj = "TestApp.HelloServer.HelloObj@tcp -h 127.0.0.1 -p 8999";
//static string helloObj = "TestApp.HelloServer.HelloObj@tcp -h 10.33.75.43 -p 8999:tcp -h 10.33.75.42 -p 8992";
//static string helloObj = "TestApp.HelloServer.HelloObj";

std::atomic<int> callback_count(0);

void init();

void syncCall();

void syncCallQuery();

void asyncCall();

void coro_call();

void test_socket();

int test_mysql();

int main(int argc, char** argv){
    auto ts = time(0);

    PidHistoryTreasurePlanInfo pt;
    pt.lHisTotal = 1001;
    pt.mDayTotal["20220911"] = 1100;
    pt.mDayTotal["20220912"] = 1200;
    std::cout << pt.writeToJsonString() << std::endl;

//    init();

//    for(int i = 0; i < 3; ++i){
//        syncCall();
//        sleep(1);
//    }


//    test_socket();

//    asyncCall();

//    coro_call();

    return 0;
}

void init(){

    _comm = new Communicator();

    _comm->setProperty("sendqueuelimit", "1000000");
    _comm->setProperty("asyncqueuecap", "1000000");
    _comm->setProperty("netthread", TC_Common::tostr(1));

//    _comm->setProperty("locator", "tars.tarsregistry.QueryObj@tcp -h 10.33.75.43 -p 17890");


    pPrx = _comm->stringToProxy<HelloPrx>(helloObj);

    pPrx->tars_connect_timeout(5000);
    pPrx->tars_async_timeout(60*1000);
    pPrx->tars_ping();
}

void syncCall(){
    TLOGINFO("sync call start"<<endl);

    string buffer(10, 'a');
    string r;

    pPrx->hello(buffer, r);
    printf("hello resp: %s\n", r.c_str());

//    vector<TC_Endpoint> vec_ep = pPrx->getEndpoint();
//    if(vec_ep.size()){
//        printf("vec_ep[0]: %s\n");
//        cout<< vec_ep[0].toString() << endl;
//    }

//    vector<EndpointInfo> vec_ae, vec_ie;
//    pPrx->tars_endpoints(vec_ae, vec_ie);
//    printf("\n ae: %d, ie: %d \n", vec_ae.size(), vec_ie.size());

    TestApp::SearchReq req;
    req.query = "search xy";

    TestApp::SearchResp resp;
    int ret = pPrx->search(req, resp);
    printf("search ret: %d , resp: %s\n", ret, resp.writeToJsonString().c_str());
}

void syncCallQuery(){

}

class HelloCallback: public HelloPrxCallback{
public:
    virtual void callback_hello(tars::Int32 ret, const std::string& resp)override{
        sleep(1);
        cerr<<"callback hello ret: "<<ret<<" resp: "<<resp<<endl;
    }

    virtual void callback_search(tars::Int32 ret, const TestApp::SearchResp& resp)override{
        sleep(1);
        cerr<<"callback hello ret: "<<ret<<" resp: "<<resp.writeToJsonString()<<endl;
    }
};

typedef tars::TC_AutoPtr<HelloCallback> HelloCallbackPtr;

void asyncCall(){
    HelloCallbackPtr cb_ptr = new HelloCallback;

    pPrx->async_hello(cb_ptr, "call_back_req_hello");

    printf("after async hello....\n");

    TestApp::SearchReq req;
    req.query = "cb_search_req";
    req.seq = 123;

    pPrx->async_search(cb_ptr, req);
    printf("after async search....\n");





//    sleep(5);
    getchar();
}

class CoroHelloCallback: public HelloCoroPrxCallback{
    public:
    virtual void callback_hello(tars::Int32 ret, const std::string& resp)override{
        sleep(1);
//        printf("callback hello ret: %d, resp: %s\n", ret, resp.c_str());
        cerr<<"coro_callback hello ret: "<<ret<<" resp: "<<resp<<endl;
    }

    virtual void callback_search(tars::Int32 ret, const TestApp::SearchResp& resp)override{
        sleep(1);
//        printf("callback search ret: %d, resp: %s\n", ret, resp.writeToJsonString().c_str());
        cerr<<"coro_callback hello ret: "<<ret<<" resp: "<<resp.writeToJsonString()<<endl;
    }
};

typedef tars::TC_AutoPtr<CoroHelloCallback> CoroHelloCallbackPtr;

void coro_call(){

    CoroHelloCallbackPtr cb_ptr = new CoroHelloCallback;
    CoroParallelBasePtr sharedPtr = new CoroParallelBase(2);
    cb_ptr->setCoroParallelBasePtr(sharedPtr);

    pPrx->coro_hello(cb_ptr, "coro_call_back_req_hello");

    printf("after coro_async hello....\n");

    TestApp::SearchReq req;
    req.query = "coro_cb_search_req";
    req.seq = 123;

    pPrx->async_search(cb_ptr, req);

    coroWhenAll(sharedPtr);
    printf("after coro_async search....\n");
}

void test_socket(){
    TC_TCPClient tcpClient;
//    tcpClient.init("10.33.75.43", 8999, 3000);
    tcpClient.init("10.33.75.43", 8084, 3000);

    TarsUniPacket<> req;
    TarsUniPacket<>rsp;
    int iRequestId=1;
    req.setRequestId(iRequestId);
    req.setServantName("TestApp.HelloServer.HelloObj");
    req.setFuncName("hello");

    req.put<string>("req", "123 xy");

    string sendBuff;
    req.encode(sendBuff);

    int iSuc = tcpClient.send(sendBuff.c_str(), sendBuff.size());

    char recvBuff[1024]={0};
    size_t recvLen = sizeof(recvBuff);
    iSuc = tcpClient.recv(recvBuff,recvLen);
    rsp.decode(recvBuff,recvLen);

    TLOGDEBUG("[requestId]:" << rsp.getRequestId() << endl);
    TLOGDEBUG("[servantName]:" << rsp.getServantName() << endl);
    TLOGDEBUG("[funcName]:" << rsp.getFuncName() << endl);
    TLOGDEBUG("getTarsVersion|"<<rsp.getTarsVersion()<<endl);

    string res_buf;
    rsp.encode(res_buf);
    cout<<"res_buf: "<<res_buf<<endl;

    int iRet = rsp.get<int>("");
    string res;
    rsp.get<string>("resp", res);

    printf("\n ret: %d, res: %s \n", iRet, res.c_str());
}

