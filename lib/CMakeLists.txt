cmake_minimum_required (VERSION 3.8)

include(cmake/lib_utils.cmake)

find_package(imgui CONFIG REQUIRED)

list(APPEND LIB_SOURCES "src/ym-sprite-editor.cpp")

declare_cpp_library(ym-sprite-editor-lib 20 ${LIB_SOURCES})

target_link_libraries(ym-sprite-editor-lib PRIVATE imgui::imgui)