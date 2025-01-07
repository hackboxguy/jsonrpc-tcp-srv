cmake_minimum_required(VERSION 3.14)

# Set the name of the executable
set(EXECUTABLE_NAME tcp-json-rpc-client)

# Define the project
project(${EXECUTABLE_NAME})

# Set the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add the source file
set(SOURCES tcp-json-rpc-client.cpp)

# Create the executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# Find and link dependencies
find_package(Threads REQUIRED)  # For pthread
find_package(json-c REQUIRED)   # For json-c

# Link the libraries
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    json-c
    Threads::Threads
)

# Install the executable (optional)
install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)
