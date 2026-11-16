cmake_minimum_required(VERSION 4.0)

find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)
find_package(cmake-prelude REQUIRED PATHS node_modules/cmake-prelude)
find_package(cmake-test REQUIRED PATHS node_modules/cmake-test)

project(js LANGUAGES C VERSION 0.0.0)

fetch_package("github:holepunchto/libintrusive")

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

get_property(shared GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS)

set_target_properties(
  qjs
  PROPERTIES
  POSITION_INDEPENDENT_CODE ${shared}
)

add_library_targets(
  js
  FEATURES
    c_std_99
  SOURCES
    src/js.c
  LINK
    utf::<TYPE>
  STATIC_LINK
    uv_a
    $<TARGET_PROPERTY:qjs,INTERFACE_LINK_LIBRARIES>
  SHARED_LINK
    uv
  EXPORT OFF
)

target_link_libraries(
  js
  PRIVATE
    qjs
    intrusive::headers
)

target_sources(
  js_static
  PRIVATE
    $<TARGET_OBJECTS:qjs>
)

if(TARGET js_shared)
  target_link_libraries(
    js_shared
    PRIVATE
      qjs
  )
endif()

target_include_directories(
  js_headers
  INTERFACE
    $<TARGET_PROPERTY:uv,INTERFACE_INCLUDE_DIRECTORIES>
)

fetch_package("github:holepunchto/libjs" SOURCE_DIR js)

target_sources(
  js_headers
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${js}/include
    FILES ${js}/include/js.h
)

add_test_subdirectory(test)
