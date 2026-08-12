# vim: syntax=cmake
cmake_minimum_required(VERSION 3.10)

project(ffmedia_release_examples LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

find_package(Threads REQUIRED)
find_package(FFMedia REQUIRED CONFIG
             PATHS "${CMAKE_CURRENT_SOURCE_DIR}/lib/cmake/FFMedia"
             NO_DEFAULT_PATH)

option(DEMO_OPENCV "Build OpenCV demos" OFF)
option(ENABLE_TESTS "Build FFMedia tests" ON)
option(ENABLE_INFERENCE_EXAMPLES "Build inference examples" OFF)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples")

function(add_ffmedia_demo target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target}
        PRIVATE FFMedia::FFMedia Threads::Threads)
endfunction()

add_ffmedia_demo(demo
    examples/demo/demo.cpp
    examples/demo/utils.cpp)
add_ffmedia_demo(demo_simple
    examples/demo/demo_simple.cpp)
add_ffmedia_demo(demo_memory_read
    examples/demo/demo_memory_read.cpp)
add_ffmedia_demo(demo_multi_drmplane
    examples/demo/demo_multi_drmplane.cpp)
add_ffmedia_demo(demo_multi_window
    examples/demo/demo_multi_window.cpp)
add_ffmedia_demo(demo_video_stack
    examples/demo/demo_video_stack.cpp)

set(FFMEDIA_DEMO_TARGETS
    demo
    demo_simple
    demo_memory_read
    demo_multi_drmplane
    demo_multi_window
    demo_video_stack)

add_ffmedia_demo(ffmedia examples/demo/ffmedia.cpp)
target_compile_definitions(ffmedia PRIVATE
    FFMEDIA_CLI_INFERENCE=$<BOOL:${FFMedia_WITH_INFERENCE}>)
list(APPEND FFMEDIA_DEMO_TARGETS ffmedia)

if(DEMO_OPENCV)
    find_package(OpenCV 4 REQUIRED)

    add_ffmedia_demo(demo_opencv examples/demo/demo_opencv.cpp)
    add_ffmedia_demo(demo_opencv_multi examples/demo/demo_opencv_multi.cpp)
    add_ffmedia_demo(demo_rgablend examples/demo/demo_rgablend.cpp)

    target_link_libraries(demo_opencv PRIVATE ${OpenCV_LIBS})
    target_link_libraries(demo_opencv_multi PRIVATE ${OpenCV_LIBS})
    target_link_libraries(demo_rgablend PRIVATE ${OpenCV_LIBS})
    list(APPEND FFMEDIA_DEMO_TARGETS
        demo_opencv demo_opencv_multi demo_rgablend)
endif()

if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(examples/tests)
endif()

if(ENABLE_INFERENCE_EXAMPLES)
    add_subdirectory(examples/inference_examples)
endif()

install(TARGETS ${FFMEDIA_DEMO_TARGETS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
