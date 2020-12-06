cmake_minimum_required(VERSION 3.17)
project(IAP_task3)
set(CMAKE_CXX_STANDARD 17)

include_directories(source_code)
set(SOURCE_FILES main.cpp source_code/image.cpp source_code/bw_image.cpp source_code/binarizer.cpp)
add_executable(IAP_task3 ${SOURCE_FILES})
target_include_directories(IAP_task3 PUBLIC source_code)

find_package(OpenCV REQUIRED)
find_package(TIFF REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS} ${TIFF_INCLUDE_DIRS})
target_link_libraries(IAP_task3 ${OpenCV_LIBS} ${TIFF_LIBRARIES})
