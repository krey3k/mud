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

#include "doom/doomstat.h"

// Game Mode - identify IWAD as shareware, retail, etc.
game_t game = {
    .mode = indetermined,
    .mission = doom,
    .description = "",
    .skill = sk_none,
    .prevskill = sk_none,
    .episode = 0,
    .map = 0,
    .state = GS_NONE,
    .time = 0,
    .action = ga_nothing,
    .keydown = { 0 }
};

// Set if homebrew PWAD stuff has been added.
bool modifiedgame = false;

bool DBIGFONT;
bool DSFLAMST;
bool FREEDOOM;
bool FREEDOOM1;
bool FREEDM;
bool M_DOOM;
bool M_EPISOD;
bool M_GDHIGH;
bool M_GDLOW;
bool M_LGTTL;
bool M_LOADG;
bool M_LSCNTR;
bool M_MSENS;
bool M_MSGOFF;
bool M_MSGON;
bool M_NEWG;
bool M_NGAME;
bool M_NMARE;
bool M_OPTTTL;
bool M_PAUSE;
bool M_SAVEG;
bool M_SGTTL;
bool M_SKILL;
bool M_SKULL1;
bool M_SVOL;
bool PUFFA0 = false;
short RROCK05;
short RROCK08;
short SLIME09;
short SLIME12;
bool STCFNxxx = false;
bool STYSNUM0;
bool WICOLON;
bool WISCRT2;

int PLAYPALs;
int STBARs;
