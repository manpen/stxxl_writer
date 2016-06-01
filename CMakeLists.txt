############################################################################
#  CMakeLists.txt
#
#  Part of a simple STXXL example. See http://stxxl.sourceforge.net
#
#  Copyright (C) 2013 Timo Bingmann <tb@panthema.net>
#
#  Distributed under the Boost Software License, Version 1.0.
#  (See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt)
############################################################################

cmake_minimum_required(VERSION 2.8)

# we first give our project a name
project(stxxl_writer)

# prohibit in-source builds
if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
    message(SEND_ERROR "In-source builds are not allowed.")
endif("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")


# add third-party dependencies
SET(TMP_CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
SET(CMAKE_BUILD_TYPE "RelWithDebInfo")
add_subdirectory(extlib/stxxl/)
SET(CMAKE_BUILD_TYPE "${TMP_CMAKE_BUILD_TYPE}")

include_directories(${STXXL_INCLUDE_DIRS})


# enable warnings (always good)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -pipe")

# apply CXXFLAGS to our configuration
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${STXXL_CXX_FLAGS}")

add_library(stxxl_writer STATIC stxxl_writer.cpp)

