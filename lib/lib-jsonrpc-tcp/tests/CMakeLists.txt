# Unit and in-process integration tests for lib-jsonrpc-tcp.
# Enable with -DADLIB_BUILD_TESTS=ON, run with ctest (from the build dir).
# Combine with -DADLIB_SANITIZE=address or =thread for the sanitizer presets.
function(adlib_add_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE AdCmn AdSttngs)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES TIMEOUT 300)
endfunction()

adlib_add_test(test_generic_chain)
adlib_add_test(test_json_framer)
adlib_add_test(test_rpc_negative)
adlib_add_test(test_rpc_stream)
adlib_add_test(test_thread)
adlib_add_test(test_event_mgr)
adlib_add_test(test_signals)
