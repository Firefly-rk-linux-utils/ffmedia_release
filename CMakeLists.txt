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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_executable(demo
               demo/demo.cpp
               demo/utils.cpp
               )

add_executable(demo_simple
               demo/demo_simple.cpp
               )

add_executable(demo_memory_read
               demo/demo_memory_read.cpp
               )

add_executable(demo_multi_drmplane
               demo/demo_multi_drmplane.cpp
               )

target_link_libraries(demo ff_media)
target_link_libraries(demo_simple ff_media)
target_link_libraries(demo_memory_read ff_media)
target_link_libraries(demo_multi_drmplane ff_media)

INCLUDE(GNUInstallDirs)

IF(DEMO_OPENCV)
add_executable(demo_opencv
               demo/demo_opencv.cpp
               )

add_executable(demo_opencv_multi
               demo/demo_opencv_multi.cpp
               )

add_executable(demo_rgablend
                demo/demo_rgablend.cpp
                )

target_link_libraries(demo_opencv ff_media ${OpenCV_LIBS})
target_link_libraries(demo_opencv_multi ff_media ${OpenCV_LIBS})
target_link_libraries(demo_rgablend ff_media ${OpenCV_LIBS})

install(TARGETS demo_opencv demo_opencv_multi demo_rgablend
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF(DEMO_OPENCV)

install(TARGETS demo demo_simple demo_memory_read demo_multi_drmplane
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES lib/libff_media.so
	DESTINATION ${CMAKE_INSTALL_LIBDIR})
