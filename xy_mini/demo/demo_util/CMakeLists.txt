project(demo_util)

set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS -pthread)

add_executable(demo_util demo_util.cpp)
target_link_libraries(demo_util xy_tars_util_lib)

