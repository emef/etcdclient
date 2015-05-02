cmake_minimum_required(VERSION 2.8)

include_directories (${PROJECT_SOURCE_DIR}/etcdclient)
link_directories (${PROJECT_SOURCE_DIR}/etcdclient)

add_executable (etcdclientdemo etcdclientdemo.cpp)
target_link_libraries (etcdclientdemo etcdclient curl)
