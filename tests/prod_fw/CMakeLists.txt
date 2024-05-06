cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-fdiagnostics-color=always -Wno-error=format= -Wno-format  -Wno-unused-value)

project(product_fw)