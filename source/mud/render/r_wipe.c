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

#include "math/math_random.h"
#include "render/v_video.h"

//
// SCREEN WIPE PACKAGE
//

static int y[V_MAXWIDTH];
static short src[V_MAXSCREENAREA];

static void Wipe_ShittyColMajorXform(short* dest)
{
    for(int yy = 0; yy < video.screen_height; yy++)
        for(int xx = 0; xx < video.screen_width / 2; xx++)
            src[yy + xx * video.screen_height] = dest[yy * video.screen_width / 2 + xx];

    memcpy(dest, src, video.screen_area);
}

static void Wipe_InitMelt(void)
{
    // makes this wipe faster (in theory) to have stuff in column-major format
    Wipe_ShittyColMajorXform((short*)v_screens[2]);
    Wipe_ShittyColMajorXform((short*)v_screens[3]);

    // setup initial column positions (y < 0 => not ready to scroll yet)
    y[0] = y[1] = -(M_BigRandom() & 15);

    for(int i = 2; i < video.screen_width - 1; i += 2)
        y[i] = y[i + 1] = BETWEEN(-15, y[i - 1] + M_BigRandom() % 3 - 1, 0);
}

static void Wipe_Melt(const int i, const int dy)
{
    short* s = &((short*)v_screens[3])[i * video.screen_height + y[i]];
    short* d = &((short*)v_screens[0])[y[i] * video.screen_width / 2 + i];

    for(int j = 0, k = dy; k > 0; k--, j += video.screen_width / 2)
        d[j] = *s++;

    y[i] += dy;
    s = &((short*)v_screens[2])[i * video.screen_height];
    d = &((short*)v_screens[0])[y[i] * video.screen_width / 2 + i];

    for(int j = 0, k = video.screen_height - y[i]; k > 0; k--, j += video.screen_width / 2)
        d[j] = *s++;
}

static bool Wipe_DoMelt(void)
{
    bool done = true;

    for(int i = 0; i < video.screen_width / 2; i++)
        if(y[i] < 0)
        {
            y[i]++;
            done = false;
        }
        else if(y[i] < 16)
        {
            Wipe_Melt(i, y[i] + 1);
            done = false;
        }
        else if(y[i] < video.screen_height)
        {
            Wipe_Melt(i, MIN(video.screen_height / 16, video.screen_height - y[i]));
            done = false;
        }

    return done;
}

void Wipe_StartScreen(void)
{
    memcpy(v_screens[2], v_screens[0], video.screen_area);
}

void Wipe_EndScreen(void)
{
    memcpy(v_screens[3], v_screens[0], video.screen_area);
    memcpy(v_screens[0], v_screens[2], video.screen_area);
}

bool Wipe_ScreenWipe(void)
{
    // when false, stop the wipe
    static bool go;

    // initial stuff
    if(!go)
    {
        go = true;
        Wipe_InitMelt();
    }

    // do a piece of wipe-in
    if(Wipe_DoMelt())
        go = false;

    return !go;
}
