include_guard(GLOBAL)
include(FetchContent)

function(findicon_configure_dependencies)
  if(NOT FINDICON_FETCH_DEPS)
    find_package(Freetype REQUIRED)
    find_package(PNG REQUIRED)
    return()
  endif()

  # Each URL is an immutable release commit from the official repository;
  # SHA-256 validates the complete source archive before it is extracted.
  # FreeType 2.15.0 / libpng 1.6.59 / zlib 1.4.
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static dependencies" FORCE)
  set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(zlib
    URL https://codeload.github.com/madler/zlib/tar.gz/99623ef7097352da7993ba614161fd6a311e046e
    URL_HASH SHA256=f0d590dd90243a12f551ee13359bf6c634249c55b316fa2b7dbecabe7ae9a904
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    OVERRIDE_FIND_PACKAGE
  )
  FetchContent_MakeAvailable(zlib)
  # zlib 1.4 gives its static target the ZLIB::ZLIBSTATIC name. Both libpng
  # and FreeType use the conventional ZLIB::ZLIB name in find_package().
  if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
  endif()

  # libpng uses this legacy variable while generating pnglibconf.h.
  # Imported target include paths alone do not reach its preprocessor script.
  set(ZLIB_INCLUDE_DIRS "${zlib_BINARY_DIR}" "${zlib_SOURCE_DIR}")

  set(PNG_SHARED OFF CACHE BOOL "" FORCE)
  set(PNG_STATIC ON CACHE BOOL "" FORCE)
  set(PNG_FRAMEWORK OFF CACHE BOOL "" FORCE)
  set(PNG_TESTS OFF CACHE BOOL "" FORCE)
  set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
  set(PNG_HARDWARE_OPTIMIZATIONS OFF CACHE BOOL "" FORCE)
  # These upstream variables stay inside this function's scope.
  set(SKIP_INSTALL_ALL ON)
  FetchContent_Declare(PNG
    URL https://codeload.github.com/pnggroup/libpng/tar.gz/cd952f49f95bb27154ae77dbb103032d95f6e580
    URL_HASH SHA256=ef1bccad2b2a58eb3c09e06ffe3b72b8604abda8b5011e4c4bb5391989040f26
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    OVERRIDE_FIND_PACKAGE
  )
  FetchContent_MakeAvailable(PNG)
  if(NOT TARGET PNG::PNG)
    add_library(PNG::PNG ALIAS png_static)
  endif()

  # Keep PNG-backed color glyph support and reuse the same zlib instance.
  # Shaping, WOFF2, bzip2 fonts and platform HVF support are unnecessary here.
  set(FT_DISABLE_ZLIB OFF CACHE BOOL "" FORCE)
  set(FT_REQUIRE_ZLIB ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_PNG OFF CACHE BOOL "" FORCE)
  set(FT_REQUIRE_PNG ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_HVF ON CACHE BOOL "" FORCE)
  FetchContent_Declare(freetype
    URL https://codeload.github.com/freetype/freetype/tar.gz/37515cf56ba3349656049edbed89ffc6f6466f2d
    URL_HASH SHA256=9efee8b0508e142f38d1c7e2e192d3e5041aa4bbf6c2ab1a901518211f76a914
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  )
  FetchContent_MakeAvailable(freetype)
  if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
  endif()

  install(FILES "${zlib_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/findicon/licenses/zlib")
  install(FILES "${png_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/findicon/licenses/libpng")
  install(FILES "${freetype_SOURCE_DIR}/docs/FTL.TXT" "${freetype_SOURCE_DIR}/docs/GPLv2.TXT"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/findicon/licenses/FreeType")
endfunction()

function(findicon_configure_test_dependencies)
  if(NOT FINDICON_FETCH_DEPS)
    find_package(Catch2 3 CONFIG REQUIRED)
    return()
  endif()
  set(CATCH_DEVELOPMENT_BUILD OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(Catch2
    URL https://codeload.github.com/catchorg/Catch2/tar.gz/08092139210881f01a34e60c5f2fd6afa0ea4024
    URL_HASH SHA256=c66daf9f31712f673ebdffb074c90b5d7e61ea50adc6a1d2db95e798af027940
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  )
  FetchContent_MakeAvailable(Catch2)
endfunction()
