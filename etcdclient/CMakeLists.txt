cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic")

add_library (etcdclient etcdclient.cpp etcdclient.h)

install (FILES etcdclient.h DESTINATION include/etcdclient)

install (
  TARGETS etcdclient
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)
