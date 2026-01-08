
add_library(sokol sokol.c)
target_include_directories(sokol PUBLIC ./)

if(APPLE)
    set_source_files_properties(sokol.c PROPERTIES LANGUAGE OBJC)
    target_link_libraries(sokol PUBLIC "-framework Cocoa" "-framework QuartzCore" "-framework Metal" "-framework MetalKit" "-framework AudioToolbox" "-framework OpenGL")
endif()

if(MUD_GLES3)
    target_compile_definitions (sokol PRIVATE MUD_GLES3)
endif()

target_compile_definitions(sokol PUBLIC NK_INCLUDE_FIXED_TYPES NK_INCLUDE_STANDARD_IO NK_INCLUDE_DEFAULT_ALLOCATOR NK_INCLUDE_VERTEX_BUFFER_OUTPUT NK_INCLUDE_FONT_BAKING NK_INCLUDE_DEFAULT_FONT NK_INCLUDE_STANDARD_VARARGS)