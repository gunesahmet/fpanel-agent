cmake_minimum_required(VERSION 3.6.0)
project(fpanel-agent)

set(dir ${CMAKE_CURRENT_SOURCE_DIR}/Compiled)
set(CMAKE_CXX_STANDARD 11)
# BOOST DEFINITIONS
set(boost_USE_STATIC_LIBS        ON)  # only find static libs
set(boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs
set(boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(boost_USE_MULTITHREADED      ON)  # only multithreading
set(boost_USE_STATIC_RUNTIME    OFF)

set(EXECUTABLE_OUTPUT_PATH ${dir})
set(CMAKE_CXX_FLAGS "-g -pthread -lcrypt -lpam -fpermissive -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -g2 -w -ggdb -O2 -Wall -ldl -pedantic -Wextra -fext-numeric-literals")

include_directories(.)
include_directories(/usr/local/include/)
include_directories(/usr/include/boost/)
include_directories(/usr/local/include/libcpuid/)

find_package(Boost 1.67.0 REQUIRED COMPONENTS
        program_options filesystem thread regex date_time)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    add_executable(fpanel-agent main.cpp plog/Log.h /usr/local/include/libcpuid/libcpuid.h Helpers/md5.cpp)
    target_link_libraries(fpanel-agent
            /usr/lib/libcrypto++.a
            /usr/local/lib/libcpuid.a
            /usr/local/lib/libboost_system.a
            /usr/local/lib/libboost_program_options.a
            /usr/local/lib/libboost_filesystem.a
            /usr/local/lib/libboost_thread.a
            /usr/local/lib/libboost_regex.a
            /usr/local/lib/libboost_date_time.a)
endif()
