# CMakeList.txt : CMake project for ym-sprite-editor, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("ym-sprite-editor")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Stb REQUIRED)

add_subdirectory(lib)

add_executable(ym-sprite-editor "ym-sprite-editor.cpp")

target_include_directories(ym-sprite-editor PRIVATE ${Stb_INCLUDE_DIR})

target_link_libraries(ym-sprite-editor
        PRIVATE
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>)

target_link_libraries(ym-sprite-editor PRIVATE imgui::imgui)
target_link_libraries(ym-sprite-editor PRIVATE ym-sprite-editor-lib)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/data
    COMMAND_EXPAND_LISTS)