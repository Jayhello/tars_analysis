project(xy_tars_net_lib)

set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS -pthread)

file(GLOB COMMON_SRCS ./*.cpp ./*.h)

add_library(xy_tars_net_lib
        ${COMMON_SRCS}
        )

target_link_libraries(xy_tars_net_lib
        xy_tars_util_lib
        )
