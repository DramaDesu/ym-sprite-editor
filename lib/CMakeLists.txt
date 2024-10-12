cmake_minimum_required (VERSION 3.8)

include(cmake/lib_utils.cmake)

find_package(imgui CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

list(APPEND LIB_SOURCES "src/editor.cpp")

declare_cpp_library(ym-sprite-editor-lib 20 ${LIB_SOURCES})

target_link_libraries(ym-sprite-editor-lib PRIVATE imgui::imgui)
target_link_libraries(ym-sprite-editor-lib PRIVATE glm::glm)