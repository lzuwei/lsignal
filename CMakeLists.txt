cmake_minimum_required(VERSION 3.0)
PROJECT(demo)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wall -fexceptions -O3 -std=c++11")

ADD_EXECUTABLE(${PROJECT_NAME} main.cpp lsignal.h)
ADD_EXECUTABLE(test_lsignal tests.cpp lsignal.h)