cmake_minimum_required(VERSION 3.19)

project(gitlab-hook LANGUAGES CXX VERSION 0.6)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(doc)
add_subdirectory(src)
add_subdirectory(test)
enable_testing()
