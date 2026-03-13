cmake_minimum_required(VERSION 3.21)

project(video-preview-native LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Concurrent Core Gui Widgets OpenGL OpenGLWidgets)

if(WIN32)
  set(LIBMPV_DIR "$ENV{LIBMPV_DIR}" CACHE PATH "Path to a prepared libmpv directory containing include/, lib/, and bin/")

  if(NOT LIBMPV_DIR)
    message(FATAL_ERROR "LIBMPV_DIR must point to a prepared libmpv directory when building on Windows.")
  endif()

  find_path(VPG_LIBMPV_INCLUDE_DIR
    NAMES mpv/client.h
    HINTS
      "${LIBMPV_DIR}/include"
    REQUIRED
  )

  find_library(VPG_LIBMPV_IMPORT_LIB
    NAMES libmpv mpv libmpv-2
    HINTS
      "${LIBMPV_DIR}/lib"
    REQUIRED
  )

  find_file(VPG_LIBMPV_RUNTIME_DLL
    NAMES libmpv-2.dll mpv-2.dll
    HINTS
      "${LIBMPV_DIR}/bin"
      "${LIBMPV_DIR}"
    REQUIRED
  )

  add_library(VPG::MPV SHARED IMPORTED)
  set_target_properties(VPG::MPV PROPERTIES
    IMPORTED_IMPLIB "${VPG_LIBMPV_IMPORT_LIB}"
    IMPORTED_LOCATION "${VPG_LIBMPV_RUNTIME_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${VPG_LIBMPV_INCLUDE_DIR}"
  )
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

  add_library(VPG::MPV INTERFACE IMPORTED)
  target_link_libraries(VPG::MPV INTERFACE PkgConfig::MPV)
endif()

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
  src/InteractiveSheetWidget.cpp
  src/InteractiveSheetWidget.hpp
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.hpp
  src/MpvWidget.cpp
  src/MpvWidget.hpp
  src/ProjectInfo.hpp
  src/RustBridge.cpp
  src/RustBridge.hpp
  src/SheetLayoutState.hpp
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
  VPG::MPV
)

if(WIN32)
  if(TARGET Qt6::qmake)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    if(QT_QMAKE_EXECUTABLE)
      get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    endif()
  endif()

  find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt
    HINTS "${QT_BIN_DIR}"
  )

  if(NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "windeployqt is required when building the native shell on Windows.")
  endif()

  add_custom_target(deploy-video-preview-native
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<IF:$<CONFIG:Debug>,--debug,--release>" --no-translations --compiler-runtime "$<TARGET_FILE:video-preview-native>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VPG_LIBMPV_RUNTIME_DLL}" "$<TARGET_FILE_DIR:video-preview-native>/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VPG_REPO_ROOT}/target/debug/vpg-cli.exe" "$<TARGET_FILE_DIR:video-preview-native>/"
    DEPENDS video-preview-native
    USES_TERMINAL
    COMMENT "Deploying Qt runtime and libmpv for the native shell"
  )
endif()
