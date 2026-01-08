
add_library(minigamepad minigamepad.c)
target_include_directories(minigamepad PUBLIC ./)

if(APPLE)
    target_link_libraries(minigamepad PUBLIC "-framework IOKit" "-framework CoreFoundation")
endif()