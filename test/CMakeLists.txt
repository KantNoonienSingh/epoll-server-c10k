cmake_minimum_required (VERSION 3.0)

project(server)
set(APP_NAME app)

#
##
### Compilation and output
#####################################################################################
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

string(TOLOWER ${CMAKE_BUILD_TYPE} MY_BUILD_TYPE)

if (MY_BUILD_TYPE STREQUAL "debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Og")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
endif (MY_BUILD_TYPE STREQUAL "debug")

if (MY_BUILD_TYPE STREQUAL "release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif (MY_BUILD_TYPE STREQUAL "release")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

file(GLOB SRC *.cpp)

set(SRCS ${SRC})
#####################################################################################
###
##
#

add_executable(${APP_NAME} ${SRCS})

