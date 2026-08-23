find_package(Qt6 REQUIRED COMPONENTS Test)

# ---- Core native smoke test (no WebEngine needed) ----
# (editor + outline + highlighter)
add_executable(core_smoke_test core_smoke_test.cpp)
target_link_libraries(core_smoke_test PRIVATE mdraft_core Qt6::Test)

# ---- Preview + shutter lifecycle test (uses WebEngine when available) ----
add_executable(preview_lifecycle_test preview_lifecycle_test.cpp)
target_link_libraries(preview_lifecycle_test PRIVATE mdraft_core Qt6::Test)

add_executable(mainwindow_test mainwindow_test.cpp)
target_link_libraries(mainwindow_test PRIVATE mdraft_core Qt6::Test)

add_executable(document_file_test document_file_test.cpp)
target_link_libraries(document_file_test PRIVATE mdraft_core Qt6::Test)

add_executable(exporter_test exporter_test.cpp)
target_link_libraries(exporter_test PRIVATE mdraft_core Qt6::Test)

foreach(test_target core_smoke_test preview_lifecycle_test mainwindow_test document_file_test exporter_test)
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox")
endforeach()
