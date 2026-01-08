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
#include "game/g_event.h"
#include "system/i_video.h"

//
// GAME
//
void G_InitNew(skill_t skill, int ep, int map);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void G_DeferredInitNew(skill_t skill, int ep, int map);

void G_DeferredLoadLevel(skill_t skill, int ep, int map);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel.
void G_LoadGame(const char* name);

void G_DoLoadGame(void);
void G_DoLoadLevel(void);

// Called by M_Responder.
void G_SaveGame(const int slot, const char* description, const char* name);

void G_ExitLevel(void);
void G_SecretExitLevel(void);

int G_GetParTime(void);

void G_WorldDone(void);

void G_Ticker(void);

void G_PlayerReborn(void);

void G_ScreenShot(void);

void G_SetFastParms(bool fast_pending);
void G_SetMovementSpeed(int scale);

void G_LoadedGameMessage(void);

extern bool controllerpress;
extern char lbmname1[MAX_PATH];
extern char lbmpath1[MAX_PATH];
extern char lbmpath2[MAX_PATH];
extern int st_facecount;
extern char savename[MAX_PATH];
extern int savegameslot;
extern bool secretexit;
extern gameaction_t loadaction;
extern bool newpars;
extern int pars[10][10];
extern int cpars[100];
extern bool resetinventory;
