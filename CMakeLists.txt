cmake_minimum_required(VERSION 3.22)

project(scanlation_tool
  VERSION 0.1.0
  DESCRIPTION "Standalone scanlation and ad page detector for manga archives"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
  enable_language(OBJCXX)
endif()

option(SCANLATION_TOOL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_executable(scanlation_tool
  src/main.cpp
  src/classifier.cpp
  src/file_utils.cpp
  src/ocr.cpp
  src/process.cpp
  src/scanner.cpp
  src/tesseract_backend.cpp
)

target_include_directories(scanlation_tool PRIVATE include)

if(APPLE)
  target_sources(scanlation_tool PRIVATE
    src/apple_vision_backend.mm
    src/image_features_macos.mm
  )
  target_compile_definitions(scanlation_tool PRIVATE SCANLATION_TOOL_APPLE=1)
  target_link_libraries(scanlation_tool PRIVATE
    "-framework Foundation"
    "-framework Vision"
    "-framework CoreImage"
    "-framework CoreGraphics"
    "-framework ImageIO"
  )
else()
  target_sources(scanlation_tool PRIVATE src/image_features_stub.cpp)
endif()

if(MSVC)
  target_compile_options(scanlation_tool PRIVATE /W4)
  if(SCANLATION_TOOL_WARNINGS_AS_ERRORS)
    target_compile_options(scanlation_tool PRIVATE /WX)
  endif()
else()
  target_compile_options(scanlation_tool PRIVATE -Wall -Wextra -Wpedantic)
  if(SCANLATION_TOOL_WARNINGS_AS_ERRORS)
    target_compile_options(scanlation_tool PRIVATE -Werror)
  endif()
endif()

install(TARGETS scanlation_tool RUNTIME DESTINATION bin)

set(CPACK_PACKAGE_NAME "scanlation-tool")
set(CPACK_PACKAGE_VENDOR "Scanlation Tool")
set(CPACK_PACKAGE_CONTACT "maintainers@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/example/scanlation-tool")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

if(WIN32)
  set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
  set(CPACK_GENERATOR "TGZ")
else()
  set(CPACK_GENERATOR "TGZ;DEB;RPM")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "tesseract-ocr, unzip")
  set(CPACK_RPM_PACKAGE_REQUIRES "tesseract, unzip")
endif()

include(CPack)
