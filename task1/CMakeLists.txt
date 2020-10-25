cmake_minimum_required(VERSION 3.15)
project(IAP_task1)

set(CMAKE_CXX_STANDARD 17)
find_package(OpenCV REQUIRED)

set(SOURCE_FILES main.cpp image.cpp vng.cpp)
add_executable(IAP_task1 ${SOURCE_FILES})

include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(IAP_task1 ${OpenCV_LIBS})
