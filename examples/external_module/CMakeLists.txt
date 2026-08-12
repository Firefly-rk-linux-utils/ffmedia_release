cmake_minimum_required(VERSION 3.10)
project(ffmedia_external_sdk_test LANGUAGES CXX)

find_package(FFMedia REQUIRED CONFIG COMPONENTS core)

if(NOT TARGET FFMedia::FFMedia)
    message(FATAL_ERROR "Stable imported target FFMedia::FFMedia is missing")
endif()
if(NOT DEFINED FFMedia_MODULE_ABI_VERSION OR
   NOT DEFINED FFMedia_GLIBCXX_USE_CXX11_ABI)
    message(FATAL_ERROR "FFMedia ABI metadata is incomplete")
endif()

add_executable(ffmedia_external_sdk_test
    main.cpp
    legacy_headers.cpp
    public_headers.cpp)
target_link_libraries(ffmedia_external_sdk_test PRIVATE FFMedia::FFMedia)
