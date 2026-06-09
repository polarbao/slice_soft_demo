cmake_minimum_required(VERSION 3.24)

project(SliceSoftDemo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(slicer_core
    src/third_party/miniz/miniz.c
    src/third_party/miniz/miniz_tdef.c
    src/third_party/miniz/miniz_tinfl.c
    src/third_party/miniz/miniz_zip.c
    src/slicer_core/config.cpp
    src/slicer_core/config.h
    src/slicer_core/json_value.cpp
    src/slicer_core/json_value.h
    src/slicer_core/model.cpp
    src/slicer_core/model.h
    src/slicer_core/rip_reader.cpp
    src/slicer_core/rip_reader.h
    src/slicer_core/slicer.cpp
    src/slicer_core/slicer.h
    src/slicer_core/texture_image.cpp
    src/slicer_core/texture_image.h
    src/slicer_core/tiff_io.cpp
    src/slicer_core/tiff_io.h
)

target_include_directories(slicer_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/miniz
)

if(WIN32)
    target_link_libraries(slicer_core PRIVATE windowscodecs ole32)
endif()

add_executable(slicer_cli
    apps/slicer_cli/main.cpp
)

target_link_libraries(slicer_cli PRIVATE slicer_core)

add_executable(rip_reader_test
    apps/rip_reader_test/main.cpp
)

target_link_libraries(rip_reader_test PRIVATE slicer_core)
