cmake_minimum_required(VERSION 2.8)

include_directories (${PROJECT_SOURCE_DIR}/etcdclient)
link_directories (${PROJECT_SOURCE_DIR}/etcdclient)

add_executable (demo demo.cpp)
target_link_libraries (demo etcdclient curl)
