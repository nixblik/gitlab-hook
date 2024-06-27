cmake_minimum_required(VERSION 3.5)

project(gitlab-hook LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



add_executable(gitlab-hook
  src/main.cpp src/config.h src/config.cpp src/log.h src/log.cpp
  src/io_context.h src/io_context.cpp
  src/signal_listener.h src/signal_listener.cpp
  src/watchdog.h src/watchdog.cpp
  src/http_server.h src/http_server.cpp
  src/hook.h src/hook.cpp
  src/pipeline_hook.h src/pipeline_hook.cpp)
target_compile_definitions(gitlab-hook PRIVATE
  EXECUTABLE="gitlab-hook"
  VERSION="1.0"
  DEFAULT_CONFIG_FILE="/etc/gitlab-hook/config.ini")
target_link_libraries(gitlab-hook
  boost_program_options event_core microhttpd systemd)



set(GTEST_ROOT /usr/src/googletest/googletest)
add_subdirectory(${GTEST_ROOT} "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
enable_testing()
include(GoogleTest)

add_executable(test-gitlab-hook
  test/test.h test/test_main.cpp
  test/pipeline_event.json test/config.ini test/curl.sh
  test/cert/generate.sh test/cert/cert.cfg)
target_precompile_headers(test-gitlab-hook PRIVATE test/test.h)
target_link_libraries(test-gitlab-hook gtest)
gtest_discover_tests(test-gitlab-hook)
