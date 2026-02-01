project(hello_demo)

add_compile_options(-std=c++11)
add_compile_options(-pthread)

include_directories(/usr/local/tars/cpp/include/)
link_directories(/usr/local/tars/cpp/lib/)

# tars server
set(TARS_SERVER_SRC tars_rpc_impl.cpp tars_rpc_server.cpp)
add_executable(tars_rpc_server ${TARS_SERVER_SRC})
target_link_libraries(tars_rpc_server
        -ltarsutil
        -ltarsservant
        -ltarsparse
        -lpthread
        "/usr/local/tars/cpp/lib/libtarsservant.a"
        "/usr/local/tars/cpp/lib/libtarsparse.a"
        "/usr/local/tars/cpp/lib/libtarsutil.a"
)

# tars client
set(TARS_CLIENT_SRC tars_rpc_client.cpp ../c17/c17_main.cpp)
add_executable(tars_rpc_client ${TARS_CLIENT_SRC})
target_link_libraries(tars_rpc_client
        -ltarsutil
        -ltarsservant
        -ltarsparse
        -lpthread
        "/usr/local/tars/cpp/lib/libtarsservant.a"
        "/usr/local/tars/cpp/lib/libtarsparse.a"
        "/usr/local/tars/cpp/lib/libtarsutil.a"
        mysqlclient
        )