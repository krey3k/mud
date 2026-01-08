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

#include "doom/d_main.h"
#include "doom/doomstat.h"
#include "game/g_game.h"
#include "system/i_input.h"
#include "system/i_timer.h"
#include "system/i_config.h"
#include "menu/m_menu.h"
#include "sound/s_sound.h"

// [AM] Fractional part of the current tic, in the half-open
//      range of [0.0, 1.0). Used for interpolation.
fixed_t fractionaltic;

ticcmd_t localcmds[BACKUPTICS];

void TryRunTics(void)
{
    static int maketic;
    static uint64_t lastmadetic;
    uint64_t newtics = I_GetTime() - lastmadetic;
    int runtics;

    lastmadetic += newtics;
    fractionaltic = ((I_GetTimeMS() * TICRATE) % 1000) * FRACUNIT / 1000;

    while(newtics--)
    {
        if(maketic - game.time > 2)
            break;

        G_BuildTiccmd(&localcmds[maketic++ % BACKUPTICS]);
    }

    if(!(runtics = maketic - game.time))
        return;

    while(runtics--)
    {
        if(advancetitle)
            D_DoAdvanceTitle();

        if(menuactive)
            M_Ticker();

        G_Ticker();
        game.time++;

        if(localcmds[0].buttons & BT_SPECIAL)
            localcmds[0].buttons = 0;
    }

    S_UpdateSounds(); // move positional sounds
}
