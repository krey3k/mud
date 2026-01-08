#define SOKOL_IMPL

#ifdef _WIN32
    #define SOKOL_D3D11
#elif defined(__APPLE__)
    #define SOKOL_METAL
#elif defined(MUD_GLES3)
    #define SOKOL_GLES3
#else
    #define SOKOL_GLCORE
#endif

#include "sokol_log.h"
#include "sokol_memtrack.h"
#include "sokol_args.h"
#include "sokol_time.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#include "sokol_audio.h"
#include "sokol_fetch.h"

#define NK_IMPLEMENTATION

#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wnull-pointer-subtraction"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#if defined(_MSC_VER)
#pragma warning(disable:4996)   // sprintf,fopen,localtime: This function or variable may be unsafe
#pragma warning(disable:4116)   // unnamed type definition in parentheses
#endif
#include "nuklear.h"

#include "sokol_nuklear.h"