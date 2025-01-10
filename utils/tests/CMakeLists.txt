cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME AdTests)
project(${PROJECT_NAME})

# Define the directories where the scripts and configs are located
set(SCRIPTS_DIR "scripts")  # Replace with the actual directory containing your .sh files
set(SYSMGR_CONFIGS "sysmgr-test")  # Directory for sysmgr configs
set(XMPROXY_CONFIGS "xmproxy-test")  # Directory for xmproxy configs

# Find all .sh files in the scripts directory
file(GLOB TEST_SCRIPTS "${SCRIPTS_DIR}/*.sh")

# Install all .sh files to tests/bin
install(PROGRAMS ${TEST_SCRIPTS} DESTINATION tests/bin)

# Install sysmgr test configs
install(
  DIRECTORY ${SYSMGR_CONFIGS}/
  DESTINATION tests/sysmgr-test
  FILES_MATCHING PATTERN "*.txt"
)

# Install xmproxy test configs
install(
  DIRECTORY ${XMPROXY_CONFIGS}/
  DESTINATION tests/xmproxy-test
  FILES_MATCHING PATTERN "*.txt"
)
