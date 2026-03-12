cmake_minimum_required(VERSION 3.21)

project(video-preview-native LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Concurrent Core Gui Widgets OpenGL OpenGLWidgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

qt_standard_project_setup()

get_filename_component(VPG_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
find_program(CARGO_EXECUTABLE cargo REQUIRED)

add_custom_target(vpg-cli-build
  COMMAND ${CARGO_EXECUTABLE} build -p vpg-cli
  WORKING_DIRECTORY ${VPG_REPO_ROOT}
  USES_TERMINAL
  COMMENT "Building Rust CLI bridge"
)

qt_add_executable(video-preview-native
  src/CropOverlayWidget.cpp
  src/CropOverlayWidget.hpp
  src/FitPreviewWidget.cpp
  src/FitPreviewWidget.hpp
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.hpp
  src/MpvWidget.cpp
  src/MpvWidget.hpp
  src/ProjectInfo.hpp
  src/RustBridge.cpp
  src/RustBridge.hpp
  src/TimelineWidget.cpp
  src/TimelineWidget.hpp
)

add_dependencies(video-preview-native vpg-cli-build)

target_compile_definitions(video-preview-native PRIVATE
  VPG_REPO_ROOT="${VPG_REPO_ROOT}"
)

target_link_libraries(video-preview-native PRIVATE
  Qt6::Concurrent
  Qt6::Core
  Qt6::Gui
  Qt6::OpenGL
  Qt6::OpenGLWidgets
  Qt6::Widgets
  PkgConfig::MPV
)
