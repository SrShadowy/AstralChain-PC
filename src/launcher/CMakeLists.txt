cmake_minimum_required(VERSION 3.20)

set(AC_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
if(NOT DEFINED AC_APP_VERSION OR AC_APP_VERSION STREQUAL "")
    if(EXISTS "${AC_VERSION_FILE}")
        file(READ "${AC_VERSION_FILE}" AC_APP_VERSION_RAW)
        string(STRIP "${AC_APP_VERSION_RAW}" AC_APP_VERSION)
    else()
        set(AC_APP_VERSION "1.0.0")
    endif()
endif()
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" AC_CMAKE_VERSION "${AC_APP_VERSION}")
if(AC_CMAKE_VERSION STREQUAL "")
    set(AC_CMAKE_VERSION "1.0.0")
endif()

project(AstralChainLauncher VERSION "${AC_CMAKE_VERSION}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)

add_executable(astral_chain_launcher
    include/Config.hpp
    include/ProcessRunner.hpp
    include/ControlsDialog.hpp
    include/SettingsDialog.hpp
    include/MouseSettingsWidget.hpp
    include/SetupDialog.hpp
    include/MainWindow.hpp
    include/LogWidget.hpp
    include/Theme.hpp
    include/LauncherIcons.hpp
    include/CustomButton.hpp
    src/main.cpp
    src/Config.cpp
    src/ProcessRunner.cpp
    src/ControlsDialog.cpp
    src/SettingsDialog.cpp
    src/MouseSettingsWidget.cpp
    src/SetupDialog.cpp
    src/MainWindow.cpp
    src/MainWindowUi.cpp
    src/LogWidget.cpp
    src/Theme.cpp
    src/LauncherIcons.cpp
    src/CustomButton.cpp
    resources/launcher.qrc
)

option(AC_PORTABLE_BUILD "Build portable x86-64 binary without host CPU extensions" OFF)

target_include_directories(astral_chain_launcher PRIVATE include)
target_compile_definitions(astral_chain_launcher PRIVATE
    AC_APP_VERSION="${AC_APP_VERSION}"
    AC_APP_ID="io.github.srshadowy.AstralChainPC"
    AC_DEVELOPER_NAME="SrShadowy"
)

if(AC_PORTABLE_BUILD AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(astral_chain_launcher PRIVATE -march=x86-64 -mtune=generic)
endif()

target_link_libraries(astral_chain_launcher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
)
