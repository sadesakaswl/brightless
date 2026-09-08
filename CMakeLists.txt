cmake_minimum_required(VERSION 3.21)
project(brightless VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS LinguistTools Network Quick QuickControls2 Widgets)
qt_standard_project_setup(REQUIRES 6.4)

if(WIN32)
    enable_language(RC)
    configure_file(resources/brightless.rc.in brightless.rc @ONLY)
    set(PLATFORM_SOURCES
        src/platform/windows/ddcbackend.cpp
        src/platform/windows/autostart.cpp
        src/platform/windows/sdrbrightnesscontroller.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/brightless.rc
    )
    set(PLATFORM_LIBRARIES dxva2 user32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    find_package(KF6GlobalAccel REQUIRED)
    find_package(KF6StatusNotifierItem REQUIRED)
    find_package(KF6Screen 6.0 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DDCUTIL REQUIRED IMPORTED_TARGET ddcutil>=1.2)
    set(PLATFORM_SOURCES
        src/platform/linux/ddcbackend.cpp
        src/platform/linux/autostart.cpp
        src/sdrbrightnesscontroller.cpp
    )
    set(PLATFORM_LIBRARIES Qt6::DBus KF6::GlobalAccel KF6::StatusNotifierItem KF6::Screen PkgConfig::DDCUTIL)
else()
    message(FATAL_ERROR "Brightless currently supports Linux and Windows")
endif()

qt_add_executable(brightless
    src/main.cpp
    src/brightlesscontroller.cpp src/brightlesscontroller.h
    src/settings.cpp
    src/desktopintegration.cpp src/desktopintegration.h
    src/singleinstance.cpp src/singleinstance.h
    src/sdrbrightnesscontroller.h
    src/model.h
    ${PLATFORM_SOURCES}
)
set_target_properties(brightless PROPERTIES WIN32_EXECUTABLE TRUE)

set(QML_FILES
    src/qt/qml/Main.qml
    src/qt/qml/SettingsWindow.qml
    src/qt/qml/MonitorCard.qml
    src/qt/qml/SdrBrightnessCard.qml
    src/qt/qml/Osd.qml
)
foreach(file IN LISTS QML_FILES)
    get_filename_component(name "${file}" NAME)
    set_source_files_properties("${file}" PROPERTIES QT_RESOURCE_ALIAS "${name}")
endforeach()
set_source_files_properties(resources/app-icon.png PROPERTIES QT_RESOURCE_ALIAS icon.png)
qt_add_qml_module(brightless
    URI com.brightless VERSION 1.0 RESOURCE_PREFIX /qt/qml
    QML_FILES ${QML_FILES}
    RESOURCES resources/app-icon.png
)

qt_add_translations(brightless TS_FILES
    translations/brightless_en.ts translations/brightless_de.ts
    translations/brightless_fr.ts translations/brightless_it.ts
    translations/brightless_tr.ts translations/brightless_es.ts
    translations/brightless_pl.ts translations/brightless_pt.ts
    translations/brightless_zh.ts translations/brightless_ko.ts
    translations/brightless_ja.ts translations/brightless_ru.ts
)
target_include_directories(brightless PRIVATE src)
target_compile_definitions(brightless PRIVATE BRIGHTLESS_VERSION="${PROJECT_VERSION}")
if(WIN32)
    target_compile_definitions(brightless PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
endif()
target_link_libraries(brightless PRIVATE
    Qt6::Network Qt6::Quick Qt6::QuickControls2 Qt6::Widgets ${PLATFORM_LIBRARIES})

# Keep release optimization without making unsupported toolchains fail configuration.
if(MSVC)
    target_compile_options(brightless PRIVATE $<$<CONFIG:Release>:/O2> /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(brightless PRIVATE $<$<CONFIG:Release>:-O3>)
endif()
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    set_target_properties(brightless PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
else()
    message(STATUS "LTO unavailable: ${ipo_error}")
endif()

include(GNUInstallDirs)
install(TARGETS brightless RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(WIN32)
    qt_generate_deploy_qml_app_script(TARGET brightless OUTPUT_SCRIPT deploy_script)
    install(SCRIPT ${deploy_script})
else()
    install(FILES resources/com.brightless.Brightless.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/com.brightless.Brightless.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES resources/app-icon.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME com.brightless.Brightless.png)
endif()
install(FILES LICENSE README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/brightless)
set(CPACK_PACKAGE_VENDOR "Brightless")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME translations COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/translations_test.py
        ${CMAKE_CURRENT_SOURCE_DIR}/src/qt/qml ${CMAKE_CURRENT_SOURCE_DIR}/translations)
    add_executable(brightless_model_test tests/model_test.cpp)
    target_include_directories(brightless_model_test PRIVATE src)
    add_test(NAME model COMMAND brightless_model_test)

    # Fake DDC transport; file-based autostart avoids touching the Windows Run registry in tests.
    add_executable(brightless_controller_test tests/controller_test.cpp
        src/brightlesscontroller.cpp src/brightlesscontroller.h src/settings.cpp
        src/platform/linux/autostart.cpp)
    target_include_directories(brightless_controller_test PRIVATE src)
    target_link_libraries(brightless_controller_test PRIVATE Qt6::Qml)
    target_compile_definitions(brightless_controller_test PRIVATE BRIGHTLESS_TESTING)
    add_test(NAME controller COMMAND brightless_controller_test)
    set_tests_properties(controller PROPERTIES TIMEOUT 30)

    add_executable(brightless_instance_test tests/single_instance_test.cpp
        src/singleinstance.cpp src/singleinstance.h)
    target_include_directories(brightless_instance_test PRIVATE src)
    target_link_libraries(brightless_instance_test PRIVATE Qt6::Network)
    target_compile_definitions(brightless_instance_test PRIVATE BRIGHTLESS_TESTING)
    if(WIN32)
        target_link_libraries(brightless_instance_test PRIVATE user32)
    endif()
    add_test(NAME single-instance COMMAND brightless_instance_test)
    set_tests_properties(single-instance PROPERTIES TIMEOUT 30)

    if(NOT WIN32)
        find_program(DBUS_RUN_SESSION dbus-run-session)
        if(DBUS_RUN_SESSION)
            add_test(NAME app-smoke COMMAND ${DBUS_RUN_SESSION} -- ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/app_smoke_test.py $<TARGET_FILE:brightless>)
            set_tests_properties(app-smoke PROPERTIES TIMEOUT 45)
        endif()
        add_executable(brightless_sdr_brightness_test tests/sdr_brightness_test.cpp
            src/sdrbrightnesscontroller.cpp src/sdrbrightnesscontroller.h)
        target_include_directories(brightless_sdr_brightness_test PRIVATE src)
        target_link_libraries(brightless_sdr_brightness_test PRIVATE Qt6::Qml KF6::Screen)
        add_test(NAME sdr-brightness COMMAND brightless_sdr_brightness_test)
        set_tests_properties(sdr-brightness PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
    endif()
endif()
