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
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_C_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -fPIC -s")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -fPIC -s")

OPTION(DEMO_EXTERNAL "external" OFF)

IF(DEMO_EXTERNAL)
find_package(OpenCV REQUIRED)
ENDIF(DEMO_EXTERNAL)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_executable(demo
               demo.cpp
               utils.cpp
               )

target_link_libraries(demo ff_media)

IF(DEMO_EXTERNAL)
add_executable(demo_external_async
               demo_external_async.cpp
               )

add_executable(demo_external_sync
               demo_external_sync.cpp
               )
target_link_libraries(demo_external_async ff_media pthread opencv_core opencv_highgui opencv_imgcodecs)
target_link_libraries(demo_external_sync ff_media pthread opencv_core opencv_highgui opencv_imgcodecs)
ENDIF(DEMO_EXTERNAL)

INCLUDE(GNUInstallDirs)

install(TARGETS demo
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES lib/libff_media.so
	DESTINATION ${CMAKE_INSTALL_LIBDIR})
