cmake_minimum_required(VERSION 3.14)
set(PROJECT_NAME AdTests)
project(${PROJECT_NAME})
set(SYSMGR_START start-sysmgr.sh)
set(SYSMGR_STOP stop-sysmgr.sh)
set(SYSMGR_STRESS stress-sysmgr.sh)
set(SYSMGR_CONFIGS sysmgr-test)

#install symgr related tests and scripts
install(PROGRAMS ${SYSMGR_START} DESTINATION tests/bin)
install(PROGRAMS ${SYSMGR_STOP} DESTINATION tests/bin)
install(PROGRAMS ${SYSMGR_STRESS} DESTINATION tests/bin)
install(
  DIRECTORY ${SYSMGR_CONFIGS}/
  DESTINATION tests/sysmgr-test
  FILES_MATCHING PATTERN "*.txt"
)
