add_library(stc
    src/cregex.c
    src/cspan.c
    src/cstr_core.c
    src/cstr_io.c
    src/cstr_utf8.c
    src/csview.c
    src/fmt.c
    src/random.c
    src/stc_core.c
)

target_include_directories(stc PUBLIC include)