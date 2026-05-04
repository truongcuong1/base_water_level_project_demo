# Yêu cầu phiên bản CMake tối thiểu
cmake_minimum_required(VERSION 3.16)

# Bao gồm file project.cmake mặc định của ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Tên dự án của bạn (viết liền không dấu)
project(water_level_sensor)