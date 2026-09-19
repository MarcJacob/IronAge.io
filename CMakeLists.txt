cmake_minimum_required(VERSION 3.20)

# Define projects
project (IronAgeIO CXX)

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32")
	# WEB CLIENT
	add_executable(IronAgeIO_WebClient
		src/platform/wasm_client_main.cpp)
	target_include_directories(IronAgeIO_WebClient PRIVATE ${CMAKE_SOURCE_DIR}/include/)
	set_target_properties(IronAgeIO_WebClient PROPERTIES SUFFIX ".wasm")

	# Auto-deploy .wasm (and latest version of web src) to dist folder on build.
	file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/deploy_web_client.bat" DEPLOY_WEB_CLIENT_SCRIPT)
	add_custom_command(TARGET IronAgeIO_WebClient POST_BUILD
		COMMAND ${DEPLOY_WEB_CLIENT_SCRIPT} $<SHELL_PATH:$<TARGET_FILE:IronAgeIO_WebClient>>)

	# Compiler & Linker options for wasm32 build.

	target_compile_options(IronAgeIO_WebClient PRIVATE
	   --target=wasm32-unknown-unknown 
	   -ffreestanding -fno-exceptions -fno-rtti -fvisibility=hidden -ffp-contract=off)

	target_link_options(IronAgeIO_WebClient PRIVATE
	    --target=wasm32-unknown-unknown 
		-nostdlib -fuse-ld=lld -Wl,--no-entry -Wl,--export-dynamic) 
else()
	# Win32 GAME SERVER
	add_executable (IronAgeIO_GameServer 
		src/platform/win32_game_server/win32_game_server_main.cpp)
	target_include_directories(IronAgeIO_GameServer PRIVATE ${CMAKE_SOURCE_DIR}/include/)
	target_compile_options(IronAgeIO_GameServer PRIVATE
		$<$<CXX_COMPILER_ID:Clang>:-ffp-contract=off>
		$<$<CXX_COMPILER_ID:MSVC>:/fp:precise>)

	# Add WinSock2 library.
	target_link_libraries(IronAgeIO_GameServer PRIVATE ws2_32)

	# TODO(MARC): MASTER SERVER
endif()

