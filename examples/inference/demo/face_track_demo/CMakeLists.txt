project(face_track_demo)

add_executable(face_track_demo
    src/cam_face_track_demo.cpp
    src/pan_tilt_pwm_controller.cpp)

target_link_libraries(face_track_demo ff_media_infer)