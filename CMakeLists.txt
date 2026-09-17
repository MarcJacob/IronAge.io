cmake_minimum_required(VERSION 3.20)
project (IronAgeIO_Server CXX)

add_executable (IronAgeIO_Server 
	src/win32/win32_main.cpp)

target_include_directories(IronAgeIO_Server PRIVATE ${CMAKE_SOURCE_DIR}/include/)
