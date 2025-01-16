cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME AdTests)
project(${PROJECT_NAME})

# Define the directories where the scripts and configs are located
set(HELPER_SCRIPTS_DIR "scripts")  # Replace with the actual directory containing your .sh files
set(HELPER_CONFIGS "configs")  # Directory for sysmgr configs

# Find all .sh files in the scripts directory
file(GLOB HELPER_SCRIPTS "${HELPER_SCRIPTS_DIR}/*.sh")

# Install all .sh helper scripts to bin
install(PROGRAMS ${HELPER_SCRIPTS} DESTINATION bin)

# Install helper configs
install(
  DIRECTORY ${HELPER_CONFIGS}/
  DESTINATION etc/xmproxy/
  FILES_MATCHING PATTERN "*.txt"
)

