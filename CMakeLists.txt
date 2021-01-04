cmake_minimum_required(VERSION 3.5)
project(open_hikv)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -g3 -fno-omit-frame-pointer")

add_executable(open_hikv_main main.cc
               hikv.h
               hikv.cc)
