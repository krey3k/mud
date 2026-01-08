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

// p_local.h - Umbrella header for playsim subsystems
// For focused includes, use the individual headers directly:
//   p_user.h   - Player movement and input
//   p_mobj.h   - Map objects (things)
//   p_enemy.h  - Monster AI and behavior
//   p_maputl.h - Map utility functions
//   p_map.h    - Movement and collision detection
//   p_inter.h  - Object interaction
//   p_setup.h  - Level setup
//   p_pspr.h   - Player sprites
//   p_spec.h   - Special effects (doors, floors, etc.)

#pragma once

#include "doom/d_main.h"
#include "system/i_config.h"
#include "render/r_local.h"

// Include focused playsim headers
#include "playsim/p_user.h"
#include "playsim/p_enemy.h"
#include "playsim/p_maputl.h"
#include "playsim/p_map.h"

#define FOOTCLIPSIZE (10 * FRACUNIT)

#define FLOATSPEED (4 * FRACUNIT)

#define VIEWHEIGHT (41 * FRACUNIT)
#define DEADVIEWHEIGHT (6 * FRACUNIT)
#define MENUVIEWHEIGHT (6 * FRACUNIT)
#define JUMPHEIGHT (9 * FRACUNIT)

#define DEADLOOKDIR 128

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS 128
#define MAPBLOCKSIZE (MAPBLOCKUNITS * FRACUNIT)
#define MAPBLOCKSHIFT (FRACBITS + 7)
#define MAPBTOFRAC (MAPBLOCKSHIFT - FRACBITS)

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS (32 * FRACUNIT)

#define GRAVITY FRACUNIT
#define MAXMOVE (30 * FRACUNIT)
#define MAXMOVE_STEP (8 * FRACUNIT)
#define MINBOUNCEMAX (-20 * FRACUNIT)

#define USERANGE (64 * FRACUNIT)
#define MELEERANGE (64 * FRACUNIT)
#define MISSILERANGE (32 * 64 * FRACUNIT)
#define WAKEUPRANGE (64 * FRACUNIT)

// follow the player exclusively for 3 seconds
#define BASETHRESHOLD 100

#define BONUSADD 6

#define MOUSE_LEFTBUTTON 1
#define MOUSE_RIGHTBUTTON 2

#define MOUSE_WHEELUP MAXMOUSEBUTTONS
#define MOUSE_WHEELDOWN (MAXMOUSEBUTTONS + 1)

#define NEEDEDCARDFLASH 10

#define WEAPONBOTTOM (128 * FRACUNIT)
#define WEAPONTOP (32 * FRACUNIT)

//
// P_PSPR.C
//
void P_SetupPlayerSprites(void);
void P_MovePlayerSprites(void);
void P_FireWeapon(void);
void P_DropWeapon(void);
void P_SetPlayerSprite(const size_t position, const statenum_t stnum);

//
// P_MOBJ.C
//
#define ONFLOORZ FIXED_MIN
#define ONCEILINGZ FIXED_MAX

// Time interval for item respawning.
#define ITEMQUEUESIZE 1024

#define CARDNOTFOUNDYET -1
#define CARDNOTINMAP 0

extern int numfriends;

void P_RespawnSpecials(void);

void P_SetPlayerViewHeight(void);

void P_LookForCards(void);
void P_InitCards(void);

mobj_t* P_SpawnMobj(const fixed_t x, const fixed_t y, const fixed_t z, const mobjtype_t type);
void P_SetShadowColumnFunction(mobj_t* mobj);
mobjtype_t P_FindDoomedNum(const int type);

void P_RemoveMobj(mobj_t* mobj);
void P_RemoveBloodMobj(mobj_t* mobj);
void P_RemoveBloodSplats(void);
bool P_SetMobjState(mobj_t* mobj, statenum_t state);
void P_MobjThinker(mobj_t* mobj);

void P_SpawnMoreBlood(mobj_t* mobj);
void P_LookForFriends(void);
void P_InitHelperDogs(const int dogs);
mobj_t* P_SpawnMapThing(mapthing_t* mthing, const bool spawnmonsters);
void P_SpawnPuff(const fixed_t x, const fixed_t y, const fixed_t z, const angle_t angle);
void P_SpawnSmokeTrail(const fixed_t x, const fixed_t y, const fixed_t z, const angle_t angle);
void P_SpawnBlood(const fixed_t x,
const fixed_t y,
const fixed_t z,
angle_t angle,
const int damage,
mobj_t* target);
void P_SetBloodSplatColor(bloodsplat_t* splat);
void P_SpawnBloodSplat(const fixed_t x,
const fixed_t y,
const int color,
const bool usemaxheight,
const bool checklineside,
const fixed_t maxheight,
mobj_t* target);
bool P_CheckMissileSpawn(mobj_t* th);
mobj_t* P_SpawnMissile(mobj_t* source, const mobj_t* dest, mobjtype_t type);
mobj_t* P_SpawnPlayerMissile(mobj_t* source, mobjtype_t type);
void P_ExplodeMissile(mobj_t* mo);
bool P_SeekerMissile(mobj_t* actor, mobj_t** seektarget, angle_t thresh, angle_t turnmax, const bool seekcenter);

//
// P_SETUP.C
//
extern const byte* rejectmatrix; // for fast sight rejection
extern int* blockmaplump;
extern int* blockmap;
extern int bmapwidth;
extern int bmapheight; // in mapblocks
extern fixed_t bmaporgx;
extern fixed_t bmaporgy;    // origin of blockmap
extern mobj_t** blocklinks; // for thing chains

// MAES: extensions to support 512x512 blockmaps.
extern int blockmapxneg;
extern int blockmapyneg;

//
// P_INTER.C
//
#define MAXHEALTH 100
#define MEDIKITHEALTH 25
#define STIMPACKHEALTH 10

bool P_TouchSpecialThing(mobj_t* special, const mobj_t* toucher, const bool message, const bool stat);
bool P_TakeSpecialThing(const mobjtype_t type);

void P_DamageMobj(mobj_t* target,
mobj_t* inflicter,
mobj_t* source,
int damage,
const bool adjust,
const bool telefragged);

void P_ResurrectMobj(mobj_t* target);

extern int god_health;
extern int idfa_armor;
extern int idfa_armor_class;
extern int idkfa_armor;
extern int idkfa_armor_class;
extern int initial_health;
extern int initial_bullets;
extern int maxhealth;
extern int max_armor;
extern int green_armor_class;
extern int blue_armor_class;
extern int max_soul;
extern int soul_health;
extern int mega_health;
extern int bfgcells;
extern bool species_infighting;
extern int maxammo[];
extern int clipammo[];
extern mobjtype_t prevtouchtype;

//
// P_SPEC.C
//
#include "playsim/p_spec.h"
