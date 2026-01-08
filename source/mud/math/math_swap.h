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

#pragma once

// Endianness handling.
// WAD files are stored little endian.

// Just use SDL's endianness swapping functions.

#ifdef MUD_SOKOL_PORT
// the #ifdef is for marker consistency, though we want the defines for now
#else
#define SDL_SwapLE16(X) (X)
#define SDL_SwapLE32(X) (X)

inline unsigned short SDL_Swap16(unsigned short x)
{
    return ((x << 8) | (x >> 8));
}

inline unsigned int SDL_Swap32(unsigned int x)
{
    return ((x << 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x >> 24));
}

#define SDL_SwapBE16(X) SDL_Swap16(X)
#define SDL_SwapBE32(X) SDL_Swap32(X)

#endif

// These are deliberately cast to signed values; this is the behavior
// of the macros in the original source and some code relies on it.
#define SHORT(x) ((signed short)SDL_SwapLE16(x))
#define LONG(x) ((signed int)SDL_SwapLE32(x))
