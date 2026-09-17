# CMakeList.txt : CMake project for IronAgeIO_Server, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

project ("IronAgeIO_Server")

# Add source to this project's executable.
add_executable (IronAgeIO_Server "src/win32/win32_main.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
# TODO: Determine exact C++ standard.
endif()
