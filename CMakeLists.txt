cmake_minimum_required(VERSION 3.30)

project(subAuto)

set(FFMPEG_DIR "C:/Users/Joel Osei-Asamoah/ffmpeg-9.0.1-full_build-shared/ffmpeg-9.0.1-full_build-shared")

add_executable(
    extract_audio
)

target_sources(extract_audio
    PRIVATE
        src/extract_audio.cpp
)

target_include_directories(extract_audio
    PRIVATE
        ${FFMPEG_DIR}/include
)

target_link_directories(extract_audio
    PRIVATE
        ${FFMPEG_DIR}/lib
)

target_link_libraries(extract_audio
    PRIVATE
        avcodec
        avformat
        avutil
        swresample
)   