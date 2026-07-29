cmake_minimum_required(VERSION 3.21)

project(ZStreamEye VERSION 0.1.10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network OpenGL OpenGLWidgets)
find_package(PkgConfig QUIET)
include(CTest)

if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET
        libavformat
        libavcodec
        libavutil
        libswscale
    )
endif()

if(NOT FFMPEG_FOUND)
    find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
    find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
    find_path(AVUTIL_INCLUDE_DIR libavutil/frame.h)
    find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h)

    find_library(AVFORMAT_LIBRARY avformat)
    find_library(AVCODEC_LIBRARY avcodec)
    find_library(AVUTIL_LIBRARY avutil)
    find_library(SWSCALE_LIBRARY swscale)

    if(AVFORMAT_INCLUDE_DIR AND AVCODEC_INCLUDE_DIR AND AVUTIL_INCLUDE_DIR AND SWSCALE_INCLUDE_DIR
            AND AVFORMAT_LIBRARY AND AVCODEC_LIBRARY AND AVUTIL_LIBRARY AND SWSCALE_LIBRARY)
        add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
        target_include_directories(FFmpeg::FFmpeg INTERFACE
            ${AVFORMAT_INCLUDE_DIR}
            ${AVCODEC_INCLUDE_DIR}
            ${AVUTIL_INCLUDE_DIR}
            ${SWSCALE_INCLUDE_DIR}
        )
        target_link_libraries(FFmpeg::FFmpeg INTERFACE
            ${AVFORMAT_LIBRARY}
            ${AVCODEC_LIBRARY}
            ${AVUTIL_LIBRARY}
            ${SWSCALE_LIBRARY}
        )
        set(FFMPEG_FOUND TRUE)
    endif()
endif()

if(NOT FFMPEG_FOUND)
    message(FATAL_ERROR
        "FFmpeg development libraries were not found. Install ffmpeg[avcodec,avformat,swscale] via vcpkg "
        "or install system packages such as libavformat-dev libavcodec-dev libavutil-dev libswscale-dev.")
endif()

add_subdirectory(src/core)
add_subdirectory(src/app)

if(WIN32)
    add_subdirectory(src/platform/windows)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
