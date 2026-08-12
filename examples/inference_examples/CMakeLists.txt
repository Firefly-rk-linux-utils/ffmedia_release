project(ff_media)

cmake_minimum_required(VERSION 3.10)

if(TARGET FFMedia::ff_media)
    set(FFMEDIA_INFERENCE_LIBRARY FFMedia::ff_media)
elseif(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # The source repository adds this directory before defining ff_media.
    # CMake resolves the target after the parent creates it.
    set(FFMEDIA_INFERENCE_LIBRARY ff_media)
else()
    find_package(FFMedia REQUIRED CONFIG
                 PATHS "${CMAKE_CURRENT_LIST_DIR}/../.."
                 NO_DEFAULT_PATH)
    set(FFMEDIA_INFERENCE_LIBRARY FFMedia::ff_media)
endif()

SET(CMAKE_BUILD_TYPE "Release")
SET(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -Wall -fPIC -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -fPIC -g2 -ggdb")
SET(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O3 -Wall -fPIC -s")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -fPIC -s")


find_package (Eigen3 3.3 REQUIRED NO_MODULE)

find_package(OpenCV 4 REQUIRED )

include_directories(${OpenCV_INCLUDE_DIRS}/include
                    inference_pool/include
                    tracker/bytetrack/include
                    yolov5/include
                    )

add_executable(demo_multi_detector
                yolov5/src/demo_multi_detector.cpp
                yolov5/src/yolov5s.cpp
                yolov5/src/postprocess.cc
                )

add_executable(demo_yolov5
                yolov5/src/demo_yolov5.cpp
                yolov5/src/postprocess.cc
                )

add_executable(demo_yolov5_extend
                yolov5/src/demo_yolov5_extend.cpp
                yolov5/src/postprocess.cc
                )

add_executable(demo_yolov5_track
                yolov5/src/demo_yolov5_track.cpp
                yolov5/src/postprocess.cc
                tracker/bytetrack/src/BytekalmanFilter.cpp
                tracker/bytetrack/src/BYTETracker.cpp
                tracker/bytetrack/src/lapjv.cpp
                tracker/bytetrack/src/STrack.cpp
                tracker/bytetrack/src/utils.cpp
                )

target_link_libraries(demo_multi_detector ${FFMEDIA_INFERENCE_LIBRARY} ${OpenCV_LIBS} pthread)
target_link_libraries(demo_yolov5 ${FFMEDIA_INFERENCE_LIBRARY} ${OpenCV_LIBS})
target_link_libraries(demo_yolov5_extend ${FFMEDIA_INFERENCE_LIBRARY} ${OpenCV_LIBS})
target_link_libraries(demo_yolov5_track ${FFMEDIA_INFERENCE_LIBRARY} ${OpenCV_LIBS} Eigen3::Eigen)
install(TARGETS demo_yolov5 demo_yolov5_extend demo_yolov5_track
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
