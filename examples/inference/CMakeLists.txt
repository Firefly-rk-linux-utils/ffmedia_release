cmake_minimum_required(VERSION 3.10)

project(ff_media_infer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Configured directly from ffmedia_release/examples/inference.
    find_package(FFMedia REQUIRED CONFIG COMPONENTS core
        PATHS "${CMAKE_CURRENT_LIST_DIR}/../.."
        NO_DEFAULT_PATH)
    set(FFMEDIA_INFER_LIBRARY FFMedia::ff_media)
elseif(TARGET FFMedia::ff_media)
    set(FFMEDIA_INFER_LIBRARY FFMedia::ff_media)
else()
    set(FFMEDIA_INFER_LIBRARY ff_media)
endif()

find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(OpenCV 4 REQUIRED)


file(GLOB_RECURSE INFER "infer/*.cpp")
file(GLOB_RECURSE BUFFER "buffer/*.cpp")
file(GLOB_RECURSE TRACK "track/*.cpp")
file(GLOB_RECURSE OSD "osd/*.cpp")

list(APPEND INFER_SOURCES ${INFER} ${BUFFER} ${TRACK} ${OSD})

add_library(ff_media_infer SHARED ${INFER_SOURCES})
target_include_directories(ff_media_infer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(ff_media_infer
    PUBLIC
        ${FFMEDIA_INFER_LIBRARY}
        Eigen3::Eigen
        ${OpenCV_LIBS}
    PRIVATE
        librknnrt.so)

add_subdirectory(demo/examples)
add_subdirectory(demo/face_track_demo)
