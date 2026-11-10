cmake_minimum_required(VERSION 3.16)
project(turrican2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(SDL2_LIBRARIES)
  # Given directly (the R36S build links a stub against a sysroot whose
  # CMake files name /usr, see nix/r36s.nix).
  add_library(SDL2::SDL2 INTERFACE IMPORTED)
  set_target_properties(SDL2::SDL2 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${SDL2_LIBRARIES}")
else()
  find_package(SDL2 REQUIRED)
endif()
include(GNUInstallDirs)

add_library(m68k STATIC
  src/m68k/cpu.cpp
  src/m68k/cpu.hpp
  src/m68k/ops.hpp
)
target_include_directories(m68k PUBLIC src)

add_library(amiga STATIC
  src/amiga/machine.cpp
  src/amiga/machine.hpp
  src/amiga/custom.cpp
  src/amiga/cia.cpp
  src/amiga/cia.hpp
  src/amiga/disk.cpp
  src/amiga/disk.hpp
  src/amiga/hle.cpp
  src/amiga/hle.hpp
  src/amiga/hunk.hpp
)
target_link_libraries(amiga PUBLIC m68k)

# Turrican II game knowledge (memory map, cheats, level select).
add_library(t2 STATIC
  src/t2/game.cpp
  src/t2/game.hpp
)
target_link_libraries(t2 PUBLIC amiga)

add_executable(turrican2 src/main.cpp)
target_link_libraries(turrican2 PRIVATE t2 SDL2::SDL2)

# Keep the existing CLI executable on Linux and provide a Finder-launchable
# .app on macOS, without introducing a macOS dependency on other platforms.
if(APPLE)
  enable_language(OBJCXX)
  set_target_properties(turrican2 PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.frapo78.turrican2"
    MACOSX_BUNDLE_BUNDLE_NAME "Turrican II"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
  )
  target_sources(turrican2 PRIVATE src/macos/select_adf.mm)
  target_link_libraries(turrican2 PRIVATE "-framework AppKit")
endif()
target_compile_definitions(turrican2 PRIVATE
  T2_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/turrican2")

# CPU conformance test against the SingleStepTests 68000 suite:
#   ./build/cpu_sst path/to/m68000/v1/*.json.bin
add_executable(cpu_sst tests/cpu_sst.cpp)
target_link_libraries(cpu_sst PRIVATE m68k)

if(APPLE)
  install(TARGETS turrican2 BUNDLE DESTINATION ".")
else()
  install(TARGETS turrican2 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Separate, 68000-free C++20 native prototype (not yet the original game).
# Target turrican2_native intentionally links no amiga/m68k libraries.
enable_testing()
add_subdirectory(native)
