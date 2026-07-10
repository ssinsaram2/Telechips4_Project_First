cmake_minimum_required(VERSION 3.16)
project(school_bus_rpi4_central C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(central_app
    src/central_app.c
)

target_include_directories(central_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(central_app PRIVATE rt)

install(TARGETS central_app RUNTIME DESTINATION bin)
install(PROGRAMS
    scripts/send_child_alert_telegram.sh
    scripts/send_object_camera_event.py
    scripts/send_face_result.py
    DESTINATION bin)
