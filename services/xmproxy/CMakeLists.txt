cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME Xmproxy)
project(${PROJECT_NAME})
add_subdirectory(srv)
add_subdirectory(clt)
add_subdirectory(helpers)

#for cross compiling
#cmake -H. -BOutput -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain-arm-backup.cmake -DCMAKE_INSTALL_PREFIX=/home/user/tmp/
#cmake --build Output -- all
#cmake --build Output -- install

#for local compiling
#cmake -H. -BOutput
#cmake --build Output -- all

#for cleaning make generated output
#rm -rf Output
