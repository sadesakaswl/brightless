cmake_minimum_required(VERSION 3.21)

project(brightless VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick QuickControls2 Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DDCUTIL REQUIRED IMPORTED_TARGET ddcutil>=1.2)

qt_standard_project_setup(REQUIRES 6.4)

qt_add_executable(brightless
    src/main.cpp
    src/brightlesscontroller.cpp
    src/brightlesscontroller.h
    src/model.h
)

set_source_files_properties(src/qt/qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(src/qt/qml/MonitorCard.qml PROPERTIES QT_RESOURCE_ALIAS MonitorCard.qml)
set_source_files_properties(resources/app-icon.png PROPERTIES QT_RESOURCE_ALIAS icon.png)

qt_add_qml_module(brightless
    URI com.brightless
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        src/qt/qml/Main.qml
        src/qt/qml/MonitorCard.qml
    RESOURCES
        resources/app-icon.png
)

target_include_directories(brightless PRIVATE src)
target_link_libraries(brightless PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    PkgConfig::DDCUTIL
)

include(GNUInstallDirs)
install(TARGETS brightless RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CTest)
if(BUILD_TESTING)
    add_executable(brightless_model_test tests/model_test.cpp)
    target_include_directories(brightless_model_test PRIVATE src)
    add_test(NAME model COMMAND brightless_model_test)
endif()
