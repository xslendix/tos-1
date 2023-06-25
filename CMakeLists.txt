# If dreams can come true what does that say about nightmares?
cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

# thank you justin
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR} AND NOT WIN32)
	message(FATAL_ERROR "You can not use CMake to build from the root of it's source tree! Remove the CMakeCache.txt file from this directory, then create a separate directory (either below this directory or elsewhere), and then re-run CMake from there.")
endif(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR} AND NOT WIN32)

# lmao lol
set(PROJECT_MAJOR_VERSION 4)
set(PROJECT_MINOR_VERSION 2)
set(PROJECT_PATCH_LEVEL   0)
set(VERSION_SIMPLE "${PROJECT_MAJOR_VERSION}.${PROJECT_MINOR_VERSION}.${PROJECT_PATCH_LEVEL}")

# set a default build type if none was specified
set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
	  STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
	"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
find_program(CLANG   clang)
find_program(CLANGXX clang++)

if (NOT WIN32)
  # prefer clang instead of gcc (or whatever garbage compiler they're using) This can be disabled with
  # the NO_CLANG option
  option(NO_CLANG "Don't prefer clang for compilation" OFF)
  if (NOT NO_CLANG)
    if (CLANG)
      set(CMAKE_C_COMPILER ${CLANG})
    endif (CLANG)
    if (CLANGXX)
      set(CMAKE_CXX_COMPILER ${CLANGXX})
    endif (CLANGXX)
  endif (NOT NO_CLANG)
endif (NOT WIN32)

project(tos
  VERSION ${VERSION_SIMPLE}
  DESCRIPTION "TempleOS in userspace"
  LANGUAGES C CXX
)

message(STATUS "${PROJECT_NAME} version: ${VERSION_SIMPLE}")
option(WARNINGS_AS_ERRORS "Set certain warnings to be treated as errors by the compiler" ON)

set(CPP_EXECUTABLE_SOURCE_FILES main.cxx dbg.cxx sdl_window.cxx runtime.cxx tos_aot.cxx mem.cxx multic.cxx vfs.cxx TOSPrint.cxx)
set(C_EXECUTABLE_SOURCE_FILES sound.c ext/dyad.c ext/argtable3.c)

set_source_files_properties(CPP_EXECUTABLE_SOURCE_FILES
  PROPERTIES
    LANGUAGE CXX
    COMPILE_FLAGS "-fno-rtti -fno-exceptions"
)

set_source_files_properties(C_EXECUTABLE_SOURCE_FILES}
  PROPERTIES
    LANGUAGE C
    COMPILE_FLAGS -Werror=implicit-function-declaration
)

add_executable(${PROJECT_NAME} ${C_EXECUTABLE_SOURCE_FILES} ${CPP_EXECUTABLE_SOURCE_FILES})

# outputs bin to src tree dir
set_target_properties(${PROJECT_NAME}
  PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
    C_EXTENSIONS YES
    LINKER_LANGUAGE CXX
)

target_compile_features(${PROJECT_NAME}
  PRIVATE
    cxx_thread_local
    cxx_right_angle_brackets
    cxx_range_for
    cxx_auto_type
    cxx_nullptr
    cxx_alignof
    cxx_alignas
)

target_compile_options(${PROJECT_NAME}
  PRIVATE
    -Werror=shadow
    -fno-omit-frame-pointer
    -m64
)


# libm
target_link_libraries(${PROJECT_NAME} PRIVATE m)

# sdl2 dep
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)

# libuv dep
include(Findlibuv)
if(LIBUV_FOUND)
  add_library(libuv INTERFACE)
  target_link_libraries(libuv INTERFACE PkgConfig::LIBUV)
else()
  message(FATAL_ERROR install libuv)
endif()
include_directories(${LIBUV_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBUV_LIBRARIES}) # uv)

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    synchronization shlwapi shcore dbghelp winmm -static)
  # no winpthreads(i hate pthreads and SuspendThread() is rad)
  set(CMAKE_THREAD_PREFER_PTHREAD OFF)
  set(THREADS_PREFER_PTHREAD_FLAG OFF)
  set(C_ABI_NAME "WIN64")
  set(BIN_FORMAT "win64")
elseif((CMAKE_SYSTEM_NAME STREQUAL "Linux") OR
       (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD"))
  target_sources("${PROJECT_NAME}" PRIVATE ext/linenoise.c)
  # ffi
  set(C_ABI_NAME "SYSV")
  set(BIN_FORMAT "elf64")
  # uses this because cmake cant detect builtins
  include(CheckCXXSourceCompiles)
  check_cxx_source_compiles(
    "#include <stdint.h>
     int main() {
       uint32_t i = 1, j = 1;
       __atomic_compare_exchange_n(
         &i, &j, 0, false,
         __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST
      );
      return !(i == 0 && j == 1);
    }" HAS_ATOMIC_LOAD_N)
  if (NOT HAS_ATOMIC_LOAD_N)
    message(FATAL_ERROR "No builtin atomics, wtf are you using as your compiler?")
  endif()
else()
    message(FATAL_ERROR "Unsupported, stop using joke operating systems")
endif()

# ffi
set(FFI_NAME "FFI_${C_ABI_NAME}")
add_custom_command(OUTPUT "${FFI_NAME}.obj"
    COMMAND yasm
    ARGS -f ${BIN_FORMAT} -o "${CMAKE_CURRENT_BINARY_DIR}/${FFI_NAME}.obj" "${CMAKE_CURRENT_SOURCE_DIR}/${FFI_NAME}.yasm"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${FFI_NAME}.yasm"
)
add_library(FFI "${FFI_NAME}.obj")
set_target_properties(FFI PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(${PROJECT_NAME} PRIVATE FFI)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
