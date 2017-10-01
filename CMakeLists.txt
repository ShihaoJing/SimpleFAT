cmake_minimum_required(VERSION 3.8)
project(SimpleFAT)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        filesystem.c
        filesystem.h
        structs.c
        structs.h
        student.c
        support.c
        support.h)

add_executable(SimpleFAT ${SOURCE_FILES})