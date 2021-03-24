cmake_minimum_required(VERSION 3.4.1)
add_library(audioconvert SHARED native-lib.cpp )

find_library(log-lib log)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external_libs/include/${ANDROID_ABI})
set(AVFORMAT_LIB ${CMAKE_CURRENT_SOURCE_DIR}/external_libs/lib/${ANDROID_ABI}/libavformat.so)
set(AVCODEC_LIB ${CMAKE_CURRENT_SOURCE_DIR}/external_libs/lib/${ANDROID_ABI}/libavcodec.so)
set(AVUTIL_LIB ${CMAKE_CURRENT_SOURCE_DIR}/external_libs/lib/${ANDROID_ABI}/libavutil.so)
set(SWRESAMPLE_LIB ${CMAKE_CURRENT_SOURCE_DIR}/external_libs/lib/${ANDROID_ABI}/libswresample.so)

target_link_libraries(audioconvert ${log-lib} ${AVFORMAT_LIB} ${AVCODEC_LIB} ${AVUTIL_LIB} ${SWRESAMPLE_LIB})