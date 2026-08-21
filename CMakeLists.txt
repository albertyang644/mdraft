cmake_minimum_required(VERSION 3.16)

project(mdraft VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 Core & Widgets (always required)
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

# Qt WebEngine is optional: enabled when available so preview/PDF work.
option(MDRAFT_ENABLE_WEBENGINE "Build with Qt WebEngine preview/PDF support" ON)
find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)
if(MDRAFT_ENABLE_WEBENGINE AND Qt6WebEngineWidgets_FOUND)
    set(MDRAFT_HAVE_WEBENGINE TRUE)
    message(STATUS "WebEngine found: preview + PDF export enabled.")
else()
    set(MDRAFT_HAVE_WEBENGINE FALSE)
    message(STATUS "WebEngine disabled or not found: preview + PDF export disabled.")
endif()

add_executable(mdraft
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/editor.cpp
    src/editor.h
    src/highlighter.cpp
    src/highlighter.h
    src/outline_model.cpp
    src/outline_model.h
    src/outline_view.cpp
    src/outline_view.h
    src/exporter.cpp
    src/exporter.h
    src/preview_widget.cpp
    src/preview_widget.h
)

target_compile_definitions(mdraft PRIVATE
    $<$<BOOL:${MDRAFT_HAVE_WEBENGINE}>:MDRAFT_HAVE_WEBENGINE>
)
target_include_directories(mdraft PRIVATE src)

target_link_libraries(mdraft PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
)

if(MDRAFT_HAVE_WEBENGINE)
    target_link_libraries(mdraft PRIVATE Qt6::WebEngineWidgets)
endif()

install(TARGETS mdraft DESTINATION bin)
