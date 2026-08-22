cmake_minimum_required(VERSION 3.16)
project(mdraft_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Test Widgets Gui Core)
find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)

# ---- Core native smoke test (no WebEngine needed) ----
# (editor + outline + highlighter)
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
target_link_libraries(core_smoke_test PRIVATE Qt6::Test Qt6::Widgets Qt6::Gui Qt6::Core)

# ---- Preview + shutter lifecycle test (uses WebEngine when available) ----
add_executable(preview_lifecycle_test
    preview_lifecycle_test.cpp
    ../src/preview_widget.cpp
    ../src/preview_widget.h
    ../src/markdown_html.cpp
    ../src/markdown_html.h
    ../src/shutter_panel.cpp
    ../src/shutter_panel.h
)
target_include_directories(preview_lifecycle_test PRIVATE ../src)
target_link_libraries(preview_lifecycle_test PRIVATE Qt6::Test Qt6::Widgets Qt6::Gui Qt6::Core)
if(Qt6WebEngineWidgets_FOUND)
    target_compile_definitions(preview_lifecycle_test PRIVATE MDRAFT_HAVE_WEBENGINE)
    target_link_libraries(preview_lifecycle_test PRIVATE Qt6::WebEngineWidgets)
endif()
