set(GTEST_ROOT /usr/src/googletest/googletest)
add_subdirectory(${GTEST_ROOT} "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
enable_testing()
include(GoogleTest)


add_executable(gitlab-hook-test
  test.h test_main.cpp test_gitlab_hook.cpp
  pipeline_event.json config.ini curl.sh script.sh
  cert/generate.sh cert/cert.cfg)
target_precompile_headers(gitlab-hook-test PRIVATE test.h)
target_link_libraries(gitlab-hook-test gtest)
gtest_discover_tests(gitlab-hook-test)
