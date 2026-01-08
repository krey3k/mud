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

#include "doom/doomdef.h"

//
// TYPES
//

//
// WADFILE I/O related stuff.
//

#define IWAD 1
#define PWAD 2

typedef struct
{
    struct fs* wad_stream;
    bool freedoom;
    char path[MAX_PATH];
    int type;
} wadfile_t;
typedef struct
{
    char name[9];
    int size;
    void* cache;

    // killough 01/31/98: hash table fields, used for ultra-fast hash table lookup
    int index;
    int next;

    int position;

    wadfile_t* wadfile;
} lumpinfo_t;

extern lumpinfo_t** lumpinfo;
extern int numlumps;
extern char* wadsloaded;

bool IsUltimateDOOM(const char* iwadname);

bool W_AddFile(const char* filename, bool autoloaded);
bool W_AutoloadFiles(const char* folder);

int W_CheckNumForName(const char* name);

int W_CheckNumForNameFromTo(int min, int max, const char* name);
int W_GetNumForName(const char* name);
int W_GetLastNumForName(const char* name);
int W_GetXNumForName(const char* name, const int x);
int W_GetNumForNameFromResourceWAD(const char* name);
void W_HashNumForNameFromTo(int from, int to, int size);

int W_GetNumLumps(const char* name);

int W_LumpLength(int lump);

void* W_CacheLumpNum(int lumpnum);

#define W_CacheLumpName(name) W_CacheLumpNum(W_GetNumForName(name))
#define W_CacheLastLumpName(name) W_CacheLumpNum(W_GetLastNumForName(name))
#define W_CacheXLumpName(name, x) W_CacheLumpNum(W_GetXNumForName(name, x))
#define W_CacheLumpNameFromResourceWAD(name) \
    W_CacheLumpNum(W_GetNumForNameFromResourceWAD(name))

void W_Init(void);
void W_CheckForPNGLumps(void);

unsigned int W_LumpNameHash(const char* s);

void W_ReleaseLumpNum(int lumpnum);

#define W_ReleaseLumpName(name) W_ReleaseLumpNum(W_GetNumForName(name))

// Open the specified file. Returns a pointer to a new wadfile_t
// handle for the WAD file, or NULL if it could not be opened.
wadfile_t* W_OpenFile(const char* path);

// Close the specified WAD file.
void W_CloseFile(wadfile_t* wad);

void W_CloseFiles(void);

gamemission_t IWADRequiredByPWAD(char* pwadname);
bool HasDehackedLump(const char* pwadname);
