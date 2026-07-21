function(add_ffmedia_test_program target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ff_media)
endfunction()

add_ffmedia_test_program(test_media_channel test_media_channel.cpp)
add_test(NAME media_channel COMMAND test_media_channel)

# The following programs require devices, media files, network services, or
# command-line arguments. Build them for validation, but do not run them from
# CTest automatically.
add_ffmedia_test_program(test_camera vi/test_camera.cpp)
add_ffmedia_test_program(test_file_reader vi/test_file_reader.cpp)
add_ffmedia_test_program(test_rtsp_client vi/test_rtsp_client.cpp)
add_ffmedia_test_program(test_mpp_decoder vp/test_mpp_decoder.cpp)
add_ffmedia_test_program(test_mpp_encoder vp/test_mpp_encoder.cpp)

if(ENABLE_FFMPEG)
    add_ffmedia_test_program(test_ffmpeg_demux vi/test_ffmpeg_demux.cpp)
endif()
