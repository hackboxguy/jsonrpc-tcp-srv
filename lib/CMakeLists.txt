cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME AdLibraries)
project(${PROJECT_NAME})
add_subdirectory(lib-display)
add_subdirectory(lib-settings)
add_subdirectory(lib-jsonrpc-tcp)
#add_subdirectory(onion)

#for cross compiling
#cmake -H. -BOutput -DCMAKE_TOOLCHAIN_FILE=toolchain-arm-backup.cmake
#cmake --build Output -- all

#for local compiling
#cmake -H. -BOutput
#cmake --build Output -- all

#for cleaning cmake generated output
#rm -rf Output


