cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME AdUtils)
project(${PROJECT_NAME})
add_subdirectory(tcp-json-rpc-client)
if(RUN_AUTO_TEST)
	add_subdirectory(tests)
endif()
