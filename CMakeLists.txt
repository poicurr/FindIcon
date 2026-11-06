cmake_minimum_required(VERSION 3.24)

# CMP0091 must be NEW before project() initializes the MSVC ABI.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(findicon VERSION 0.1.0 DESCRIPTION "Local icon search and PNG previews" LANGUAGES C CXX)

option(FINDICON_FETCH_DEPS "Download and statically build pinned dependencies" ON)
option(FINDICON_BUILD_TESTS "Build the findicon tests" OFF)
option(FINDICON_ENABLE_PACKAGING "Enable CPack source and binary archives" OFF)
option(FINDICON_STATIC_RUNTIME "Use static compiler runtimes on Windows" ON)
if(NOT FINDICON_STATIC_RUNTIME)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)
include(cmake/Dependencies.cmake)
findicon_configure_dependencies()

add_library(findicon_core STATIC
  src/Catalog.cpp
  src/Preview.cpp
  src/Font.cpp
  src/Output.cpp
  thirdparty/DokuText/Bitmap.cpp
  thirdparty/DokuText/PngWriter.cpp
)
target_include_directories(findicon_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/DokuText"
)
target_include_directories(findicon_core SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")
target_link_libraries(findicon_core PUBLIC Freetype::Freetype PNG::PNG)
target_compile_features(findicon_core PUBLIC cxx_std_20)

add_executable(findicon src/Main.cpp)
target_link_libraries(findicon PRIVATE findicon_core)
set_target_properties(findicon PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
if(MINGW)
  target_link_options(findicon PRIVATE -municode)
  if(FINDICON_STATIC_RUNTIME)
    target_link_options(findicon PRIVATE -static)
  endif()
endif()

function(findicon_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()
findicon_enable_warnings(findicon_core)
findicon_enable_warnings(findicon)

# Keep the runtime bundle usable after copying the executable and assets elsewhere.
# This target also refreshes changed data files without relinking the program.
add_custom_target(findicon_assets ALL
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:findicon>/assets"
  VERBATIM
)
add_dependencies(findicon findicon_assets)

install(TARGETS findicon RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY assets/ DESTINATION "${CMAKE_INSTALL_DATADIR}/findicon")
install(FILES README.md README.ja.md THIRD_PARTY.md
  DESTINATION "${CMAKE_INSTALL_DATADIR}/findicon")
install(DIRECTORY .agents/skills/findicon
  DESTINATION "${CMAKE_INSTALL_DATADIR}/findicon/skills")
install(DIRECTORY thirdparty/ DESTINATION "${CMAKE_INSTALL_DATADIR}/findicon/licenses"
  FILES_MATCHING PATTERN "LICENSE*" PATTERN "Copyright*" PATTERN "README.md"
  PATTERN "OFL*" PATTERN "license-audit.md" PATTERN "NoticeSources.json"
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/findicon")
endif()
if(MINGW AND LLVM_MINGW_ROOT)
  install(FILES "${LLVM_MINGW_ROOT}/LICENSE.TXT"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/findicon/licenses/LLVM")
  install(DIRECTORY "${LLVM_MINGW_ROOT}/x86_64-w64-mingw32/share/mingw32/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/findicon/licenses/MinGW"
    FILES_MATCHING PATTERN "COPYING*")
endif()

if(FINDICON_BUILD_TESTS)
  enable_testing()
  findicon_configure_test_dependencies()
  set(FINDICON_EXTRA_TEST_SOURCES "" CACHE STRING "Additional findicon test sources")
  add_executable(findicon_tests
    tests/CatalogTest.cpp
    tests/RenderingTest.cpp
    tests/OutputTest.cpp
    ${FINDICON_EXTRA_TEST_SOURCES}
  )
  target_link_libraries(findicon_tests PRIVATE findicon_core Catch2::Catch2WithMain)
  target_compile_definitions(findicon_tests PRIVATE
    FINDICON_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    FINDICON_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-output"
    FINDICON_EXECUTABLE="$<TARGET_FILE:findicon>"
  )
  findicon_enable_warnings(findicon_tests)
  if(MINGW AND FINDICON_STATIC_RUNTIME)
    target_link_options(findicon_tests PRIVATE -static)
  endif()
  add_dependencies(findicon_tests findicon)
  add_test(NAME findicon.tests COMMAND findicon_tests)
  set_tests_properties(findicon.tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  if(NOT CMAKE_CROSSCOMPILING)
    add_test(NAME findicon.cli
      COMMAND "${CMAKE_COMMAND}"
              "-DFINDICON_EXE=$<TARGET_FILE:findicon>"
              "-DFINDICON_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-output/Cli"
              -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/CliTest.cmake"
    )
  endif()
endif()

if(FINDICON_ENABLE_PACKAGING)
  set(CPACK_PACKAGE_NAME findicon)
  set(CPACK_PACKAGE_VENDOR findicon)
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Local Nerd Fonts search and PNG contact sheets")
  set(CPACK_GENERATOR "ZIP;TGZ")
  set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
  set(CPACK_SOURCE_IGNORE_FILES "/build[^/]*/;/\\.git/;~$;/out/;\\.zip$;\\.tar\\.gz$")
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
  include(CPack)
endif()
