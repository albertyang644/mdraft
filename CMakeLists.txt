cmake_minimum_required(VERSION 3.16)

project(mdraft VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
add_compile_options("$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic>")

# Qt6 Core & Widgets (always required)
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core PrintSupport)

# Qt WebEngine is optional and used only for the on-demand preview.
option(MDRAFT_ENABLE_WEBENGINE "Build with Qt WebEngine preview support" ON)
find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)
if(MDRAFT_ENABLE_WEBENGINE AND Qt6WebEngineWidgets_FOUND)
    set(MDRAFT_HAVE_WEBENGINE TRUE)
    message(STATUS "WebEngine found: rendered preview enabled.")
else()
    set(MDRAFT_HAVE_WEBENGINE FALSE)
    message(STATUS "WebEngine disabled or not found: rendered preview disabled.")
endif()

add_library(mdraft_core STATIC
    src/mainwindow.cpp
    src/mainwindow.h
    src/editor.cpp
    src/editor.h
    src/document_file.cpp
    src/document_file.h
    src/highlighter.cpp
    src/highlighter.h
    src/outline_model.cpp
    src/outline_model.h
    src/outline_view.cpp
    src/outline_view.h
    src/exporter.cpp
    src/exporter.h
    src/markdown_html.cpp
    src/markdown_html.h
    src/preview_widget.cpp
    src/preview_widget.h
    src/shutter_panel.cpp
    src/shutter_panel.h
    src/left_panel.cpp
    src/left_panel.h
    src/toggle_switch.cpp
    src/toggle_switch.h
    src/theme.cpp
    src/theme.h
)

target_compile_definitions(mdraft_core PUBLIC
    $<$<BOOL:${MDRAFT_HAVE_WEBENGINE}>:MDRAFT_HAVE_WEBENGINE>
)
target_include_directories(mdraft_core PUBLIC src)

target_link_libraries(mdraft_core PUBLIC
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::PrintSupport
)

if(MDRAFT_HAVE_WEBENGINE)
    target_link_libraries(mdraft_core PUBLIC Qt6::WebEngineWidgets)
endif()

add_executable(mdraft src/main.cpp resources.qrc)
target_link_libraries(mdraft PRIVATE mdraft_core)

install(TARGETS mdraft DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/mdraft.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(DIRECTORY assets/icons/hicolor/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor)
install(FILES assets/icons/mdraft.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
