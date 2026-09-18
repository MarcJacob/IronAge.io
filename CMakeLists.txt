cmake_minimum_required(VERSION 3.20)

# Define projects
project (IronAgeIO_GameServer CXX)

# GAME SERVER
add_executable (IronAgeIO_GameServer 
	src/platform/win32_game_server_main.cpp)

target_include_directories(IronAgeIO_GameServer PRIVATE ${CMAKE_SOURCE_DIR}/include/)

# WEB CLIENT

# TODO(MARC): MASTER SERVER
