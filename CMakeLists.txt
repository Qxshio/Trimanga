cmake_minimum_required(VERSION 3.22)

project(trimanga
  VERSION 0.1.0
  DESCRIPTION "Production-grade manga archive cleanup tool for detecting removable scanlation and ad pages"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TRIMANGA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(TRIMANGA_WITH_SDL "Build the interactive SDL2 previewer when SDL2 is available" ON)

set(TRIMANGA_PREVIEW_SOURCE src/preview/previewer_stub.cpp)
if(TRIMANGA_WITH_SDL)
  find_package(SDL2 QUIET)
  if(SDL2_FOUND)
    set(TRIMANGA_PREVIEW_SOURCE src/preview/sdl_previewer.cpp)
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
  ${TRIMANGA_PREVIEW_SOURCE}
)

target_include_directories(trimanga PRIVATE include third_party/stb)

if(TRIMANGA_WITH_SDL AND SDL2_FOUND)
  target_compile_definitions(trimanga PRIVATE TRIMANGA_WITH_SDL=1)
  target_include_directories(trimanga PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(trimanga PRIVATE ${SDL2_LIBRARIES})
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

install(TARGETS trimanga RUNTIME DESTINATION bin)

enable_testing()
add_test(NAME trimanga_version COMMAND $<TARGET_FILE:trimanga> --version)
add_test(NAME trimanga_help COMMAND $<TARGET_FILE:trimanga> --help)

set(CPACK_PACKAGE_NAME "trimanga")
set(CPACK_PACKAGE_VENDOR "Trimanga")
set(CPACK_PACKAGE_CONTACT "Qxshio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Qxshio/Trimanga")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(WIN32)
  set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
  set(CPACK_GENERATOR "TGZ")
else()
  set(CPACK_GENERATOR "TGZ;DEB;RPM")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "unzip")
  set(CPACK_RPM_PACKAGE_REQUIRES "unzip")
endif()

include(CPack)
