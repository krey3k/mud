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

#include "doom/doomtype.h"
#include "math/math_fixed.h"
#include "render/r_defs.h"

//
// P_MAP - Movement and collision detection
//

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
extern fixed_t attackrange;
extern bool floatok;
extern bool felldown; // killough 11/98: indicates object pushed off ledge
extern fixed_t tmfloorz;
extern fixed_t tmbbox[4]; // phares 03/20/98
extern msecnode_t* sector_list;
extern line_t* ceilingline;
extern line_t* blockline;

// killough 01/11/98: Limit removed on special lines crossed
extern line_t** spechit;
extern int numspechit;

extern bool infight;
extern bool hitwall;

void P_CheckSpechits(void);
bool P_CheckPosition(mobj_t* thing, const fixed_t x, const fixed_t y);
mobj_t* P_CheckOnMobj(mobj_t* thing);
bool P_IsInLiquid(mobj_t* thing);
bool P_TryMove(mobj_t* thing, const fixed_t x, const fixed_t y, const int dropoff);
bool P_CheckLineSide(const mobj_t* actor, const fixed_t x, const fixed_t y);
bool P_TeleportMove(mobj_t* thing, const fixed_t x, const fixed_t y, const fixed_t z, const bool boss);
void P_SlideMove(mobj_t* mo);
bool P_CheckSight(mobj_t* t1, mobj_t* t2);
bool P_CheckFOV(const mobj_t* t1, const mobj_t* t2, const angle_t fov);
bool P_DoorClosed(const line_t* line);
void P_UseLines(void);

bool P_ChangeSector(sector_t* sector, const bool crunch);
void P_CreateSecNodeList(mobj_t* thing, const fixed_t x, const fixed_t y);
void P_FreeSecNodeList(void);
void P_DelSeclist(msecnode_t* node);

extern mobj_t* linetarget; // who got hit (or NULL)

fixed_t P_AimLineAttack(mobj_t* t1, angle_t angle, const fixed_t distance, const int mask);

void P_LineAttack(mobj_t* t1, angle_t angle, const fixed_t distance, const fixed_t slope, const int damage);

bool PIT_RadiusAttack(mobj_t* thing);
void P_RadiusAttack(mobj_t* spot, mobj_t* source, const int damage, const int distance, const bool verticality);

int P_GetMoveFactor(const mobj_t* mo, int* frictionp);    // killough 08/28/98
int P_GetFriction(const mobj_t* mo, int* frictionfactor); // killough 08/28/98
void P_ApplyTorque(mobj_t* mo);                           // killough 09/12/98

void P_MapEnd(void);
