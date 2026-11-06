cmake_minimum_required(VERSION 4.0)

find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)

project(js C)

set(QJS_ENABLE_INSTALL OFF CACHE BOOL "Enable QuickJS installation")

fetch_package(
  "github:quickjs-ng/quickjs@0.18.0"
  PATCHES
    patches/001-type-predicates.patch
    patches/002-defer-module-resolution.patch
    patches/003-arraybuffer-allocator.patch
    patches/004-bigint-words.patch
    patches/005-string-buffer.patch
    patches/006-gc-hook.patch
    patches/007-host-defined.patch
    patches/008-dataview.patch
    patches/009-link-module.patch
    patches/010-error-location.patch
    patches/011-weakref.patch
)

set_target_properties(
  qjs
  PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

add_library(js OBJECT)

fetch_package("github:holepunchto/libjs" SOURCE_DIR js)
fetch_package("github:holepunchto/libintrusive")

set_target_properties(
  js
  PROPERTIES
  C_STANDARD 99
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  js
  PRIVATE
    src/js.c
)

target_include_directories(
  js
  INTERFACE
    include
  PUBLIC
    ${js}/include
  PRIVATE
    $<TARGET_PROPERTY:qjs,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:uv,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:utf,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:intrusive,INTERFACE_INCLUDE_DIRECTORIES>
)

add_library(js_shared SHARED)

set_target_properties(
  js_shared
  PROPERTIES
  OUTPUT_NAME js
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(
  js_shared
  INTERFACE
    $<TARGET_PROPERTY:js,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(
  js_shared
  PUBLIC
    uv
    utf_shared
  PRIVATE
    js
    qjs
)

add_library(js_static STATIC)

target_sources(
  js_static
  PRIVATE
    $<TARGET_OBJECTS:qjs>
)

set_target_properties(
  js_static
  PROPERTIES
  OUTPUT_NAME js
  PREFIX lib
)

target_include_directories(
  js_static
  INTERFACE
    $<TARGET_PROPERTY:js,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(
  js_static
  PUBLIC
    $<TARGET_PROPERTY:qjs,INTERFACE_LINK_LIBRARIES>
    uv_a
    utf_static
  PRIVATE
    js
)

install(TARGETS js_shared js_static)

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()

  add_subdirectory(test)
endif()
