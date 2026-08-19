cmake_minimum_required(VERSION 3.30)

project(subAuto)

add_executable(
    extract_audio
)

target_sources(extract_audio
    PRIVATE
        src/extract_audio.cpp
)