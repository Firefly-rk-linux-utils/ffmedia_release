# vim: syntax=cmake
# ----------------------------------------------------------------------------
# mpp built-in unit test case
# ----------------------------------------------------------------------------
# macro for adding mpp sub-module unit test

project(ffmedia)

cmake_minimum_required(VERSION 3.10)

set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_INSTALL_PREFIX /usr)

SET(CMAKE_BUILD_TYPE "Release")
SET(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -Wall -fPIC -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -fPIC -g2 -ggdb")
SET(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O3 -Wall -fPIC -s")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -fPIC -s")

OPTION(DEMO_OPENCV "OpencvDemo" OFF)

IF(DEMO_OPENCV)
find_package(OpenCV REQUIRED)
ENDIF(DEMO_OPENCV)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_executable(demo
               demo.cpp
               utils.cpp
               )

add_executable(demo_simple
               demo_simple.cpp
               )

target_link_libraries(demo ff_media)
target_link_libraries(demo_simple ff_media)

IF(DEMO_OPENCV)
add_executable(demo_opencv
               demo_opencv.cpp
               )

add_executable(demo_opencv_multi
               demo_opencv_multi.cpp
               )

target_link_libraries(demo_opencv ff_media pthread opencv_core opencv_highgui opencv_imgcodecs)
target_link_libraries(demo_opencv_multi ff_media pthread opencv_core opencv_highgui opencv_imgcodecs)
ENDIF(DEMO_OPENCV)

INCLUDE(GNUInstallDirs)

install(TARGETS demo
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES lib/libff_media.so
	DESTINATION ${CMAKE_INSTALL_LIBDIR})
