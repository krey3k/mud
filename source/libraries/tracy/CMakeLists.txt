##########################################
# mud_tracy
##########################################

add_library(mud_tracy INTERFACE)

if (MUD_PROFILING)
  include(FetchContent)
  FetchContent_Declare (
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG e6b9ea460993a6f093283055ef3c98025b19f909
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
  )
  option (TRACY_ON_DEMAND "" OFF)
  option (TRACY_CALLSTACK "" ON)
  FetchContent_MakeAvailable (tracy)  
endif()

target_include_directories(mud_tracy INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR})