cmake_minimum_required(VERSION 3.16)
project(rtsp_relay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
  libavformat
  libavcodec
  libavutil)
find_package(Threads REQUIRED)

add_executable(rtsp_relay
  main.cpp
  src/streamer.cpp
  src/libav.cpp)

target_include_directories(rtsp_relay PRIVATE include)
target_link_libraries(rtsp_relay PRIVATE PkgConfig::FFMPEG Threads::Threads)