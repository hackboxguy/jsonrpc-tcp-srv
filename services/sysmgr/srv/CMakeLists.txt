cmake_minimum_required(VERSION 3.14)

# Set the name of the binary and libraries
set(COMPONENT_NAME sysmgr)  # Name of the binary
set(LIB_ADSTTNG AdSttngs)   # Settings library
set(LIB_ADCOMMN AdCmn)      # Common library

# Define the project
project(${COMPONENT_NAME})

# Options
option(AUTO_GIT_VERSION "Automatically include Git version" ON)  # Use Git instead of SVN
option(INSTALL_MY_HEADERS "Install header files" ON)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Startup scripts
set(STARTUP_SCRIPT SysmgrStartupScr)          # Server startup script
set(WRTSTARTUP_SCRIPT WrtSysmgrStartupScr)    # Server startup script for OpenWrt
set(SYSTEMDSTARTUP_SCRIPT sysmgr.service)     # Server startup script for systemd
set(STARTNUM "70" CACHE STRING "70")          # Init order number of the service

# Source files
file(GLOB SOURCES "src/*.cpp")

# Create the executable
add_executable(${COMPONENT_NAME} ${SOURCES})

# Include directories
set(INC_ADSTTNG  "${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/lib-settings/include")
set(INC_ADCOMMN  "${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/lib-jsonrpc-tcp/include")
set(INC_SRVCOMMN "${CMAKE_CURRENT_SOURCE_DIR}/../include")  # RPC server's common header file

# Add include paths for the target
target_include_directories(${COMPONENT_NAME} PRIVATE
    ${INC_ADSTTNG}
    ${INC_ADCOMMN}
    ${INC_SRVCOMMN}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Specify component's dependencies
find_package(Threads REQUIRED)  # For pthread
find_package(json-c REQUIRED)   # For json-c

target_link_libraries(${COMPONENT_NAME} PRIVATE
    ${LIB_ADCOMMN}
    ${LIB_ADSTTNG}
    json-c
    Threads::Threads
    rt
)

# Install the binary, headers, and startup scripts
file(GLOB INCFILES "${INC_SRVCOMMN}/*.*")
install(TARGETS ${COMPONENT_NAME} DESTINATION bin)
install(PROGRAMS ${STARTUP_SCRIPT} DESTINATION etc/init.d)  # Copy server's startup script to init.d
install(PROGRAMS ${WRTSTARTUP_SCRIPT} DESTINATION etc/init.d)  # Copy server's startup script to init.d
install(PROGRAMS ${SYSTEMDSTARTUP_SCRIPT} DESTINATION etc/systemd/system)  # Copy server's startup script to systemd/system
if(INSTALL_MY_HEADERS)
    install(FILES ${INCFILES} DESTINATION include)
endif()

# Create a symbolic link for the startup script
if(CMAKE_HOST_UNIX)
    install(CODE "execute_process(
        WORKING_DIRECTORY \$ENV{TARGET_DIR}\${CMAKE_INSTALL_PREFIX}/etc/init.d
        COMMAND ln -sf ${STARTUP_SCRIPT} S${STARTNUM}${STARTUP_SCRIPT}
    )")
endif()

# Git version integration
if(AUTO_GIT_VERSION)
    find_package(Git REQUIRED)
    if(GIT_FOUND)
        # Run `git rev-list --count HEAD` to get the number of commits
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_COUNT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    else()
        # Fallback if Git is not found
        set(GIT_COMMIT_COUNT "0001")
    endif()

    # Write the version to SrcControlVersion.h
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/SrcControlVersion.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/SrcControlVersion.h
        @ONLY
    )
else()
    # Fallback if Git versioning is disabled
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/SrcControlVersion.h "#define SRC_CONTROL_VERSION 0001\n")
endif()
