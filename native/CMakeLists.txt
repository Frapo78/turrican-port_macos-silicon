cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(turrican2_native LANGUAGES CXX)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(t2_native_core STATIC core/game.cpp assets/adf_probe.cpp)
target_include_directories(t2_native_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

option(T2_NATIVE_WITH_SDL "Build prototype graphical native game app" ON)
option(T2_NATIVE_TESTS "Build native engine unit tests" ON)
if(T2_NATIVE_TESTS)
  enable_testing()
  add_executable(t2_native_tests tests/test_core.cpp)
  target_link_libraries(t2_native_tests PRIVATE t2_native_core)
  add_test(NAME t2_native_core COMMAND t2_native_tests)
endif()

if(T2_NATIVE_WITH_SDL)
  if(NOT TARGET SDL2::SDL2)
    find_package(SDL2 REQUIRED)
  endif()
  add_executable(turrican2_native main.cpp)
  target_link_libraries(turrican2_native PRIVATE t2_native_core SDL2::SDL2)
  if(APPLE)
    enable_language(OBJCXX)
    target_sources(turrican2_native PRIVATE platform/macos/onboarding.mm)
    target_link_libraries(turrican2_native PRIVATE "-framework AppKit")
    set_target_properties(turrican2_native PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_GUI_IDENTIFIER "org.frapo78.turrican2.native"
      MACOSX_BUNDLE_BUNDLE_NAME "Turrican II Native Prototype"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "0.0.1"
      MACOSX_BUNDLE_BUNDLE_VERSION "1")
  endif()
endif()
