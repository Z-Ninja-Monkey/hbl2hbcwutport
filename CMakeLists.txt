cmake_minimum_required(VERSION 3.2)
project(hbl2hbcport)
set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HEADERS "src/*.h")

include_directories(src)

add_executable(hbl2hbc ${SOURCES})

wut_create_rpx(hbl2hbc)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/hbl2hbc.rpx"
        DESTINATION "${CMAKE_INSTALL_PREFIX}")