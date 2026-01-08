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

#include "console/c_cmds.h"
#include "console/c_console.h"
#include "doom/d_deh.h"
#include "doom/d_main.h"
#include "doom/doomstat.h"
#include "math/math_swap.h"
#include "system/i_system.h"
#include "utils/m_argv.h"
#include "utils/m_misc.h"
#include "system/i_filesystem.h"
#include "system/i_version.h"
#include "wad/w_merge.h"
#include "wad/w_wad.h"
#include "utils/z_zone.h"

#define MAXWADS 16

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push, 1)
#endif

typedef struct
{
    char id[4]; // Should be "IWAD" or "PWAD"
    int numlumps;
    int infotableofs;
} PACKEDATTR wadinfo_t;

typedef struct
{
    int filepos;
    int size;
    char name[8];
} PACKEDATTR filelump_t;

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

// Location of each lump on disk.
lumpinfo_t** lumpinfo;

int numlumps;

char* wadsloaded;

static int numwads;
static wadfile_t* wadlist[MAXWADS];

static bool IsFreedoom(const char* iwadname)
{
    fs_file* fp = FS_OpenFile(iwadname, FS_READ, FS_TRUE);
    wadinfo_t header;
    int result = false;

    if(!fp)
        return false;

    // read IWAD header
    if(FS_Read(&header, 1, sizeof(header), fp) == sizeof(header))
    {
        filelump_t lump = { 0 };
        const char* n   = lump.name;

        FS_Seek(fp, LONG(header.infotableofs), FS_SEEK_SET);

        for(int i = LONG(header.numlumps); i && FS_Read(&lump, sizeof(lump), 1, fp); i--)
            if(!strncmp(n, "FREEDOOM", 8))
            {
                result = true;
                break;
            }
    }

    FS_CloseFile(fp);
    return result;
}

static bool IsBFGEdition(const char* iwadname)
{
    fs_file* fp = FS_OpenFile(iwadname, FS_READ, FS_TRUE);
    wadinfo_t header;
    int result1 = false;
    int result2 = false;

    if(!fp)
        return false;

    // read IWAD header
    if(FS_Read(&header, 1, sizeof(header), fp) == sizeof(header))
    {
        filelump_t lump = { 0 };
        const char* n   = lump.name;

        FS_Seek(fp, LONG(header.infotableofs), FS_SEEK_SET);

        for(int i = LONG(header.numlumps); i && FS_Read(&lump, sizeof(lump), 1, fp); i--)
            if(!strncmp(n, "DMENUPIC", 8))
            {
                result1 = true;

                if(result2)
                    break;
            }
            else if(!strncmp(n, "M_ACPT", 6))
            {
                result2 = true;

                if(result1)
                    break;
            }
    }

    FS_CloseFile(fp);
    return (result1 && result2);
}

bool IsUltimateDOOM(const char* iwadname)
{
    fs_file* fp = FS_OpenFile(iwadname, FS_READ, FS_TRUE);
    wadinfo_t header;
    int result = false;

    if(!fp)
        return false;

    // read IWAD header
    if(FS_Read(&header, 1, sizeof(header), fp) == sizeof(header))
    {
        filelump_t lump = { 0 };
        const char* n   = lump.name;

        FS_Seek(fp, LONG(header.infotableofs), FS_SEEK_SET);

        for(int i = LONG(header.numlumps); i && FS_Read(&lump, sizeof(lump), 1, fp); i--)
            if(!strncmp(n, "E4M1", 4))
            {
                result = true;
                break;
            }
    }

    FS_CloseFile(fp);
    return result;
}

//
// LUMP BASED ROUTINES.
//

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
//
bool W_AddFile(const char* filename, bool autoloaded)
{
    static bool resourcewadadded;
    wadinfo_t header;
    size_t length;
    int startlump;
    filelump_t* fileinfo;
    filelump_t* filerover;
    lumpinfo_t* filelumps;
    char* temp;
    const char* file = leafname(filename);

    // open the file and add to directory
    wadfile_t* wadfile = W_OpenFile(filename);

    if(!wadfile)
        return false;

    if(numwads < MAXWADS)
        wadlist[numwads++] = wadfile;

    M_StringCopy(wadfile->path, filename, sizeof(wadfile->path));

    wadfile->freedoom = IsFreedoom(filename);
    if(wadfile->freedoom)
        FREEDOOM = true;

    // WAD file
    FS_WADSeek(wadfile->wad_stream, 0, SEEK_SET);
    FS_WADRead(&header, sizeof(header), 1, wadfile->wad_stream);

    // Homebrew levels?
    if(strncmp(header.id, "IWAD", 4) && strncmp(header.id, "PWAD", 4))
        I_Error("%s doesn't have an IWAD or PWAD id.", filename);

    if(!strncmp(header.id, "IWAD", 4) || D_IsDOOMIWAD(file))
        wadfile->type = IWAD;
    else
        wadfile->type = PWAD;

    header.numlumps     = LONG(header.numlumps);
    header.infotableofs = LONG(header.infotableofs);
    length              = header.numlumps * sizeof(filelump_t);
    fileinfo            = malloc(length);
    FS_WADSeek(wadfile->wad_stream, header.infotableofs, SEEK_SET);
    FS_WADRead(fileinfo, length, 1, wadfile->wad_stream);

    // Increase size of numlumps array to accommodate the new file.
    filelumps = calloc(header.numlumps, sizeof(lumpinfo_t));

    startlump = numlumps;
    numlumps += header.numlumps;
    lumpinfo  = I_Realloc(lumpinfo, numlumps * sizeof(lumpinfo_t*));
    filerover = fileinfo;

    for(int i = startlump; i < numlumps; i++)
    {
        lumpinfo_t* lump_p = &filelumps[i - startlump];

        lump_p->wadfile  = wadfile;
        lump_p->position = LONG(filerover->filepos);
        lump_p->size     = LONG(filerover->size);
        lump_p->cache    = NULL;
        M_CopyLumpName(lump_p->name, filerover->name);
        lumpinfo[i] = lump_p;
        filerover++;
    }

    free(fileinfo);

    if(!D_IsResourceWAD(file))
    {
        if(wadsloaded)
            wadsloaded = M_StringJoin(wadsloaded, ", ", file, NULL);
        else
            wadsloaded = M_StringDuplicate(file);
    }

    if(!D_IsResourceWAD(file) || devparm)
    {
        const int count = numlumps - startlump;
        static int wadcount;

        temp = commify((int64_t)count);

        if(!count)
            C_Warning(0, "%s%s %s been %s from the %s " BOLD("%s") ".",
            (wadcount++ ? "An additional " : ""), temp,
            (numlumps - startlump == 1 ? "lump has" : "lumps have"),
            (autoloaded ? "automatically added" : "added"),
            (wadfile->type == IWAD ? "IWAD" : "PWAD"), wadfile->path);
        else
            C_Output("%s%s %s been %s from the %s " BOLD("%s") ".",
            (wadcount++ ? "An additional " : ""), temp,
            (numlumps - startlump == 1 ? "lump has" : "lumps have"),
            (autoloaded ? "automatically added" : "added"),
            (wadfile->type == IWAD ? "IWAD" : "PWAD"), wadfile->path);

        free(temp);

        if(D_IsDOOM1IWAD(file))
        {
            if(M_StringCompare(file, "DOOM1.WAD"))
                C_Warning(0,
                "This is the shareware version of " ITALICS(
                "DOOM") ". "
                        "You can buy the full version on " ITALICS(
                        "Steam") ", etc.");
        }
    }

    if(!resourcewadadded)
    {
        resourcewadadded = true;

        if(!W_MergeFile(resourcewad, true))
            I_Error("%s is invalid.", resourcewad);
    }

    return true;
}

bool W_AutoloadFiles(const char* folder)
{
    bool result = false;
    if (!folder) return result;
    fs_iterator *iter = FS_GetDirIterator(folder, FS_READ, FS_TRUE);
    if (!iter) return result;
    while (iter != NULL) 
    {  
        if (iter->info.directory || !iter->pName)
        {
            iter = fs_next(iter);
            continue;
        }
        if(M_StringEndsWith(iter->pName, ".wad") || M_StringEndsWith(iter->pName, ".pwad"))
            result = W_MergeFile(iter->pName, true);
        else if(M_StringEndsWith(iter->pName, ".deh") ||
            M_StringEndsWith(iter->pName, ".bex"))
        {
            D_ProcessDehFile(iter->pName, 0, true);
            result = true;
        }
        else if(M_StringEndsWith(iter->pName, ".cfg"))
        {
            char strparm[512] = "";
            fs_file* file;
            int linecount = 0;

            if(!(file = FS_OpenFile(iter->pName, FS_READ, FS_TRUE)))
            {
                C_Warning(0, BOLD("%s") " couldn't be opened.", iter->pName);
                iter = fs_next(iter);
                continue;
            }

            result         = true;
            parsingcfgfile = true;

            while(FS_GetString(strparm, sizeof(strparm), file))
            {
                if(strparm[0] == ';')
                    continue;

                if(C_ValidateInput(strparm))
                    linecount++;
            }

            parsingcfgfile = false;
            FS_CloseFile(file);

            if(linecount == 1)
                C_Output("One line has been parsed in " BOLD("%s") ".", iter->pName);
            else
            {
                char* temp2 = commify(linecount);

                C_Output("%s lines have been parsed in " BOLD("%s") ".", temp2, iter->pName);
                free(temp2);
            }
        }
        iter = fs_next(iter);  
    }

    return result;
}

// Hash function used for lump names.
// Must be modded with table size.
// Can be used for any 8-character names.
// by Lee Killough
unsigned int W_LumpNameHash(const char* s)
{
    unsigned int hash;

    (void)((hash = toupper(s[0]), s[1]) && (hash = hash * 3 + toupper(s[1]), s[2]) &&
    (hash = hash * 2 + toupper(s[2]), s[3]) && (hash = hash * 2 + toupper(s[3]), s[4]) &&
    (hash = hash * 2 + toupper(s[4]), s[5]) && (hash = hash * 2 + toupper(s[5]), s[6]) &&
    (hash = hash * 2 + toupper(s[6]), hash = hash * 2 + toupper(s[7])));

    return hash;
}

bool HasDehackedLump(const char* pwadname)
{
    fs_file* fp        = FS_OpenFile(pwadname, FS_READ, FS_TRUE);
    filelump_t lump = { 0 };
    wadinfo_t header;
    int result = false;

    if(!fp)
        return false;

    // read IWAD header
    if(FS_Read(&header, 1, sizeof(header), fp) == sizeof(header))
    {
        const char* n = lump.name;

        FS_Seek(fp, LONG(header.infotableofs), FS_SEEK_SET);

        for(int i = LONG(header.numlumps); i && FS_Read(&lump, sizeof(lump), 1, fp); i--)
            if(!strncmp(n, "DEHACKED", 8))
            {
                result = true;
                break;
            }
    }

    FS_CloseFile(fp);
    return result;
}

gamemission_t IWADRequiredByPWAD(char* pwadname)
{
    fs_file* fp             = FS_OpenFile(pwadname, FS_READ, FS_TRUE);
    gamemission_t result = none;

    if(!fp)
        I_Error("Can't open PWAD: %s", pwadname);
    else
    {
        wadinfo_t header;

        if(FS_Read(&header, 1, sizeof(header), fp) != sizeof(header) ||
        (strncmp(header.id, "IWAD", 4) && strncmp(header.id, "PWAD", 4)))
        {
            FS_CloseFile(fp);
            I_Error("%s doesn't have an IWAD or PWAD id.", pwadname);
        }
        else
        {
            filelump_t lump = { 0 };
            const char* n   = lump.name;

            FS_Seek(fp, LONG(header.infotableofs), FS_SEEK_SET);

            for(int i = LONG(header.numlumps); i && FS_Read(&lump, sizeof(lump), 1, fp); i--)
                if(n[0] == 'E' && isdigit((int)n[1]) && n[2] == 'M' &&
                isdigit((int)n[3]) && n[4] == '\0')
                {
                    result = doom;
                    break;
                }
                else if(n[0] == 'M' && n[1] == 'A' && n[2] == 'P' &&
                isdigit((int)n[3]) && isdigit((int)n[4]) && n[5] == '\0')
                {
                    result = doom2;
                    break;
                }

            FS_CloseFile(fp);

            if(result == doom2)
            {
                const char* leaf = leafname(pwadname);

                if(M_StringCompare(leaf, "pl2.wad") ||
                M_StringCompare(leaf, "plut3.wad") || M_StringCompare(leaf, "prcp2.wad"))
                    result = pack_plut;
                else if(M_StringCompare(leaf, "tnto.wad") ||
                M_StringCompare(leaf, "tntr.wad") || M_StringCompare(leaf, "tnt-ren.wad") ||
                M_StringCompare(leaf, "resist.wad"))
                    result = pack_tnt;
            }
        }
    }

    return result;
}

//
// W_CheckNumForName
// Returns -1 if name not found.
//
// Rewritten by Lee Killough to use hash table for performance. Significantly
// cuts down on time -- increases DOOM performance over 300%. This is the
// single most important optimization of the original DOOM sources, because
// lump name lookup is used so often, and the original DOOM used a sequential
// search. For large wads with > 1000 lumps this meant an average of over
// 500 were probed during every search. Now the average is under 2 probes per
// search. There is no significant benefit to packing the names into longwords
// with this new hashing algorithm, because the work to do the packing is
// just as much work as simply doing the string comparisons with the new
// algorithm, which minimizes the expected number of comparisons to under 2.
//
int W_CheckNumForName(const char* name)
{
    // Hash function maps the name to one of possibly numlump chains.
    // It has been tuned so that the average chain length never exceeds 2.
    int i = lumpinfo[W_LumpNameHash(name) % numlumps]->index;

    while(i >= 0 && strncasecmp(lumpinfo[i]->name, name, 8))
        i = lumpinfo[i]->next;

    // Return the matching lump, or -1 if none found.
    return i;
}

//
// W_GetNumLumps
// Check if there's more than one of the same lump.
//
int W_GetNumLumps(const char* name)
{
    int count = 0;

    for(int i = numlumps - 1; i >= 0; i--)
        if(!strncasecmp(lumpinfo[i]->name, name, 8))
            count++;

    return count;
}

// W_HashNumForNameFromTo and W_CheckNumForNameFromTo
//  [PN/JN] Optimized flat lookup with hash.
//  Builds a one-time open-addressing hash table for the [from, to] range,
//  using lump name hash modulo the range size. Collision resolution is linear.
//  This replaces linear search for flat lumps with near-O(1) lookup.
static int* flat_hash = NULL;
static int hash_size  = 0;

void W_HashNumForNameFromTo(int from, int to, int size)
{
    // Compute number of entries in the range
    hash_size = size;

    // Allocate hash table (statically sized to the range)
    flat_hash = Z_Malloc(sizeof(int) * hash_size, PU_STATIC, NULL);

    // Initialize all entries to -1 (empty)
    for(int i = 0; i < hash_size; i++)
        flat_hash[i] = -1;

    // Fill the table with all lump names in the range
    // Use open addressing for collision resolution
    for(int i = from; i <= to; i++)
    {
        int h = W_LumpNameHash(lumpinfo[i]->name) % hash_size;

        while(flat_hash[h] != -1)
            h = (h + 1) % hash_size;

        flat_hash[h] = i;
    }
}

int W_CheckNumForNameFromTo(int min, int max, const char* name)
{
    // Lookup using same hash function
    int hash = W_LumpNameHash(name) % hash_size;

    // Probe the table using open addressing
    for(int probes = 0; probes < hash_size; probes++)
    {
        int i = flat_hash[hash];

        if(i == -1)
            break;

        if(!strncasecmp(lumpinfo[i]->name, name, 8))
            return i;

        hash = (hash + 1) % hash_size;
    }

    // Fallback: brute-force scan (should never happen)
    for(int i = min; i <= max; i++)
        if(!strncasecmp(lumpinfo[i]->name, name, 8))
            return i;

    return -1;
}

void W_Init(void)
{
    for(int i = 0; i < numlumps; i++)
        lumpinfo[i]->index = -1; // mark slots empty

    // Insert nodes to the beginning of each chain, in first-to-last
    // lump order, so that the last lump of a given name appears first
    // in any chain, observing PWAD ordering rules. -- killough
    for(int i = 0; i < numlumps; i++)
    {
        // hash function:
        const int j = W_LumpNameHash(lumpinfo[i]->name) % numlumps;

        lumpinfo[i]->next  = lumpinfo[j]->index; // prepend to list
        lumpinfo[j]->index = i;
    }
}

static bool W_IsPNGLump(const int lump)
{
    bool result = false;

    if(W_LumpLength(lump) >= 13)
    {
        const unsigned char* patch = (const unsigned char*)W_CacheLumpNum(lump);

        if(patch[0] == 0x89 && patch[1] == 'P' && patch[2] == 'N' && patch[3] == 'G')
            result = true;

        W_ReleaseLumpNum(lump);
    }

    return result;
}

void W_CheckForPNGLumps(void)
{
    const int lump = W_CheckNumForName("TITLEPIC");

    if(lump >= 0 && W_IsPNGLump(lump))
        I_Error("The TITLEPIC lump is an unsupported PNG image!");

    for(int i = 0; i < numlumps; i++)
        if(W_IsPNGLump(i))
            C_Warning(0, "The " BOLD("%.8s") " lump is an unsupported PNG image.",
            lumpinfo[i]->name);
}

//
// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
int W_GetNumForName(const char* name)
{
    const int i = W_CheckNumForName(name);

    if(i < 0)
        I_Error("W_GetNumForName: %s not found!", name);

    return i;
}

// Go forwards rather than backwards so we get lump from IWAD and not PWAD
int W_GetLastNumForName(const char* name)
{
    int i;

    for(i = 0; i < numlumps; i++)
        if(!strncasecmp(lumpinfo[i]->name, name, 8))
            break;

    if(i == numlumps)
        I_Error("W_GetLastNumForName: %s not found!", name);

    return i;
}

int W_GetXNumForName(const char* name, const int x)
{
    int count = 0;
    int i;

    for(i = 0; i < numlumps; i++)
        if(!strncasecmp(lumpinfo[i]->name, name, 8) && ++count == x)
            break;

    if(i == numlumps)
        I_Error("W_GetXNumForName: %s not found!", name);

    return i;
}

int W_GetNumForNameFromResourceWAD(const char* name)
{
    int i;

    for(i = 0; i < numlumps; i++)
        if(!strncasecmp(lumpinfo[i]->name, name, 8) &&
        D_IsResourceWAD(lumpinfo[i]->wadfile->path))
            break;

    if(i == numlumps)
        I_Error("W_GetLastNumForName: %s not found!", name);

    return i;
}

//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength(int lump)
{
    if(lump >= numlumps)
        I_Error("W_LumpLength: %i >= numlumps", lump);

    return lumpinfo[lump]->size;
}

void* W_CacheLumpNum(int lumpnum)
{
    lumpinfo_t* lump = lumpinfo[lumpnum];

    if(!lump->cache)
        lump->cache = FS_GetRawLump(lump);

    return lump->cache;
}

void W_ReleaseLumpNum(int lumpnum)
{
    // WADs are now cached in memory as a whole; just clear the pointer to the
    // lump's beginning position instead of freeing anything
    lumpinfo[lumpnum]->cache = NULL;
}

wadfile_t* W_OpenFile(const char* path)
{
    wadfile_t* result;
    fs* fstream = FS_OpenWAD(path, FS_TRUE);

    if(!fstream)
        return NULL;

    // Create a new wadfile_t to hold the file handle.
    result          = Z_Malloc(sizeof(wadfile_t), PU_STATIC, NULL);
    result->wad_stream = fstream;

    return result;
}

void W_CloseFile(wadfile_t* wad)
{
    FS_CloseWAD(wad->wad_stream);
    Z_Free(wad);
}

void W_CloseFiles(void)
{
    for(int i = 0; i < numwads; i++)
        W_CloseFile(wadlist[i]);
}
