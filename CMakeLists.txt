cmake_minimum_required(VERSION 3.19)

project(gitlab-hook LANGUAGES CXX VERSION 1.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(debian)
add_subdirectory(doc)
add_subdirectory(src)
add_subdirectory(test)
enable_testing()

add_library(project INTERFACE EXCLUDE_FROM_ALL
  README.md COPYING)
