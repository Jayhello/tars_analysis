//
// Created by wenwen on 2021/7/11.
//

#include "tars_rpc_server.h"
#include "tars_rpc_impl.h"

using namespace std;

HelloServer g_app;

void HelloServer::initialize(){
    //initialize application here:
    addServant<HelloImp>(ServerConfig::Application + "." + ServerConfig::ServerName + ".HelloObj");
}

void HelloServer::destroyApp(){
    //destroy application here:
    //...
}


int main(int argc, char** argv){

    g_app.main(argc, argv);
    g_app.waitForShutdown();

    return 0;
}