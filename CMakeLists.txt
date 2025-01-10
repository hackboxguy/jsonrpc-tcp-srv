cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME BrBoxComponents)
project(${PROJECT_NAME})

# Enable warnings as errors globally for C++ projects
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
# Enable warnings as errors globally for C projects
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

set(ONION_USE_SQLITE3 FALSE CACHE BOOL "Enable or disable SQLite3 support in Onion")
set(ONION_EXAMPLES FALSE CACHE BOOL "Enable or disable building examples of Onion")
option(INSTALL_CLIENT "Install client binary" ON)
option(SDSRV_USE_MUSL "link with musl c library" OFF)
option(RUN_AUTO_TEST "run automated testing" OFF)
if(RUN_AUTO_TEST)
	add_definitions(-DDEBUG_TEST_MODE)
endif()


if(${SDSRV_USE_MUSL})
	add_definitions(-DSDSRV_USE_MUSL_LIB)
endif()
add_subdirectory(lib)
add_subdirectory(services)
add_subdirectory(utils)
if(RUN_AUTO_TEST)
install(CODE "
  # Generate the ANSI escape code for red text
  string(ASCII 27 ESCAPE)
  set(RED \"\${ESCAPE}[31m\")  #Red text
  set(GREEN \"\${ESCAPE}[32m\")   # Green text 
  set(RESET \"\${ESCAPE}[0m\") #reset color 
  message(STATUS \"---------Automated Runtime test started---------\")
  message(STATUS \"Starting rpc-servers...\")
  execute_process(COMMAND /bin/bash -c \"CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/start-sysmgr.sh\")
  execute_process(COMMAND sleep 2)
  #message(STATUS \"running: CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/stress-sysmgr.sh\")
  execute_process(COMMAND /bin/bash -c \"CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/stress-sysmgr.sh\"
	 OUTPUT_VARIABLE TEST_OUTPUT
 	 RESULT_VARIABLE TEST_RESULT
 	)
  if(NOT TEST_RESULT EQUAL 0) 
	  message(STATUS \"\${RED}stress-sysmgr:fail!!! \${TEST_OUTPUT}\${RESET}\")
  else()
	  message(STATUS \"\${GREEN}stress-sysmgr:pass\${RESET}\")
  endif()
  message(STATUS \"Stoping rpc-servers...\")
  execute_process(COMMAND /bin/bash -c \"CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/stop-sysmgr.sh\")
  message(STATUS \"---------Automated Runtime test finished--------\")
")
endif()

#execute_process(COMMAND $BUILDOUT/bin/tcp-json-rpc-client --servertcpport=40001 --requests=$BUILDOUT/tests/sysmgr-test/requests.txt --responses=$BUILDOUT/tests/sysmgr-test/responses.txt --repeat=2000)

#for cross compiling
#cmake -H. -BOutput -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain-arm-backup.cmake -DCMAKE_INSTALL_PREFIX=/home/user/tmp/
#cmake --build Output -- all
#cmake --build Output -- install -j$(nproc)

#for local compiling
#cmake -H. -BOutput -DCMAKE_INSTALL_PREFIX=/home/adav/tmp/brbox -DAUTO_SVN_VERSION=OFF -DINSTALL_CLIENT=ON
#cmake -H. -BOutput
#cmake --build Output -- all -j$(nproc)

#for cleaning make generated output
#rm -rf Output

