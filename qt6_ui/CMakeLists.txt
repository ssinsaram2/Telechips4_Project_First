cmake_minimum_required(VERSION 3.16)
project(school_bus_qt6_ui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)

add_executable(school_bus_ui
    main.cpp
    centralclient.h
    centralclient.cpp
    faceauthscreen.h
    faceauthscreen.cpp
    mainwindow.h
    mainwindow.cpp
    scenariowindow.h
    scenariowindow.cpp
)

target_include_directories(school_bus_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(school_bus_ui PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
)

if (UNIX)
    target_link_libraries(school_bus_ui PRIVATE rt)
endif ()

install(TARGETS school_bus_ui RUNTIME DESTINATION bin)
