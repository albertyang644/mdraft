cmake_minimum_required(VERSION 3.16)
project(mdraft_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Test Widgets Gui Core)

add_executable(core_smoke_test
    core_smoke_test.cpp
    ../src/outline_model.cpp
    ../src/outline_model.h
    ../src/editor.cpp
    ../src/editor.h
    ../src/highlighter.cpp
    ../src/highlighter.h
)

target_include_directories(core_smoke_test PRIVATE ../src)
target_link_libraries(core_smoke_test PRIVATE
    Qt6::Test
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
)
