message("start to build inference examples")

add_executable(yolov5_demo "yolov5_demo.cpp")
target_link_libraries(yolov5_demo ${PROJECT_NAME})

add_executable(retina_face_demo "retina_face_demo.cpp")
target_link_libraries(retina_face_demo ${PROJECT_NAME})

add_executable(yolov8_demo "yolov8_demo.cpp")
target_link_libraries(yolov8_demo ${PROJECT_NAME})

add_executable(yolov8_pose_demo "yolov8_pose_demo.cpp")
target_link_libraries(yolov8_pose_demo ${PROJECT_NAME})
