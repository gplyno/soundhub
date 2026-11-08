cmake_minimum_required(VERSION 3.21)

project(SoundHub VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia)

qt_add_executable(soundhub
    source/cpp/main.cpp
    source/cpp/mainwindow.cpp
    source/cpp/catalog.cpp
    source/cpp/player.cpp
    source/h/mainwindow.h
    source/h/catalog.h
    source/h/player.h
    resources.qrc
)

target_include_directories(soundhub PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source/h)

target_link_libraries(soundhub PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

set_target_properties(soundhub PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER org.gplyno.soundhub
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
)

if(WIN32)
    # rc.exe cannot handle non-ASCII paths, so use the 8.3 short form of the icon path.
    set(SOUNDHUB_ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon.ico")
    execute_process(
        COMMAND cmd /c "${CMAKE_CURRENT_SOURCE_DIR}/scripts/shortpath.bat" "${SOUNDHUB_ICON}"
        OUTPUT_VARIABLE SOUNDHUB_ICON_RC
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT SOUNDHUB_ICON_RC)
        set(SOUNDHUB_ICON_RC "${SOUNDHUB_ICON}")
    endif()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/app.rc" "IDI_ICON1 ICON \"${SOUNDHUB_ICON_RC}\"\n")
    target_sources(soundhub PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
endif()

if(MSVC)
    target_compile_options(soundhub PRIVATE /utf-8)
endif()

if(WIN32)
    target_link_libraries(soundhub PRIVATE dwmapi)
endif()

if(ANDROID)
    set_property(TARGET soundhub PROPERTY QT_ANDROID_APP_ICON
        "${CMAKE_CURRENT_SOURCE_DIR}/icons/icon-512.png")
    set_property(TARGET soundhub PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/android)
    set_property(TARGET soundhub PROPERTY QT_ANDROID_PACKAGE_NAME "org.gplyno.soundhub")
    set_property(TARGET soundhub PROPERTY QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}")
    set_property(TARGET soundhub PROPERTY QT_ANDROID_VERSION_CODE 1)
    set_property(TARGET soundhub PROPERTY QT_ANDROID_MIN_SDK_VERSION 23)
    set_property(TARGET soundhub PROPERTY QT_ANDROID_TARGET_SDK_VERSION 34)
endif()

install(TARGETS soundhub
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)
