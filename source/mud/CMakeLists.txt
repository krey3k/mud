
# Gather all source files
file(GLOB_RECURSE MUD_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

# Create executable
add_executable(mud WIN32 ${MUD_SOURCE})

# Include directories for subfolder headers
target_include_directories(mud PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Platform-specific libraries
set(MUD_LINK_LIBRARIES atomix cjson dr_libs fs minigamepad mtlib mud_tracy pocketpy sokol stb stc thread)

if (MUD_PROFILING)
  list (APPEND MUD_LINK_LIBRARIES TracyClient)
endif()


if(APPLE)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/doom/d_main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/m_misc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/system/i_app.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui_main.c
        PROPERTIES LANGUAGE OBJC
    )
elseif(MSVC)
    list(APPEND MUD_LINK_LIBRARIES 
        ws2_32 
    )
elseif(MINGW)
    list(APPEND MUD_LINK_LIBRARIES 
        d3d11 
        pthread
        winmm
        ws2_32
        kernel32
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT EMSCRIPTEN)
    list(APPEND MUD_LINK_LIBRARIES 
        asound 
        X11 
        X11::Xi 
        X11::Xcursor 
        dl 
        pthread 
        m
    )
    if(MUD_GLES3)
        list(APPEND MUD_LINK_LIBRARIES
            OpenGL::EGL
            OpenGL::GLES3
        )
    else()
        list(APPEND MUD_LINK_LIBRARIES 
            OpenGL::GL
        )
    endif()
endif()

# Link libraries
target_link_libraries(mud ${MUD_LINK_LIBRARIES})

# Target properties
if (NOT EMSCRIPTEN)
    set_target_properties(mud PROPERTIES
        LINKER_LANGUAGE C
        DEBUG_POSTFIX "_DEBUG"
        RELWITHDEBINFO_POSTFIX "_RELWITHDEBINFO"
    )
endif()

if (EMSCRIPTEN)
  set (SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  set (DEST_DIR "${CMAKE_SOURCE_DIR}/web/site")

  target_link_options(mud PRIVATE "--pre-js=${DEST_DIR}/mud-data.js")

  add_custom_command(
  TARGET mud
  PRE_BUILD
  COMMAND python3 ${EMPACKAGER} ${DEST_DIR}/mud.data --preload
    ${CMAKE_SOURCE_DIR}/web/preload@/
    --js-output=${DEST_DIR}/mud-data.js
    --use-preload-cache
    --no-node
    --lz4
  )

endif()
