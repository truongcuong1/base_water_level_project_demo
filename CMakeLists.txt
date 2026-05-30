# Yêu cầu phiên bản CMake tối thiểu
cmake_minimum_required(VERSION 3.16)

# Nạp các công cụ build mặc định của ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Đặt tên cho project của cậu
project(water_level_project)