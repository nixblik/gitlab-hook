cmake_minimum_required(VERSION 3.5)

project(gitlab-hook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gitlab-hook
  src/main.cpp src/config.h src/config.cpp src/log.h src/log.cpp
  src/http_server.h src/http_server.cpp)
target_compile_definitions(gitlab-hook
  PRIVATE EXECUTABLE="gitlab-hook"
  PRIVATE VERSION="1.0"
  PRIVATE DEFAULT_CONFIG_FILE="/etc/gitlab-hook/config.ini")
target_link_libraries(gitlab-hook boost_program_options microhttpd systemd)
