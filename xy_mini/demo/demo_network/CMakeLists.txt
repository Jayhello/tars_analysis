project(demo_network)

set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS -pthread)

add_executable(demo_network demo_network.cpp)
target_link_libraries(demo_network
        xy_tars_util_lib
        xy_tars_net_lib
        )

