
add_library(atomix atomix.c)
target_link_libraries(atomix PRIVATE thread)
target_include_directories(atomix PUBLIC ./)