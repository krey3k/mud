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
// P_MAPUTL - Map utility functions
//

typedef struct
{
    fixed_t x, y;
    fixed_t dx, dy;
} divline_t;

typedef struct
{
    fixed_t frac; // along trace line
    mobj_t* thing;
    line_t* line;
} intercept_t;

typedef bool (*traverser_t)(intercept_t* in);

fixed_t P_ApproxDistance(fixed_t dx, fixed_t dy);
int P_PointOnLineSide(const fixed_t x, const fixed_t y, const line_t* line);
int P_BoxOnLineSide(const fixed_t* tmbox, const line_t* ld);
fixed_t P_InterceptVector(const divline_t* v2, const divline_t* v1);

// MAES: support 512x512 blockmaps.
int P_GetSafeBlockX(int coord);
int P_GetSafeBlockY(int coord);

mobj_t* P_RoughTargetSearch(mobj_t* mo, const angle_t fov, const int distance);

extern fixed_t opentop;
extern fixed_t openbottom;
extern fixed_t openrange;
extern fixed_t lowfloor;

void P_LineOpening(const line_t* line);

bool P_BlockLinesIterator(const int x, const int y, bool func(line_t*));
bool P_BlockThingsIterator(const int x, const int y, bool func(mobj_t*), bool blockmapfix);

#define PT_ADDLINES 1
#define PT_ADDTHINGS 2

extern divline_t dltrace;

bool P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, const int flags, traverser_t trav);

void P_UnsetThingPosition(mobj_t* thing);
void P_UnsetBloodSplatPosition(bloodsplat_t* splat);
void P_SetThingPosition(mobj_t* thing);
void P_SetBloodSplatPosition(bloodsplat_t* splat);

void P_CheckIntercepts(void);
