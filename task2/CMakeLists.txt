cmake_minimum_required(VERSION 3.15)
project(IAP_task2)
set(CMAKE_CXX_STANDARD 17)

include_directories(source_code)
set(SOURCE_FILES source_code/image.cpp source_code/compressor.cpp source_code/decompressor.cpp)
add_executable(FractalEncoder ${SOURCE_FILES} encode.cpp)
add_executable(FractalDecoder ${SOURCE_FILES} decode.cpp)
target_include_directories(FractalEncoder PUBLIC source_code)
target_include_directories(FractalDecoder PUBLIC source_code)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(FractalEncoder ${OpenCV_LIBS})
target_link_libraries(FractalDecoder ${OpenCV_LIBS})