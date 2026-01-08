add_library(fs fs.c backends/zip/fs_zip.c backends/sub/fs_sub.c backends/wad/fs_wad.c)
target_include_directories(fs PUBLIC ./)