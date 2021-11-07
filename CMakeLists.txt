cmake_minimum_required(VERSION 3.3)

project(gstbluetoothaudiosink)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GST
    REQUIRED
        gstreamer-1.0>=1.4
        gstreamer-base-1.0>=1.4
        gstreamer-audio-1.0>=1.4)

add_library(${PROJECT_NAME} SHARED "")

set(TARGET ${PROJECT_NAME})

set(COMMON_INCLUDES
        ${GST_INCLUDE_DIRS})

set(COMMON_LIBRARIES
        ${GST_LIBRARIES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${COMMON_INCLUDES})

target_sources(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/gstbluetoothaudiosink.c)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${COMMON_LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/gstreamer-1.0)
