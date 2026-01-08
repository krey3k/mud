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

#include "doom/d_event.h"
#include "doom/d_ticcmd.h"
#include "system/i_video.h"

#define FORWARDMOVE0 0x19
#define FORWARDMOVE1 0x32

#define SIDEMOVE0 0x18
#define SIDEMOVE1 0x28

#define CONTROLLERANGLETURN 5120

#define MAXPLMOVE forwardmove[1]

#define NUMWEAPONKEYS 7

#define SLOWTURNTICS 6

// Read current data from inputs and build a player movement command.
void G_BuildTiccmd(ticcmd_t* cmd);

bool G_Responder(const event_t* ev);

void G_ToggleAlwaysRun(evtype_t type);

void G_NextWeapon(void);
void G_PrevWeapon(void);
void G_RemoveChoppers(void);
void G_ClearInput(void);

extern fixed_t forwardmove[2];
extern fixed_t sidemove[2];
extern fixed_t angleturn[3];
extern bool* mousebuttons;
extern char keyactionlist[NUMKEYS][255];
extern char mouseactionlist[MAXMOUSEBUTTONS + 2][255];
extern bool usefreelook;
extern bool sendpause;
extern bool sendsave;

