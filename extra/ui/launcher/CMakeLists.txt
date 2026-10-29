cmake_minimum_required(VERSION 3.20)
project(AstralChainLauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(astral_chain_launcher
    include/Config.hpp
    include/ProcessRunner.hpp
    include/ControlsDialog.hpp
    include/SettingsDialog.hpp
    include/SetupDialog.hpp
    include/MainWindow.hpp
    include/LogWidget.hpp
    include/Theme.hpp
    src/main.cpp
    src/Config.cpp
    src/ProcessRunner.cpp
    src/ControlsDialog.cpp
    src/SettingsDialog.cpp
    src/SetupDialog.cpp
    src/MainWindow.cpp
    src/MainWindowUi.cpp
    src/LogWidget.cpp
    src/Theme.cpp
)

target_include_directories(astral_chain_launcher PRIVATE include)

target_link_libraries(astral_chain_launcher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
