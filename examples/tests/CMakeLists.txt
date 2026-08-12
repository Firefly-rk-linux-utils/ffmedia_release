if(TARGET FFMedia::ff_media)
    set(FFMEDIA_TEST_LIBRARY FFMedia::ff_media)
else()
    set(FFMEDIA_TEST_LIBRARY ff_media)
endif()

function(add_ffmedia_test_program target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${FFMEDIA_TEST_LIBRARY})
endfunction()

add_ffmedia_test_program(test_media_parameter test_media_parameter.cpp)
target_link_libraries(test_media_parameter PRIVATE pthread)
add_test(NAME media_parameter COMMAND test_media_parameter)

add_ffmedia_test_program(test_media_channel test_media_channel.cpp)
target_link_libraries(test_media_channel PRIVATE pthread)
add_test(NAME media_channel COMMAND test_media_channel)

add_ffmedia_test_program(test_module_app_source test_module_app_source.cpp)
add_test(NAME module_app_source COMMAND test_module_app_source)

add_ffmedia_test_program(test_module_app_processor test_module_app_processor.cpp)
add_test(NAME module_app_processor COMMAND test_module_app_processor)

add_ffmedia_test_program(test_camera vi/test_camera.cpp)
add_ffmedia_test_program(test_file_reader vi/test_file_reader.cpp)
add_ffmedia_test_program(test_rtsp_client vi/test_rtsp_client.cpp)
add_ffmedia_test_program(test_mpp_decoder vp/test_mpp_decoder.cpp)
add_ffmedia_test_program(test_mpp_encoder vp/test_mpp_encoder.cpp)
add_ffmedia_test_program(test_module_drmDisplay vo/test_module_drmDisplay.cpp)
add_ffmedia_test_program(test_module_imageProcessor vp/test_module_imageProcessor.cpp)
add_ffmedia_test_program(test_ffmpeg_demux vi/test_ffmpeg_demux.cpp)
