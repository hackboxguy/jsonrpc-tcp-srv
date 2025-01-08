cmake_minimum_required(VERSION 3.14)

# Set the name of the binary and libraries
set(COMPONENT_NAME xmproxysrv)           # Server name
set(STARTUP_SCRIPT XmproxyStartupScr)    # Server startup script
set(WRTSTARTUP_SCRIPT WrtXmproxyStartupScr) # Server startup script for OpenWrt-based system
set(SYSTEMDSTARTUP_SCRIPT xmproxysrv.service) # Server startup script for systemd-based system
set(STARTNUM "74" CACHE STRING "74")     # Init order number of the service
set(LIB_ADSTTNG AdSttngs)                # Settings library
set(LIB_ADCOMMN AdCmn)                   # Common library

# Options
option(WITH_CXMPP_LIB "Link with libstrophe XMPP C library" OFF)
option(WITH_AI_BOT "Link with cpp-httplib" OFF)
option(AUTO_GIT_VERSION "Automatically include Git version" ON)  # Use Git instead of SVN
option(INSTALL_MY_HEADERS "Install header files" ON)

# Add definitions based on options
if(WITH_CXMPP_LIB)
  add_definitions(-DUSE_CXMPP_LIB)
endif()
if(WITH_AI_BOT)
  add_definitions(-DUSE_AI_BOT)
endif()

# Define the project
project(${COMPONENT_NAME})

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files
file(GLOB SOURCES "src/*.cpp")
if(WITH_CXMPP_LIB)
  list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/ADXmppProxy.cpp)
else()
  list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/CXmppProxy.cpp)
endif()

# Create the executable
add_executable(${COMPONENT_NAME} ${SOURCES})

# Include directories
set(INC_ADSTTNG  "${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/lib-settings/include")
set(INC_ADCOMMN  "${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/lib-jsonrpc-tcp/include")
set(INC_SRVCOMMN "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set(INC_LIBSTROPHE "${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/libstrophe")

# Add include paths for the target
target_include_directories(${COMPONENT_NAME} PRIVATE
  ${INC_ADSTTNG}
  ${INC_ADCOMMN}
  ${INC_SRVCOMMN}
  ${CMAKE_CURRENT_BINARY_DIR}
)

# Find cpp-httplib
find_path(CPP_HTTPLIB_INCLUDE_DIR httplib.h
    PATHS
        /usr/include
        /usr/local/include
        ${CMAKE_SOURCE_DIR}/3rd_party/cpp-httplib
    DOC "Path to cpp-httplib header file"
)
if (NOT CPP_HTTPLIB_INCLUDE_DIR)
    message(FATAL_ERROR "cpp-httplib not found. Please install it using apt install libcpp-httplib-dev")
else()
    message(STATUS "Found cpp-httplib: ${CPP_HTTPLIB_INCLUDE_DIR}")
endif()
# Add cpp-httplib include directory
target_include_directories(${COMPONENT_NAME} PRIVATE ${CPP_HTTPLIB_INCLUDE_DIR})

# Specify component's dependencies
if(WITH_CXMPP_LIB)
  if(WITH_AI_BOT)
    target_link_libraries(${COMPONENT_NAME} PRIVATE
      ${LIB_ADCOMMN}
      ${LIB_ADSTTNG}
      json-c
      pthread
      rt
      strophe
      cpp-httplib
    )
  else()
    target_link_libraries(${COMPONENT_NAME} PRIVATE
      ${LIB_ADCOMMN}
      ${LIB_ADSTTNG}
      json-c
      pthread
      rt
      strophe
    )
  endif()
else()
  if(WITH_AI_BOT)
    target_link_libraries(${COMPONENT_NAME} PRIVATE
      ${LIB_ADCOMMN}
      ${LIB_ADSTTNG}
      json-c
      pthread
      rt
      gloox
      cpp-httplib
    )
  else()
    target_link_libraries(${COMPONENT_NAME} PRIVATE
      ${LIB_ADCOMMN}
      ${LIB_ADSTTNG}
      json-c
      pthread
      rt
      gloox
    )
  endif()
endif()

# Install the binary and headers
file(GLOB INCFILES "${INC_SRVCOMMN}/*.*")
install(TARGETS ${COMPONENT_NAME} DESTINATION bin)
install(PROGRAMS ${STARTUP_SCRIPT} DESTINATION etc/init.d) # Copy server's startup script to init.d
install(PROGRAMS ${WRTSTARTUP_SCRIPT} DESTINATION etc/init.d) # Copy server's startup script to init.d
install(PROGRAMS ${SYSTEMDSTARTUP_SCRIPT} DESTINATION etc/systemd/system) # Copy server's startup script to systemd/system
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
    # Get the number of commits as an integer
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

# Additional includes for libstrophe or gloox
if(WITH_CXMPP_LIB)
  target_include_directories(${COMPONENT_NAME} PRIVATE ${INC_LIBSTROPHE})
else()
  include(FindPkgConfig)
  pkg_check_modules(GLOOX REQUIRED gloox)
  if(NOT GLOOX_FOUND)
    message(FATAL_ERROR "ERROR: Could not find gloox development files")
  endif()
  target_include_directories(${COMPONENT_NAME} PRIVATE
    ${GLOOX_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party
  )
endif()
