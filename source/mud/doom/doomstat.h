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

#include <time.h>

// We need globally shared data structures,
//  for defining the global state variables.
#include "doom/d_loop.h"

// We need the player data structure as well.
#include "doom/d_player.h"

// Forward declarations for game_t structure
#include "doom/d_event.h"

#define NUMKEYS 512

// ------------------------
// Command line parameters.
//
extern bool infiniteammo;
extern bool nomonsters; // checkparm of -nomonsters
extern bool regenhealth;
extern bool respawnitems;
extern bool respawnmonsters; // checkparm of -respawn
extern bool solonet;         // [BH] checkparm of -solonet
extern bool pistolstart;     // [BH] checkparm of -pistolstart
extern bool fastparm;        // checkparm of -fast

extern bool devparm; // DEBUG: launched with -devparm

// -----------------------------------------------------
// Game statistics structure
//
typedef struct
{
    int kills;
    int items;
    int secrets;
    int pickups;
    int monstercount[NUMMOBJTYPES];
    int barrels;
    int player1starts;
    int maptime;   // tics in game play for par
    int totaltime; // cumulative time across all maps
} gamestat_t;

// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//
typedef struct
{
    gamemode_t mode;
    gamemission_t mission;
    char description[255];
    skill_t skill;
    skill_t prevskill;
    int episode;
    int map;
    gamestate_t state;
    int time;
    gameaction_t action;
    bool keydown[NUMKEYS];
    gamestat_t stats;
} game_t;

extern game_t game;

// Set if homebrew PWAD stuff has been added.
extern bool modifiedgame;

// -------------------------------------------
// Selected skill type, map etc.
//

// Defaults for menu, methinks.
extern skill_t startskill;
extern int startepisode;

extern bool autostart;

extern char speciallumpname[6];

extern bool freeze;

extern bool DBIGFONT;
extern bool DSFLAMST;
extern bool FREEDOOM;
extern bool FREEDOOM1;
extern bool FREEDM;
extern bool M_DOOM;
extern bool M_EPISOD;
extern bool M_GDHIGH;
extern bool M_GDLOW;
extern bool M_LGTTL;
extern bool M_LOADG;
extern bool M_LSCNTR;
extern bool M_MSENS;
extern bool M_MSGOFF;
extern bool M_MSGON;
extern bool M_NEWG;
extern bool M_NGAME;
extern bool M_NMARE;
extern bool M_OPTTTL;
extern bool M_PAUSE;
extern bool M_SAVEG;
extern bool M_SGTTL;
extern bool M_SKILL;
extern bool M_SKULL1;
extern bool M_SVOL;
extern bool PUFFA0;
extern short RROCK05;
extern short RROCK08;
extern short SLIME09;
extern short SLIME12;
extern bool STCFNxxx;
extern bool STYSNUM0;
extern bool WICOLON;
extern bool WISCRT2;

extern int PLAYPALs;
extern int STBARs;

// -------------------------
// Status flags for refresh.
//

extern bool automapactive; // In automap mode?
extern bool menuactive;    // Menu overlaid?
extern bool paused;        // Game Pause?

extern bool viewactive;


// -----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.

extern bool realframe;

// Intermission stats.
// Parameters for world map/intermission.
extern wbstartstruct_t wminfo;

// -----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern char* savegamefolder;
extern char* autoloadfolder;
extern char* autoloadiwadsubfolder;
extern char* autoloadpwadsubfolder;

// wipegamestate can be set to -1
//  to force a wipe on the next draw
extern gamestate_t wipegamestate;

// Needed to store the number of the dummy sky flat.
// Used for rendering,
//  as well as tracking projectiles etc.
extern int skyflatnum;

extern ticcmd_t localcmds[BACKUPTICS];
