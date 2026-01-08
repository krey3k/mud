/*
==============================================================================

                                 DOOM Retro
           The classic, refined DOOM source port. For Windows PC.

==============================================================================

    Copyright © 1993-2025 by id Software LLC, a ZeniMax Media company.
    Copyright © 2013-2025 by Brad Harding <mailto:brad@doomretro.com>.

    This file is a part of DOOM Retro.

    DOOM Retro is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the license, or (at your
    option) any later version.

    DOOM Retro is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DOOM Retro. If not, see <https://www.gnu.org/licenses/>.

    DOOM is a registered trademark of id Software LLC, a ZeniMax Media
    company, in the US and/or other countries, and is used without
    permission. All other trademarks are the property of their respective
    holders. DOOM Retro is in no way affiliated with nor endorsed by
    id Software.

==============================================================================
*/

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "console/c_console.h"
#include "doom/d_main.h"
#include "doom/doomstat.h"
#include "system/i_controller.h"
#include "system/i_filesystem.h"
#include "system/i_input.h"
#include "system/i_system.h"
#include "system/i_config.h"
#include "utils/m_misc.h"
#include "sound/s_sound.h"
#include "system/i_version.h"
#include "wad/w_wad.h"

//
// I_Quit
//
NORETURN void I_Quit(bool shutdown)
{
    if(shutdown)
    {
        D_FadeScreenToBlack();

        S_Shutdown();

        M_SaveCVARs();

        I_ShutdownInput();
        I_ShutdownGraphics();
    }

    W_CloseFiles();

    FS_Shutdown();

#if defined(_WIN32)
    I_ShutdownWindows32();
#endif

    exit(0);
}

//
// I_Error
//
NORETURN void I_Error(const char* error, ...)
{
    va_list args;
    char buffer[512];
    static bool already_quitting;

    if(already_quitting)
        exit(-1);

    already_quitting = true;

    // Shutdown. Here might be other errors.
    S_Shutdown();

    M_SaveCVARs();

    I_ShutdownInput();
    I_ShutdownGraphics();

    W_CloseFiles();

    FS_Shutdown();

#if defined(_WIN32)
    I_ShutdownWindows32();
#endif

    va_start(args, error);
    vfprintf(stderr, error, args);
    fprintf(stderr, "\n\n");
    va_end(args);
    fflush(stderr);

    va_start(args, error);
    memset(buffer, 0, sizeof(buffer));
    M_vsnprintf(buffer, sizeof(buffer) - 1, error, args);
    va_end(args);

    exit(-1);
}

//
// I_Malloc
//
void* I_Malloc(size_t size)
{
    void* newp = malloc(size);

    if(!newp && size)
        I_Error("I_Malloc: Failure trying to allocate %zu bytes", size);

    return newp;
}

//
// I_Realloc
//
void* I_Realloc(void* block, size_t size)
{
    void* newp = realloc(block, size);

    if(!newp && size)
        I_Error("I_Realloc: Failure trying to reallocate %zu bytes", size);

    block = newp;
    return block;
}

void I_SetPriority(bool active)
{
#ifdef MUD_SOKOL_PORT
    if(active)
    {
#if defined(_WIN32)
        SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
#endif

        SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
    }
    else
    {
#if defined(_WIN32)
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#endif

        SDL_SetThreadPriority(SDL_THREAD_PRIORITY_NORMAL);
    }
#endif
}
