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
adlib_add_test(test_lifecycle)
adlib_add_test(test_rpc_client)

# libFuzzer targets (-DADLIB_FUZZ=ON with clang). ctest runs each for a few
# seconds on the seed corpus as a smoke test; for real fuzzing run e.g.
#   ./fuzz_request -dict=<src>/tests/fuzz.dict -max_total_time=600 corpus/
if(ADLIB_FUZZ)
  foreach(fuzzer fuzz_framer fuzz_client_parse fuzz_request)
    add_executable(${fuzzer} fuzz/${fuzzer}.cpp)
    target_link_libraries(${fuzzer} PRIVATE AdCmn AdSttngs)
    target_include_directories(${fuzzer} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_options(${fuzzer} PRIVATE -fsanitize=fuzzer)
    string(REPLACE "fuzz_" "" corpus_name ${fuzzer})
    string(REPLACE "_parse" "" corpus_name ${corpus_name})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus-${corpus_name})
    add_test(NAME ${fuzzer}
             COMMAND ${fuzzer} -max_total_time=20 -timeout=10
                     -dict=${CMAKE_CURRENT_SOURCE_DIR}/fuzz.dict
                     ${CMAKE_CURRENT_BINARY_DIR}/corpus-${corpus_name}
                     ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/${corpus_name})
    set_tests_properties(${fuzzer} PROPERTIES TIMEOUT 120)
  endforeach()
endif()
