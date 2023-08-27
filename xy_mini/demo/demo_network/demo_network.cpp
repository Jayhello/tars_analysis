//
// Created by Administrator on 2023/8/2.
//

#include <iostream>
#include "demo_network.h"
#include "network/xy_scoket.h"
#include "network/xy_clientsocket.h"
#include "network/xy_http.h"
#include "util/xy_common.h"
#include "util/xy_logger.h"

int main(){
//    xy::test_scoket();
//    xy::test_post();
    xy::test_client_scoket();

    return 0;
}

namespace xy{

void test_scoket(){
    try{
//        TC_Socket ts;
//        ts.createSocket();
//        ts.connect("www.baidu.com");

        TC_TCPClient tc("198.2.4.5", 8088, 200);
        int ret = tc.send("abc", 3);
        std::cout << "send ret: " << ret << std::endl;

        TC_Common::sleep(5);
    }catch (const std::exception& ex){
        std::cout << "ex: " << ex.what() << std::endl;
    }
}

int test_post(){
    string strOut;

    string sServer1("http://127.0.0.1:10101/");

    TC_TCPClient tcpClient1;
    tcpClient1.init("127.0.0.1", 10101, 3000);

    TC_HttpRequest stHttpReq;
    stHttpReq.setCacheControl("no-cache");
    stHttpReq.setHeader("x-tx-host", "");
    stHttpReq.setHeader("Connection", "Keep-Alive");
    stHttpReq.setContentType("application/json");

    int iRet = -1;

    string sBuff("");
    sBuff="{\"key1\":\"value1\" \"key2\":\"value2\"}";

    stHttpReq.setPostRequest(sServer1, sBuff);
    // string sSendBuffer = stHttpReq.encode();
    TC_HttpResponse stHttpRsp;

    try
    {

        iRet = stHttpReq.doRequest(tcpClient1, stHttpRsp);
        cout << "do_req_ret: " << iRet << endl;
        if (iRet ==0)
        {
            strOut=stHttpRsp.getContent();
//                TLOGDEBUG("Http rsp: "<<stHttpRsp.getContent()<<endl);

        }
    }

    catch(TC_Exception &e)
    {
        cout << " exception: " << e.what() << endl;
    }
    catch(...)
    {
        cout << " unknown exception." << endl;
    }

    return iRet;
}

void test_client_scoket(){
    int ret = 0;
    try{
        TC_TCPClient client("0.0.0.0", 8084, 2000);
        ret = client.send("abcd", 4);

        std::size_t len = 0;
        string sData(100, '\0');
        ret = client.recv(&sData[0], len);
        cout << "recv_ret: " << ret << ", len: " << len << ", data: " << sData << endl;

    } catch (exception &ex) {
        cout << "client_ex: " << ex.what() << endl;
    }
}

}// xy