
if(MUD_WEB_MULTITHREADED)
  if(MUD_WEB_SIMD)
    set(MUD_EMSC_COMMON_FLAGS "-sSHARED_MEMORY=1 -msimd128 -msse -pthread")
  else()
    set(MUD_EMSC_COMMON_FLAGS "-sSHARED_MEMORY=1 -pthread")
  endif()
  set(MUD_EMSC_COMPILER_FLAGS "-DMUD_WEB=1 -DMUD_WEB_MULTITHREADED=1")
  set(MUD_EMSC_LINKER_FLAGS "-sPTHREAD_POOL_SIZE_STRICT=0 -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency -sENVIRONMENT=web -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=512Mb -sERROR_ON_UNDEFINED_SYMBOLS=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_MAX_TEMP_BUFFER_SIZE=16777216 -sLZ4=1 -lidbfs.js")

  # Enable sourcemap support
  set(MUD_EMSC_LINKER_FLAGS "${MUD_EMSC_LINKER_FLAGS} -gsource-map --source-map-base=/")

  # force file system, see: https://github.com/emscripten-core/emscripten/blob/main/tools/file_packager.py
  set(EMPACKAGER "${EMSCRIPTEN_ROOT_PATH}/tools/file_packager.py")
  set(MUD_EMSC_LINKER_FLAGS "${MUD_EMSC_LINKER_FLAGS} -sFORCE_FILESYSTEM=1")

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MUD_EMSC_COMPILER_FLAGS "${MUD_EMSC_COMPILER_FLAGS} -O0")

    # Add -sSAFE_HEAP=1 or -sSAFE_HEAP=2 to test heap and alignment, though slows things down a lot
    set(MUD_EMSC_LINKER_FLAGS "${MUD_EMSC_LINKER_FLAGS} -sASSERTIONS=1")
  else()
    set(MUD_EMSC_COMMON_FLAGS "${MUD_EMSC_COMMON_FLAGS} -flto")
    set(MUD_EMSC_COMPILER_FLAGS "${MUD_EMSC_COMPILER_FLAGS} -O3")
    set(MUD_EMSC_LINKER_FLAGS "${MUD_EMSC_LINKER_FLAGS} -sASYNCIFY=1")
  endif()

  add_compile_options("SHELL: ${MUD_EMSC_COMMON_FLAGS} ${MUD_EMSC_COMPILER_FLAGS}")
  add_link_options("SHELL: ${MUD_EMSC_COMMON_FLAGS} ${MUD_EMSC_LINKER_FLAGS}")
else()
  if(MUD_WEB_SIMD)
    set(MUD_EMSC_COMMON_FLAGS "-msimd128 -msse")
  else()
    set(MUD_EMSC_COMMON_FLAGS "")
  endif()
  set(MUD_EMSC_COMPILER_FLAGS "-DMUD_WEB=1")
  set(MUD_EMSC_LINKER_FLAGS "-sSTACK_SIZE=262144 -sENVIRONMENT=web -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=512Mb -sERROR_ON_UNDEFINED_SYMBOLS=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_MAX_TEMP_BUFFER_SIZE=16777216 -sLZ4=1 -lidbfs.js")

  # Enable sourcemap support
  set(MUD_EMSC_LINKER_FLAGS "${MUD_EMSC_LINKER_FLAGS} -gsource-map --source-map-base=/")

  # force file system, see: https://github.com/emscripten-core/emscripten/blob/main/tools/file_packager.py
  set(EMPACKAGER "${EMSCRIPTEN_ROOT_PATH}/tools/file_packager.py")
  set(MUD_EMSC_LINKER_FLAGS "${MUD_EMSC_LINKER_FLAGS} -sFORCE_FILESYSTEM=1")

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MUD_EMSC_COMPILER_FLAGS "${MUD_EMSC_COMPILER_FLAGS} -O0")

    # Add -sSAFE_HEAP=1 or -sSAFE_HEAP=2 to test heap and alignment, though slows things down a lot
    set(MUD_EMSC_LINKER_FLAGS "${MUD_EMSC_LINKER_FLAGS} -sASSERTIONS=1")
  else()
    set(MUD_EMSC_COMMON_FLAGS "${MUD_EMSC_COMMON_FLAGS} -flto")
    set(MUD_EMSC_COMPILER_FLAGS "${MUD_EMSC_COMPILER_FLAGS} -O3")
    set(MUD_EMSC_LINKER_FLAGS "${MUD_EMSC_LINKER_FLAGS} -sASYNCIFY=1")
  endif()

  add_compile_options("SHELL: ${MUD_EMSC_COMMON_FLAGS} ${MUD_EMSC_COMPILER_FLAGS}")
  add_link_options("SHELL: ${MUD_EMSC_COMMON_FLAGS} ${MUD_EMSC_LINKER_FLAGS}")
endif()