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

#include "render/r_data.h"

//
// VIDEO
//

#define DX ((V_NONWIDEWIDTH << FRACBITS) / V_VANILLAWIDTH)
#define DXI ((V_VANILLAWIDTH << FRACBITS) / V_NONWIDEWIDTH)
#define DY ((video.screen_height << FRACBITS) / V_VANILLAHEIGHT)
#define DYI ((V_VANILLAHEIGHT << FRACBITS) / video.screen_height)

#define V_NUMSCREENS 4

// Screen 0 is the screen updated by I_Update screen.
// Screen 1 is an extra buffer.
extern byte* v_screens[V_NUMSCREENS];

#define R_NUMSCREENS 4
extern byte* r_screens[R_NUMSCREENS];

extern int lowpixelwidth;
extern int lowpixelheight;

extern void (*postprocessfunc)(byte*, int, int, int, int, int, int, int);

extern byte* colortranslation[10];
extern byte cr_gold[256];
extern byte cr_none[256];

void V_InitColorTranslation(void);

// Allocates buffer screens, call before R_Init.
void V_Init(void);

// Reallocates render screen buffers for new max_screen_area
// Called automatically by R_ResizeRenderState() when needed
void V_ResizeRenderScreens(void);

bool V_ScreenShot(void);
