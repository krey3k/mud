
CMAKE_MINIMUM_REQUIRED(VERSION 3.27)

if(APPLE)
    PROJECT(mud C CXX OBJC)
else()
    PROJECT(mud C CXX)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Early out if unsupported platform
if(NOT EMSCRIPTEN AND NOT APPLE AND NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Unsupported target platform!")
endif()

if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "AppleClang")
    set(CLANG true)
else()
    set(CLANG false)
endif()

# Rendering options
if (EMSCRIPTEN)
    option(MUD_GLES3 "Sokol GLES3" ON)
else()
    option(MUD_GLES3 "Sokol GLES3" OFF)
endif()

# Web Player
option(MUD_WEB_MULTITHREADED "Build multithreaded web player" ON)
option(MUD_WEB_SIMD "Build SIMD-enabled web player" ON)

# Development options
option(MUD_SANITIZE "Enable code sanitizing" OFF)
option(MUD_SANITIZE_THREADS "Enable thread sanitizing (No-op with MSVC)" OFF)
option(MUD_SANITIZE_UB "Enable undefined behavior sanitizing (No-op with MSVC)" OFF)

option(MUD_PROFILING "Enable Profiling" OFF)

if (EMSCRIPTEN)
  include("${CMAKE_SOURCE_DIR}/cmake/emscripten.cmake")
  add_compile_definitions(MUD_WEB)

  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/web/site)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/web/site)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/web/site)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/web/site)
else()
    # Runtime output directories
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)

    # Build configuration definitions
    add_compile_definitions(
        $<$<CONFIG:Debug>:MUD_DEBUG>
        $<$<CONFIG:RelWithDebInfo>:MUD_DEBUG>
        $<$<CONFIG:Release>:MUD_RELEASE>
        $<$<CONFIG:MinSizeRel>:MUD_RELEASE>
    )
endif()

if (MUD_PROFILING)
  add_compile_definitions(MUD_PROFILING TRACY_ENABLE)
endif()

# Compiler-specific settings
if(MSVC)
    if(NOT CLANG)
        # Get the number of logical cores for parallel build
        cmake_host_system_information(RESULT LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
        math(EXPR COMPILE_CORES "${LOGICAL_CORES} - 1")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP${COMPILE_CORES}")
    endif()

    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /utf-8 /experimental:c11atomics")
    
else()
    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "-static -mwindows ${CMAKE_EXE_LINKER_FLAGS}")
    endif()

    # This needs to be fixed, has to do with action function prototype 
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-non-prototype")

    # Sanitizer options (mutually exclusive)
    if((MUD_SANITIZE AND MUD_SANITIZE_THREADS) OR 
       (MUD_SANITIZE AND MUD_SANITIZE_UB) OR 
       (MUD_SANITIZE_THREADS AND MUD_SANITIZE_UB))
        message(FATAL_ERROR "Enable only one of MUD_SANITIZE, MUD_SANITIZE_THREADS or MUD_SANITIZE_UB!")
        
    elseif(MUD_SANITIZE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fno-common")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        if(NOT CLANG)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libasan")
        endif()
        
    elseif(MUD_SANITIZE_THREADS)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -g -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread -g")
        if(NOT CLANG)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libtsan")
        endif()
        
    elseif(MUD_SANITIZE_UB)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -g -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined -g")
        if(NOT CLANG)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libubsan")
        endif()
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT EMSCRIPTEN AND NOT MINGW)
    find_package(ALSA REQUIRED)
    find_package(X11 COMPONENTS Xi Xcursor REQUIRED)
    if(MUD_GLES3)
        find_package(OpenGL COMPONENTS EGL GLES3 REQUIRED)
    else()
        find_package(OpenGL REQUIRED)
    endif()
endif()

add_subdirectory(source)
