cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME BrBoxComponents)
project(${PROJECT_NAME})

set(ONION_USE_SQLITE3 FALSE CACHE BOOL "Enable or disable SQLite3 support in Onion")
set(ONION_EXAMPLES FALSE CACHE BOOL "Enable or disable building examples of Onion")
option(INSTALL_CLIENT "Install client binary" ON)
option(SDSRV_USE_MUSL "link with musl c library" OFF)
option(RUN_AUTO_TEST "run automated testing" OFF)
option(VMBOX_BUILD "Build for vmbox Alpine deployment (installs manifest and scripts)" OFF)

# Enable warnings as errors globally (disabled for vmbox builds due to musl header warnings)
if(NOT VMBOX_BUILD)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()
if(RUN_AUTO_TEST)
	add_definitions(-DDEBUG_TEST_MODE)
endif()

#optimize of size
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os")  # Optimize for size
endif()

# -----------------------------------------------------------------------------
# Safety net: sanitizers, strict warnings, unit tests
# -----------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(ADLIB_SANITIZE "none" CACHE STRING "Sanitizer build: none|address|thread|undefined")
set_property(CACHE ADLIB_SANITIZE PROPERTY STRINGS none address thread undefined)
option(ADLIB_STRICT_WARNINGS "Enable -Wall -Wextra and friends (reported, not fatal)" OFF)
option(ADLIB_BUILD_TESTS "Build lib unit/integration tests (run with ctest)" OFF)

if(ADLIB_SANITIZE STREQUAL "address")
    # ASan and UBSan are combined into one preset; TSan needs its own build.
    set(ADLIB_SAN_FLAGS "-fsanitize=address,undefined -fno-sanitize-recover=undefined")
elseif(ADLIB_SANITIZE STREQUAL "thread")
    set(ADLIB_SAN_FLAGS "-fsanitize=thread")
elseif(ADLIB_SANITIZE STREQUAL "undefined")
    set(ADLIB_SAN_FLAGS "-fsanitize=undefined -fno-sanitize-recover=undefined")
elseif(NOT ADLIB_SANITIZE STREQUAL "none")
    message(FATAL_ERROR "ADLIB_SANITIZE must be none|address|thread|undefined")
endif()
if(ADLIB_SAN_FLAGS)
    # later -O1 overrides the -Os above
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADLIB_SAN_FLAGS} -fno-omit-frame-pointer -O1 -g")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ADLIB_SAN_FLAGS} -fno-omit-frame-pointer -O1 -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ADLIB_SAN_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ADLIB_SAN_FLAGS}")
endif()

# Release hardening: turns overflows of fixed buffers into a clean abort.
# Off by default when cross compiling (some OpenWrt toolchains lack libssp).
if(CMAKE_CROSSCOMPILING)
    set(ADLIB_HARDENING_DEFAULT OFF)
else()
    set(ADLIB_HARDENING_DEFAULT ON)
endif()
option(ADLIB_HARDENING "Build with -D_FORTIFY_SOURCE=2 -fstack-protector-strong" ${ADLIB_HARDENING_DEFAULT})
if(ADLIB_HARDENING AND NOT ADLIB_SAN_FLAGS)
    # -U first: some toolchains predefine _FORTIFY_SOURCE (redefinition
    # would be a -Werror failure). FORTIFY needs optimization (-Os above).
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector-strong")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -fstack-protector-strong")
endif()

if(ADLIB_STRICT_WARNINGS)
    set(ADLIB_WARN_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wshadow -Wformat=2")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(ADLIB_WARN_FLAGS "${ADLIB_WARN_FLAGS} -Wformat-overflow=2 -Wformat-truncation=2 -Wduplicated-cond -Wlogical-op")
    endif()
    # warnings are reported but not fatal until the backlog is cleaned up
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADLIB_WARN_FLAGS} -Wno-error")
endif()

if(ADLIB_BUILD_TESTS)
    enable_testing()
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
  #####xmproxysrv#########
  execute_process(COMMAND /bin/bash -c \"CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/start-sysmgr.sh\")
  execute_process(COMMAND sleep 2)
  #message(STATUS \"running: CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/stress-sysmgr.sh\")
  execute_process(COMMAND /bin/bash -c \"CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/stress-sysmgr.sh\"
	 OUTPUT_VARIABLE TEST_OUTPUT
 	 RESULT_VARIABLE TEST_RESULT
 	)
  set(AUTO_TEST_FAILED 0)
  if(NOT TEST_RESULT EQUAL 0) 
	  message(STATUS \"\${RED}stress-sysmgr:fail!!! \${TEST_OUTPUT}\${RESET}\")
	  set(AUTO_TEST_FAILED 1)
  else()
	  message(STATUS \"\${GREEN}stress-sysmgr:pass\${RESET}\")
  endif()
  #####xmproxysrv#########
  execute_process(COMMAND /bin/bash -c \"CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/start-xmproxysrv.sh\")
  execute_process(COMMAND sleep 3)
  execute_process(COMMAND /bin/bash -c \"CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/stress-xmproxysrv.sh\"
         OUTPUT_VARIABLE TEST_OUTPUT
         RESULT_VARIABLE TEST_RESULT
        )
  if(NOT TEST_RESULT EQUAL 0)
          message(STATUS \"\${RED}stress-xmproxysrv:fail!!! \${TEST_OUTPUT}\${RESET}\")
          set(AUTO_TEST_FAILED 1)
  else()
          message(STATUS \"\${GREEN}stress-xmproxysrv:pass\${RESET}\")
  endif()

  message(STATUS \"Stoping rpc-servers...\")
  execute_process(COMMAND /bin/bash -c \"CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/stop-xmproxysrv.sh\")
  execute_process(COMMAND /bin/bash -c \"CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} ${CMAKE_INSTALL_PREFIX}/tests/bin/stop-sysmgr.sh\")
  message(STATUS \"---------Automated Runtime test finished--------\")
  if(AUTO_TEST_FAILED)
    message(FATAL_ERROR \"Automated runtime test failed\")
  endif()
")
endif()

# =============================================================================
# vmbox Alpine Deployment (enabled with -DVMBOX_BUILD=ON)
# =============================================================================
# These install targets are specific to vmbox Alpine deployment and are
# disabled by default to avoid affecting buildroot/openwrt builds.

if(VMBOX_BUILD)
    message(STATUS "VMBOX_BUILD enabled - installing manifest and startup scripts")

    # Generate manifest.json from template
    set(VMBOX_PROJECT_NAME "jsonrpc-tcp-srv")
    configure_file(manifest.template.json
                   ${CMAKE_CURRENT_BINARY_DIR}/manifest.json
                   @ONLY)

    # Install manifest
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/manifest.json
            DESTINATION .)

    # Install startup/shutdown scripts
    install(PROGRAMS
            scripts/start-services.sh
            scripts/stop-services.sh
            scripts/restart-xmproxy.sh
            DESTINATION scripts)

    # Install XMPP configuration files
    install(FILES services/xmproxy/srv/xmpp-login.txt
            DESTINATION etc/xmproxy)
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

