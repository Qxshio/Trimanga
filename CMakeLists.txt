cmake_minimum_required(VERSION 3.22)

project(trimanga
  VERSION 0.1.0
  DESCRIPTION "Production-grade manga archive cleanup tool for detecting removable scanlation and ad pages"
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TRIMANGA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(TRIMANGA_WITH_SDL "Build the interactive SDL2 previewer when SDL2 is available" ON)

set(TRIMANGA_PREVIEW_SOURCE src/preview/previewer_stub.cpp)
if(TRIMANGA_WITH_SDL)
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    set(TRIMANGA_PREVIEW_SOURCE src/preview/previewer_spawn.cpp src/preview/preview_ipc.cpp)
  endif()
endif()

add_executable(trimanga
  src/main.cpp
  src/classifier.cpp
  src/file_utils.cpp
  src/image/image_loader.cpp
  src/image/page_features.cpp
  src/process.cpp
  src/scanner.cpp
  src/detectors/scanlation_detector.cpp
  third_party/miniz/miniz.c
  third_party/miniz/miniz_tdef.c
  third_party/miniz/miniz_tinfl.c
  third_party/miniz/miniz_zip.c
  ${TRIMANGA_PREVIEW_SOURCE}
)

target_include_directories(trimanga PRIVATE include third_party/stb third_party/miniz)

if(TRIMANGA_WITH_SDL AND SDL2_FOUND)
  add_executable(trimanga-preview
    src/preview/sdl_preview_main.cpp
    src/preview/sdl_previewer.cpp
    src/preview/preview_ipc.cpp
    src/image/image_loader.cpp
    src/preview/sdl/drawing.cpp
    src/preview/sdl/layout.cpp
    src/preview/sdl/page_card.cpp
    src/preview/sdl/texture_cache.cpp
    src/preview/sdl/widgets.cpp
  )
  target_compile_definitions(trimanga-preview PRIVATE TRIMANGA_WITH_SDL=1)
  target_include_directories(trimanga-preview PRIVATE include third_party/stb ${SDL2_INCLUDE_DIRS})
  target_link_libraries(trimanga-preview PRIVATE ${SDL2_LIBRARIES})
endif()

if(MSVC)
  target_compile_options(trimanga PRIVATE /W4)
  if(TRIMANGA_WARNINGS_AS_ERRORS)
    target_compile_options(trimanga PRIVATE /WX)
  endif()
else()
  target_compile_options(trimanga PRIVATE -Wall -Wextra -Wpedantic)
  if(TRIMANGA_WARNINGS_AS_ERRORS)
    target_compile_options(trimanga PRIVATE -Werror)
  endif()
endif()

enable_testing()
add_test(NAME trimanga_version COMMAND $<TARGET_FILE:trimanga> --version)
add_test(NAME trimanga_help COMMAND $<TARGET_FILE:trimanga> --help)
