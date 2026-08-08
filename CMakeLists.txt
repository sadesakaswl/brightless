cmake_minimum_required(VERSION 3.21)

project(brightless VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS DBus LinguistTools Quick QuickControls2 Widgets)
find_package(KF6GlobalAccel REQUIRED)
find_package(KF6StatusNotifierItem REQUIRED)
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

qt_add_translations(brightless
    TS_FILES
        translations/brightless_en.ts
        translations/brightless_de.ts
        translations/brightless_fr.ts
        translations/brightless_it.ts
        translations/brightless_tr.ts
        translations/brightless_es.ts
        translations/brightless_pl.ts
        translations/brightless_pt.ts
        translations/brightless_zh.ts
        translations/brightless_ko.ts
        translations/brightless_ja.ts
        translations/brightless_ru.ts
)

target_include_directories(brightless PRIVATE src)
target_compile_definitions(brightless PRIVATE BRIGHTLESS_VERSION="${PROJECT_VERSION}")
target_link_libraries(brightless PRIVATE
    Qt6::DBus
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::GlobalAccel
    KF6::StatusNotifierItem
    PkgConfig::DDCUTIL
)

include(CheckIPOSupported)
check_ipo_supported()
set_property(TARGET brightless PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

include(GNUInstallDirs)
install(TARGETS brightless RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME translations
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/translations_test.py
            ${CMAKE_CURRENT_SOURCE_DIR}/src/qt/qml
            ${CMAKE_CURRENT_SOURCE_DIR}/translations
    )

    add_executable(brightless_model_test tests/model_test.cpp)
    target_include_directories(brightless_model_test PRIVATE src)
    add_test(NAME model COMMAND brightless_model_test)

    add_executable(brightless_controller_test
        tests/controller_test.cpp
        src/brightlesscontroller.cpp
        src/brightlesscontroller.h
    )
    target_include_directories(brightless_controller_test PRIVATE src)
    target_link_libraries(brightless_controller_test PRIVATE Qt6::Qml PkgConfig::DDCUTIL)
    add_test(NAME controller COMMAND brightless_controller_test)

    find_program(DBUS_RUN_SESSION dbus-run-session)
    if(DBUS_RUN_SESSION)
        add_test(NAME single-instance
            COMMAND ${DBUS_RUN_SESSION} -- sh
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/single_instance_test.sh
                $<TARGET_FILE:brightless>
        )
        set_tests_properties(single-instance PROPERTIES TIMEOUT 30)
    endif()
endif()
