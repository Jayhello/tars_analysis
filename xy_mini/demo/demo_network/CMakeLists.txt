project(demo_network)

set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS -pthread)

add_executable(xy_demo_network demo_network.cpp)
target_link_libraries(xy_demo_network
        xy_tars_util_lib
        xy_tars_net_lib
        )

add_executable(xy_demo_epoll_server demo_epoll_server.cpp)
target_link_libraries(xy_demo_epoll_server
        xy_tars_util_lib
        xy_tars_net_lib
        )

