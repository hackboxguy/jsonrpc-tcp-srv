cmake_minimum_required(VERSION 3.14)

# Set the name of the library here
set(ADCMN_LIBNAME AdCmn)

# Set the path of the dependent library here
set(LIBADSTTNG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib-settings/include")

# Set the version of the library here
set(ADCMN_LIBVER_MAJOR 0)
set(ADCMN_LIBVER_MINOR 1)
set(ADCMN_LIBVER_PATCH 1)
set(ADCMN_LIBVER_STRING ${ADCMN_LIBVER_MAJOR}.${ADCMN_LIBVER_MINOR}.${ADCMN_LIBVER_PATCH})

# Define the project
project(${ADCMN_LIBNAME})

# Use GLOB to collect all .cpp files in the src directory
file(GLOB SOURCES "src/*.cpp")

# Generate the shared library from the sources
add_library(${ADCMN_LIBNAME} SHARED ${SOURCES})

# Set the version for the shared library
set_target_properties(${ADCMN_LIBNAME} PROPERTIES
    VERSION ${ADCMN_LIBVER_STRING}
    SOVERSION ${ADCMN_LIBVER_MAJOR}
)

# Include directories
target_include_directories(${ADCMN_LIBNAME} PUBLIC
    include
    ${LIBADSTTNG_SOURCE_DIR}
)

# Find and link dependencies
find_package(json-c REQUIRED)
target_link_libraries(${ADCMN_LIBNAME} PUBLIC json-c)
target_include_directories(${ADCMN_LIBNAME} PUBLIC ${JSON_INCLUDE_DIRS})

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Install the library and headers
file(GLOB PUBLIC_HEADERS "include/*.h")  # Use GLOB for headers if there are many
install(TARGETS ${ADCMN_LIBNAME}
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)
install(FILES ${PUBLIC_HEADERS} DESTINATION include)
