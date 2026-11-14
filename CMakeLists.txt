cmake_minimum_required(VERSION 3.16)

project(RemoteHub_App VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS OpenGL OpenGLWidgets)
endif()

# KDE can provide compositor blur. Other Linux desktops retain real alpha
# transparency without requiring a desktop-specific dependency.
if(UNIX AND NOT APPLE)
    if(QT_VERSION_MAJOR EQUAL 6)
        find_package(KF6WindowSystem QUIET)
    else()
        find_package(KF5WindowSystem 5.82 QUIET)
    endif()
endif()

set(UI_SOURCES
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        hubview.cpp
        hubview.h
        glasswindow.cpp
        glasswindow.h
)
set(PROJECT_SOURCES main.cpp ${UI_SOURCES})

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(RemoteHub_App
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET RemoteHub_App APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(RemoteHub_App SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(RemoteHub_App
            ${PROJECT_SOURCES}
        )
    endif()
endif()

function(configure_hub_target target)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${target} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(${target} PRIVATE Qt6::OpenGL Qt6::OpenGLWidgets)
    endif()
    if(WIN32)
        target_link_libraries(${target} PRIVATE dwmapi)
    endif()
    if(TARGET KF6::WindowSystem)
        target_link_libraries(${target} PRIVATE KF6::WindowSystem)
        target_compile_definitions(${target} PRIVATE REMOTEHUB_HAS_KWINDOWSYSTEM)
    elseif(TARGET KF5::WindowSystem)
        target_link_libraries(${target} PRIVATE KF5::WindowSystem)
        target_compile_definitions(${target} PRIVATE REMOTEHUB_HAS_KWINDOWSYSTEM)
    endif()
endfunction()
configure_hub_target(RemoteHub_App)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.RemoteHub_App)
endif()
set_target_properties(RemoteHub_App PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS RemoteHub_App
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(RemoteHub_App)
endif()

option(REMOTEHUB_BUILD_TESTS "Build interactive OpenGL smoke tests (requires a display)" OFF)
if(REMOTEHUB_BUILD_TESTS)
    enable_testing()
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(hubview_smoke tests/hubview_smoke.cpp ${UI_SOURCES})
    configure_hub_target(hubview_smoke)
    target_link_libraries(hubview_smoke PRIVATE Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME hubview_smoke COMMAND hubview_smoke)
endif()
