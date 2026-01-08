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

#define __STDC_WANT_LIB_EXT1__ 1

#include <time.h>

#if defined(_WIN32)
#pragma comment(lib, "winmm.lib")
// clang-format off
#include <windows.h>
#include <commdlg.h>
// clang-format on
#endif

#include "automap/am_map.h"
#include "console/c_cmds.h"
#include "console/c_console.h"
#include "doom/d_deh.h"
#include "doom/d_iwad.h"
#include "doom/d_main.h"
#include "doom/doomstat.h"
#include "game/g_game.h"
#include "hud/hu_stuff.h"
#include "math/math_colors.h"
#include "system/i_input.h"
#include "math/math_swap.h"
#include "system/i_system.h"
#include "system/i_video.h"
#include "system/i_timer.h"
#include "utils/m_argv.h"
#include "system/i_config.h"
#include "menu/m_menu.h"
#include "utils/m_misc.h"
#include "playsim/p_local.h"
#include "playsim/p_setup.h"
#include "render/r_main.h"
#include "sound/s_sound.h"
#include "hud/st_stuff.h"
#include "render/v_draw.h"
#include "render/v_video.h"
#include "system/i_filesystem.h"
#include "system/i_version.h"
#include "wad/w_merge.h"
#include "wad/w_wad.h"

#define MAXDEHFILES 16

char** episodes[] = { &s_CAPTION_EPISODE1, &s_CAPTION_EPISODE2, &s_CAPTION_EPISODE3,
    &s_CAPTION_EPISODE4, &s_CAPTION_EPISODE5, &s_CAPTION_EPISODE6, &s_CAPTION_EPISODE7,
    &s_CAPTION_EPISODE8, &s_CAPTION_EPISODE9, &s_CAPTION_EPISODE10 };

char** expansions[] = { &s_CAPTION_EXPANSION1, &s_CAPTION_EXPANSION2 };

char** skilllevels[] = { &s_M_SKILLLEVEL1, &s_M_SKILLLEVEL2, &s_M_SKILLLEVEL3,
    &s_M_SKILLLEVEL4, &s_M_SKILLLEVEL5 };

static char* iwadsrequired[] = { "doom.wad", "doom2.wad", "tnt.wad",
    "plutonia.wad", "nerve.wad", "doom2.wad" };

// Location where savegames are stored
char* savegamefolder;

char* autoloadfolder;
char* autoloadiwadsubfolder;
char* autoloadpwadsubfolder;
char* autoloadnervesubfolder  = "";

char* pwadfile = "";

char* configfile;
char* resourcewad;

static char dehwarning[256];

bool devparm;  // started game with -devparm
bool fastparm; // checkparm of -fast
bool freeze;
bool infiniteammo;
bool nomonsters;  // checkparm of -nomonsters
bool pistolstart; // [BH] checkparm of -pistolstart
bool regenhealth;
bool respawnitems;
bool respawnmonsters; // checkparm of -respawn
bool solonet;         // checkparm of -solo-net

skill_t startskill;
int startepisode;
static int startmap;
bool autostart;

static bool error;

static player_t player = { 0 };

static char dehfiles[MAXDEHFILES][MAX_PATH];
static int dehfilecount;

static bool DehFileProcessed(const char* path)
{
    for(int i = 0; i < dehfilecount; i++)
        if(M_StringCompare(path, dehfiles[i]))
            return true;

    return false;
}

static char* FindDehPath(char* path, const char* ext)
{
    // Returns a malloc'd path to the .deh file that matches a WAD path.
    // Or NULL if no matching .deh file can be found.
    char* dehpath = M_StringDuplicate(path);

    if(M_StringEndsWith(path, ".wad"))
        dehpath = M_StringReplaceFirst(path, ".wad", ext);
    else if(M_StringEndsWith(path, ".iwad"))
        dehpath = M_StringReplaceFirst(path, ".iwad", ext);
    else if(M_StringEndsWith(path, ".pwad"))
        dehpath = M_StringReplaceFirst(path, ".pwad", ext);

    return (M_FileExists(dehpath) ? dehpath : NULL);
}

static void LoadDEHFile(char* path, bool autoloaded)
{
    char* dehpath = FindDehPath(path, ".bex");

    if(dehpath)
    {
        if(!DehFileProcessed(dehpath))
        {
            if(!HasDehackedLump(path))
                D_ProcessDehFile(dehpath, 0, autoloaded);

            if(dehfilecount < MAXDEHFILES)
            {
                M_StringCopy(dehfiles[dehfilecount], dehpath, sizeof(dehfiles[0]));
                dehfilecount++;
            }
        }
    }
    else
    {
        dehpath = FindDehPath(path, ".deh");

        if(dehpath && !DehFileProcessed(dehpath))
        {
            if(!HasDehackedLump(path))
                D_ProcessDehFile(dehpath, 0, autoloaded);

            if(dehfilecount < MAXDEHFILES)
            {
                M_StringCopy(dehfiles[dehfilecount], dehpath, sizeof(dehfiles[0]));
                dehfilecount++;
            }
        }
    }
}

bool D_IsDOOM1IWAD(const char* filename)
{
    const char* file = leafname(filename);

    return (M_StringCompare(file, "DOOM.WAD") ||
    M_StringCompare(file, "DOOM1.WAD") || M_StringCompare(file, "DOOMU.WAD") ||
    M_StringCompare(file, "BFGDOOM.WAD") || M_StringCompare(file, "KEXDOOM.WAD") ||
    M_StringCompare(file, "UNITYDOOM.WAD") || M_StringCompare(file, "DOOMBFG.WAD") ||
    M_StringCompare(file, "DOOMKEX.WAD") || M_StringCompare(file, "DOOMUNITY.WAD"));
}

bool D_IsDOOM2IWAD(const char* filename)
{
    const char* file = leafname(filename);

    return (M_StringCompare(file, "DOOM2.WAD") || M_StringCompare(file, "DOOM2F.WAD") ||
    M_StringCompare(file, "BFGDOOM2.WAD") || M_StringCompare(file, "KEXDOOM2.WAD") ||
    M_StringCompare(file, "UNITYDOOM2.WAD") || M_StringCompare(file, "DOOM2BFG.WAD") ||
    M_StringCompare(file, "DOOM2KEX.WAD") || M_StringCompare(file, "DOOM2UNITY.WAD"));
}

bool D_IsDOOMIWAD(const char* filename)
{
    const char* file = leafname(filename);

    return (D_IsDOOM1IWAD(filename) || D_IsDOOM2IWAD(filename) ||
    M_StringCompare(file, "chex.wad") || M_StringCompare(file, "rekkrsa.wad"));
}

bool D_IsFinalDOOMIWAD(const char* filename)
{
    const char* file = leafname(filename);

    return (M_StringCompare(file, "PLUTONIA.WAD") || M_StringCompare(file, "TNT.WAD"));
}

bool D_IsResourceWAD(const char* filename)
{
    return (M_StringCompare(leafname(filename), DOOMRETRO_RESOURCEWAD));
}

static bool D_IsUnsupportedIWAD(const char* filename)
{
    const struct
    {
        char* iwad;
        char* title;
    } unsupported[] = { { "heretic.wad", "Heretic" }, { "heretic1.wad", "Heretic" },
        { "hexen.wad", "Hexen" }, { "hexdd.wad", "Hexen" }, { "strife0.wad", "Strife" },
        { "strife1.wad", "Strife" }, { "voices.wad", "Strife" } };

    for(int i = 0; i < arrlen(unsupported); i++)
        if(M_StringCompare(leafname(filename), unsupported[i].iwad))
        {
            char buffer[1024];

            M_snprintf(buffer, sizeof(buffer),
            DOOMRETRO_NAME " doesn't support %s.\n", unsupported[i].title);

            error = true;
            return true;
        }

    return false;
}

static bool D_IsWADFile(const char* filename)
{
    return (M_StringEndsWith(filename, ".wad") ||
    M_StringEndsWith(filename, ".iwad") || M_StringEndsWith(filename, ".pwad"));
}

static bool D_IsCFGFile(const char* filename)
{
    return M_StringEndsWith(filename, ".cfg");
}

static bool D_IsDEHFile(const char* filename)
{
    return (M_StringEndsWith(filename, ".deh") || M_StringEndsWith(filename, ".bex"));
}


static void D_ProcessDehOnCmdLine(void)
{
    const char *p = M_GetParms("deh", "bex", NULL);

    if(*p != '\0')
    {
        if(!strchr(p, ','))
        {
            D_ProcessDehFile(p, 0, false);
        }
        else
        {
            size_t len = strlen(p);
            size_t begin = 0;
            size_t delim = 0;
            const char *check = p;

            while(strchr(check, ','))
            {
                if(*check == ',' && delim > begin)
                {
                    char *file = M_SubString(p, begin, delim-begin);
                    if(file)
                    {
                        D_ProcessDehFile(file, 0, false);
                        free(file);
                    }
                    ++delim;
                    begin = delim;
                }
                ++delim;
                ++check;
            }
            if (*check != '\0')
            {
                D_ProcessDehFile(check, 0, false);
            }
        }
    }
}

static void D_ProcessDehInWad(void)
{
    if(*dehwarning)
        C_Warning(1, dehwarning);

    if(!M_CheckParm("nodeh") && !M_CheckParm("nobex"))
        for(int i = 0; i < numlumps; i++)
            if(M_StringCompare(lumpinfo[i]->name, "DEHACKED") &&
            !D_IsResourceWAD(lumpinfo[i]->wadfile->path))
                D_ProcessDehFile(NULL, i, false);

    for(int i = numlumps - 1; i >= 0; i--)
        if(M_StringCompare(lumpinfo[i]->name, "DEHACKED") &&
        D_IsResourceWAD(lumpinfo[i]->wadfile->path))
        {
            D_ProcessDehFile(NULL, i, false);
            break;
        }
}

//
// D_DoomMainSetup
//
// CPhipps - the old contents of D_DoomMain, but moved out of the main
//  line of execution so its stack space can be freed
static void D_DoomMainSetup(void)
{
    const char *p            = M_GetParm("config");
    int choseniwad   = 0;
    bool autoloading = false;
    char lumpname[6];
    const char* appdatafolder = M_GetAppDataFolder();
    char* iwadfile;
    int startloadgame;
    const char* resourcefolder = M_GetResourceFolder();

    I_TimeInit();

    resourcewad =
    M_StringJoin(resourcefolder, DIR_SEPARATOR_S, DOOMRETRO_RESOURCEWAD, NULL);

    M_MakeDirectory(appdatafolder);
    configfile =
    (*p != '\0' ? M_StringDuplicate(p) :
         M_StringJoin(appdatafolder, DIR_SEPARATOR_S, DOOMRETRO_CONFIGFILE, NULL));

    C_ClearConsole();

    dsdh_InitTables();
    D_BuildBEXTables();

    C_PrintCompileDate();

    // Load configuration files before initializing other subsystems.
    M_LoadCVARs(configfile);

    iwadfile = D_FindIWAD();

    for(int i = 0; i < MAXALIASES; i++)
    {
        aliases[i].name[0]   = '\0';
        aliases[i].string[0] = '\0';
    }

    if(M_StringCompare(wadfolder, wadfolder_default) || !M_FolderExists(wadfolder))
        D_InitWADfolder();

    if((respawnmonsters = M_CheckParm("respawn")))
        C_Output(
        "A " BOLD("respawn") " parameter was found on the command-line. "
                              "Monsters will now respawn.");
    else if((respawnmonsters = M_CheckParm("respawnmonsters")))
        C_Output("A " BOLD(
        "respawnmonsters") " parameter was found on the command-line. "
                            "Monsters will now respawn.");

    if((nomonsters = M_CheckParm("nomonsters")))
    {
        C_Output(
        "A " BOLD("nomonsters") " parameter was found on the command-line. "
                                 "No monsters will now be spawned.");
        stat_cheatsentered = SafeAdd(stat_cheatsentered, 1);
        M_SaveCVARs();
    }

    if((pistolstart = M_CheckParm("pistolstart")))
        C_Output(
        "A " BOLD("pistolstart") " parameter was found on the command-line. "
                                  "The player will now start each map with "
                                  "100%% health, no armor, "
                                  "and only a pistol with 50 bullets.");

    if((fastparm = M_CheckParm("fast")))
        C_Output("A " BOLD("fast") " parameter was found on the command-line. "
                                    "Monsters will now be fast.");
    else if((fastparm = M_CheckParm("fastmonsters")))
        C_Output(
        "A " BOLD("fastmonsters") " parameter was found on the command-line. "
                                   "Monsters will now be fast.");

    if((solonet = M_CheckParm("solonet")))
        C_Output(
        "A " BOLD("solonet") " parameter was found on the command-line. "
                              "Things usually intended for multiplayer will "
                              "now spawn at the start of each map, "
                              "and the player will respawn without the map "
                              "restarting if they die.");
    else if((solonet = M_CheckParm("solo-net")))
        C_Output(
        "A " BOLD("solo-net") " parameter was found on the command-line. "
                               "Things usually intended for multiplayer will "
                               "now spawn at the start of each map, "
                               "and the player will respawn without the map "
                               "restarting if they die.");

    if((devparm = M_CheckParm("devparm")))
        C_Output(
        "A " BOLD("devparm") " parameter was found on the command-line. %s", s_D_DEVSTR);

    // turbo option
    if(M_CheckParm("turbo"))
    {
        int scale = turbo_default * 2;

        p = M_GetParm("turbo");

        if(*p != '\0')
        {
            scale = strtol(p, NULL, 10);

            if(scale < turbo_min || scale > turbo_max)
                scale = turbo_default * 2;
        }

        C_Output(
        "A " BOLD("-turbo") " parameter was found on the command-line. "
                            "The player will now be %i%% their normal speed.",
        scale);

        if(scale != turbo_default)
            G_SetMovementSpeed(scale);

        if(scale > turbo_default)
        {
            stat_cheatsentered = SafeAdd(stat_cheatsentered, 1);
            M_SaveCVARs();
        }
    }
    else
        G_SetMovementSpeed(turbo);

    // init subsystems
    if(!R_ResizeRenderState(r_scale))  // Initialize render state before V_Init needs it
        I_Error("Failed to initialize render state");

    V_Init();

    if(!stat_runs)
    {
        const time_t now       = time(NULL);
        struct tm* currenttime = localtime(&now);

        stat_firstrun = (uint64_t)currenttime->tm_mday +
        ((uint64_t)currenttime->tm_mon + 1) * 100 +
        ((uint64_t)currenttime->tm_year + 1900) * 10000;

        C_Output("This is the first time " ITALICS(
        DOOMRETRO_NAME) " has been run on this " DEVICE ".");
    }
    else
    {
        char* temp = commify(SafeAdd(stat_runs, 1));

        if(stat_firstrun)
        {
            const int day   = stat_firstrun % 100;
            const int month = (stat_firstrun % 10000) / 100;
            const int year  = (int)stat_firstrun / 10000;

            C_Output(
            ITALICS(DOOMRETRO_NAME) " has been run %s times on this " DEVICE
                                    " since it was installed on %s, %s %i, %i.",
            temp, dayofweek(day, month, year), monthnames[month - 1], day, year);
        }
        else
            C_Output(
            ITALICS(DOOMRETRO_NAME) " has been run %s times on this " DEVICE ".", temp);

        free(temp);
    }

    if(!M_FileExists(resourcewad))
        I_Error("%s can't be found.", resourcewad);

    if(M_CheckParm("nodeh"))
        C_Output("A " BOLD(
        "nodeh") " parameter was found on the command-line. "
                  "All " BOLD("DEHACKED") " lumps will now be ignored.");
    else if(M_CheckParm("nobex"))
        C_Output("A " BOLD(
        "nobex") " parameter was found on the command-line. "
                  "All " BOLD("DEHACKED") " lumps will now be ignored.");

    p = M_GetParms("file", "pwad", "merge");

    if(iwadfile)
    {
        if(W_AddFile(iwadfile, false))
            stat_runs = SafeAdd(stat_runs, 1);
    }
    else if(*p == '\0')
    {
        stat_runs = SafeAdd(stat_runs, 1);
    }

    M_SaveCVARs();

    if(*p != '\0')
    {
        if(!strchr(p, ','))
        {
            char* file = D_TryFindWADByName(M_StringDuplicate(p));
            if(iwadfile)
            {
                if(W_MergeFile(file, false))
                {
                    modifiedgame = true;

                    if(IWADRequiredByPWAD(file) != none)
                        pwadfile = M_StringDuplicate(leafname(file));
                }
            }
            else
            {
                gamemission_t iwadrequired = IWADRequiredByPWAD(file);
                char fullpath[MAX_PATH];
                char* folder = M_ExtractFolder(file);

                if(iwadrequired == none)
                    iwadrequired = doom2;

                // try the current folder first
                M_snprintf(fullpath, sizeof(fullpath), "%s" DIR_SEPARATOR_S "%s",
                folder, iwadsrequired[iwadrequired]);
                D_IdentifyIWADByName(fullpath);

                if(W_AddFile(fullpath, true))
                {
                    iwadfile  = M_StringDuplicate(fullpath);
                    wadfolder = M_StringDuplicate(folder);

                    if(W_MergeFile(file, false))
                    {
                        modifiedgame = true;

                        if(IWADRequiredByPWAD(file) != none)
                            pwadfile = M_StringDuplicate(leafname(file));
                    }
                }
                else
                {
                    // otherwise try the wadfolder CVAR
                    M_snprintf(fullpath, sizeof(fullpath), "%s" DIR_SEPARATOR_S "%s",
                    wadfolder, iwadsrequired[iwadrequired]);
                    D_IdentifyIWADByName(fullpath);

                    if(W_AddFile(fullpath, true))
                    {
                        iwadfile = M_StringDuplicate(fullpath);

                        if(W_MergeFile(file, false))
                        {
                            modifiedgame = true;

                            if(IWADRequiredByPWAD(file) != none)
                                pwadfile = M_StringDuplicate(leafname(file));
                        }
                    }
                    else
                    {
                        // still nothing? try some common installation folders
                        if(W_AddFile(D_FindWADByName(iwadsrequired[iwadrequired]), true))
                        {
                            iwadfile = M_StringDuplicate(fullpath);

                            if(W_MergeFile(file, false))
                            {
                                modifiedgame = true;

                                if(IWADRequiredByPWAD(file) != none)
                                    pwadfile = M_StringDuplicate(leafname(file));
                            }
                        }
                    }
                }

                free(folder);
            }
        }
        else
        {
            size_t len = strlen(p);
            size_t begin = 0;
            size_t delim = 0;
            const char *check = p;

            while(strchr(check, ','))
            {
                if(*check == ',' && delim > begin)
                {
                    char *file = M_SubString(p, begin, delim-begin);
                    if(file)
                    {
                        file = D_TryFindWADByName(file);
                        if(iwadfile)
                        {
                            if(W_MergeFile(file, false))
                            {
                                modifiedgame = true;

                                if(IWADRequiredByPWAD(file) != none)
                                    pwadfile = M_StringDuplicate(leafname(file));
                            }
                        }
                        else
                        {
                            gamemission_t iwadrequired = IWADRequiredByPWAD(file);
                            char fullpath[MAX_PATH];
                            char* folder = M_ExtractFolder(file);

                            if(iwadrequired == none)
                                iwadrequired = doom2;

                            // try the current folder first
                            M_snprintf(fullpath, sizeof(fullpath), "%s" DIR_SEPARATOR_S "%s",
                            folder, iwadsrequired[iwadrequired]);
                            D_IdentifyIWADByName(fullpath);

                            if(W_AddFile(fullpath, true))
                            {
                                iwadfile  = M_StringDuplicate(fullpath);
                                wadfolder = M_StringDuplicate(folder);

                                if(W_MergeFile(file, false))
                                {
                                    modifiedgame = true;

                                    if(IWADRequiredByPWAD(file) != none)
                                        pwadfile = M_StringDuplicate(leafname(file));
                                }
                            }
                            else
                            {
                                // otherwise try the wadfolder CVAR
                                M_snprintf(fullpath, sizeof(fullpath), "%s" DIR_SEPARATOR_S "%s",
                                wadfolder, iwadsrequired[iwadrequired]);
                                D_IdentifyIWADByName(fullpath);

                                if(W_AddFile(fullpath, true))
                                {
                                    iwadfile = M_StringDuplicate(fullpath);

                                    if(W_MergeFile(file, false))
                                    {
                                        modifiedgame = true;

                                        if(IWADRequiredByPWAD(file) != none)
                                            pwadfile = M_StringDuplicate(leafname(file));
                                    }
                                }
                                else
                                {
                                    // still nothing? try some common installation folders
                                    if(W_AddFile(D_FindWADByName(iwadsrequired[iwadrequired]), true))
                                    {
                                        iwadfile = M_StringDuplicate(fullpath);

                                        if(W_MergeFile(file, false))
                                        {
                                            modifiedgame = true;

                                            if(IWADRequiredByPWAD(file) != none)
                                                pwadfile = M_StringDuplicate(leafname(file));
                                        }
                                    }
                                }
                            }

                            free(folder);
                        }
                    }
                    ++delim;
                    begin = delim;
                }
                ++delim;
                ++check;
            }
            if (*check != '\0')
            {
                char *file = D_TryFindWADByName(M_StringDuplicate(check));
                if(iwadfile)
                {
                    if(W_MergeFile(file, false))
                    {
                        modifiedgame = true;

                        if(IWADRequiredByPWAD(file) != none)
                            pwadfile = M_StringDuplicate(leafname(file));
                    }
                }
                else
                {
                    gamemission_t iwadrequired = IWADRequiredByPWAD(file);
                    char fullpath[MAX_PATH];
                    char* folder = M_ExtractFolder(file);

                    if(iwadrequired == none)
                        iwadrequired = doom2;

                    // try the current folder first
                    M_snprintf(fullpath, sizeof(fullpath), "%s" DIR_SEPARATOR_S "%s",
                    folder, iwadsrequired[iwadrequired]);
                    D_IdentifyIWADByName(fullpath);

                    if(W_AddFile(fullpath, true))
                    {
                        iwadfile  = M_StringDuplicate(fullpath);
                        wadfolder = M_StringDuplicate(folder);

                        if(W_MergeFile(file, false))
                        {
                            modifiedgame = true;

                            if(IWADRequiredByPWAD(file) != none)
                                pwadfile = M_StringDuplicate(leafname(file));
                        }
                    }
                    else
                    {
                        // otherwise try the wadfolder CVAR
                        M_snprintf(fullpath, sizeof(fullpath), "%s" DIR_SEPARATOR_S "%s",
                        wadfolder, iwadsrequired[iwadrequired]);
                        D_IdentifyIWADByName(fullpath);

                        if(W_AddFile(fullpath, true))
                        {
                            iwadfile = M_StringDuplicate(fullpath);

                            if(W_MergeFile(file, false))
                            {
                                modifiedgame = true;

                                if(IWADRequiredByPWAD(file) != none)
                                    pwadfile = M_StringDuplicate(leafname(file));
                            }
                        }
                        else
                        {
                            // still nothing? try some common installation folders
                            if(W_AddFile(D_FindWADByName(iwadsrequired[iwadrequired]), true))
                            {
                                iwadfile = M_StringDuplicate(fullpath);

                                if(W_MergeFile(file, false))
                                {
                                    modifiedgame = true;

                                    if(IWADRequiredByPWAD(file) != none)
                                        pwadfile = M_StringDuplicate(leafname(file));
                                }
                            }
                        }
                    }

                    free(folder);
                }
            }
        }
    }

    if(!iwadfile && !modifiedgame && !choseniwad)
        I_Error(DOOMRETRO_NAME " couldn't find any IWADs.");

    W_Init();
    D_IdentifyVersion();

    if(!M_CheckParm("noautoload") && game.mode != shareware)
    {
        D_SetAutoloadFolder();

        autoloading = W_AutoloadFiles(autoloadfolder);
        autoloading |= W_AutoloadFiles(autoloadiwadsubfolder);

        if(autoloadpwadsubfolder)
            autoloading |= W_AutoloadFiles(autoloadpwadsubfolder);

        if(autoloading)
            W_Init();
    }

    W_CheckForPNGLumps();

    FREEDM = (W_CheckNumForName("FREEDM") >= 0);

    PLAYPALs =
    (FREEDOOM ? 2 : W_GetNumLumps("PLAYPAL"));
    STBARs = W_GetNumLumps("STBAR");

    DBIGFONT = (W_CheckNumForName("DBIGFONT") >= 0);
    DSFLAMST = (W_GetNumLumps("DSFLAMST") > 1);
    M_DOOM   = (W_GetNumLumps("M_DOOM") > 2);
    M_EPISOD = (W_GetNumLumps("M_EPISOD") > 1);
    M_GDHIGH = (W_GetNumLumps("M_GDHIGH") > 1);
    M_GDLOW  = (W_GetNumLumps("M_GDLOW") > 1);
    M_LOADG  = (W_GetNumLumps("M_LOADG") > 1);
    M_LGTTL  = (W_GetNumLumps("M_LGTTL") > 1);
    M_LSCNTR = (W_GetNumLumps("M_LSCNTR") > 1);
    M_MSENS  = (W_GetNumLumps("M_MSENS") > 1);
    M_MSGOFF = (W_GetNumLumps("M_MSGOFF") > 1);
    M_MSGON  = (W_GetNumLumps("M_MSGON") > 1);
    M_NEWG   = (W_GetNumLumps("M_NEWG") > 1);
    M_NGAME  = (W_GetNumLumps("M_NGAME") > 1);
    M_NMARE  = (W_GetNumLumps("M_NMARE") > 1);
    M_OPTTTL = (W_GetNumLumps("M_OPTTTL") > 1);
    M_PAUSE  = (W_GetNumLumps("M_PAUSE") > 1);
    M_SAVEG  = (W_GetNumLumps("M_SAVEG") > 1);
    M_SGTTL  = (W_GetNumLumps("M_SGTTL") > 1);
    M_SKILL  = (W_GetNumLumps("M_SKILL") > 1);
    M_SKULL1 = (W_GetNumLumps("M_SKULL1") > 1);
    M_SVOL   = (W_GetNumLumps("M_SVOL") > 1);
    STYSNUM0 = (W_GetNumLumps("STYSNUM0") > 1);
    WICOLON  = (W_GetNumLumps("WICOLON") > 1);
    WISCRT2  = (W_GetNumLumps("WISCRT2") > 1);

    I_InitGraphics();
    I_InitInput();

    D_ProcessDehOnCmdLine();
    D_ProcessDehInWad();
    D_PostProcessDeh();
    D_TranslateDehStrings();
    D_SetGameDescription();

    if(dehcount > 2)
    {
        if(game.mode == shareware)
        {
            I_Error(
            "Other files can't be loaded with the shareware version of DOOM.");
        }

        C_Warning(0, "Loading multiple " BOLD("DEHACKED") " lumps or files may cause unexpected results.");
    }

    if(!autoloading)
    {
        if(autoloadpwadsubfolder)
            C_Output(
            "Any " BOLD(".wad") ", " BOLD(".deh") " or " BOLD(
            ".cfg") " files in " BOLD("%s") ", " BOLD("%s") " or " BOLD("%s") " will be automatically loaded.",
            autoloadfolder, autoloadiwadsubfolder, autoloadpwadsubfolder);
        else
            C_Output("Any " BOLD(".wad") ", " BOLD(".deh") " or " BOLD(".cfg") " files in " BOLD(
                     "%s") " or " BOLD("%s") " will be automatically loaded.",
            autoloadfolder, autoloadiwadsubfolder);
    }

    if(!M_StringCompare(s_VERSION, DOOMRETRO_NAMEANDVERSIONSTRING))
    {
        I_Error("The wrong version of %s was found.", resourcewad);
    }

    FREEDOOM1 = (FREEDOOM && game.mission == doom);

    D_SetSaveGameFolder(true);

    D_SetScreenshotsFolder();

    C_Output(
    "Files created using the " BOLD("condump") " CCMD are placed in " BOLD(
    "%s" DIR_SEPARATOR_S DOOMRETRO_CONSOLEFOLDER DIR_SEPARATOR_S) ".",
    appdatafolder);

    // Check for -file in shareware
    if(modifiedgame)
    {
        if(game.mode == shareware)
            I_Error(
            "Other files can't be loaded with the shareware version of DOOM.");

        // Check for fake IWAD with right name,
        // but w/o all the lumps of the registered version.
        if(game.mode == registered)
        {
            // These are the lumps that will be checked in IWAD,
            // if any one is not present, execution will be aborted.
            const char name[23][9] = { "E2M1", "E2M2", "E2M3", "E2M4", "E2M5",
                "E2M6", "E2M7", "E2M8", "E2M9", "E3M1", "E3M3", "E3M3", "E3M4",
                "E3M5", "E3M6", "E3M7", "E3M8", "E3M9", "DPHOOF", "BFGGA0",
                "HEADA1", "CYBRA1", "SPIDA1D1" };

            for(int i = 0; i < 23; i++)
                if(W_CheckNumForName(name[i]) < 0)
                    I_Error("This is not the registered version of DOOM.WAD.");
        }
    }

    // get skill/episode/map from parms
    startskill   = sk_medium;
    startepisode = 1;
    startmap     = 1;
    autostart    = false;

    if(M_CheckParm("skill") || M_CheckParm("skilllevel"))
    {
        p = M_GetParms("skill", "skilllevel", NULL);

        if(*p != '\0')
        {
            const int temp = p[0] - '1';

            if(temp >= sk_baby && temp <= sk_nightmare)
            {
                char* string = titlecase(*skilllevels[temp]);

                startskill = (skill_t)temp;
                skilllevel = startskill + 1;
                M_SaveCVARs();

                M_StringReplaceAll(string, ".", "", false);
                M_StringReplaceAll(string, "!", "", false);

                C_Output(
                "A " BOLD("%s") " parameter was found on the command-line. "
                                "The skill level is now " ITALICS("%s."),
                p, string);
                free(string);
            }
        }
    }

    if(M_CheckParm("episode") && game.mode != commercial)
    {
        p = M_GetParm("episode");

        if (*p != '\0')
        {
            const int temp = p[0] - '0';

            if((game.mode == shareware && temp == 1) ||
            (temp >= 1 &&
            ((game.mode == registered && temp <= 3) || (game.mode == retail && temp <= 4))))
            {
                startepisode = temp;
                episode      = temp;
                M_SaveCVARs();
                M_snprintf(lumpname, sizeof(lumpname), "E%iM%i", startepisode, startmap);
                autostart = true;
                C_Output(
                "An " BOLD("-episode") " parameter was found on the command-line. "
                                    "The episode is now " ITALICS("%s."),
                *episodes[episode - 1]);
            }
        }
    }

    if(M_CheckParm("expansion") && game.mode == commercial)
    {
        p = M_GetParm("expansion");

        if (*p != '\0')
        {
            const int temp = p[0] - '0';

            if(temp == 1)
            {
                game.mission = doom2;
                expansion   = temp;
                M_SaveCVARs();
                M_snprintf(lumpname, sizeof(lumpname), "MAP%02i", startmap);
                autostart = true;
                C_Output("An " BOLD(
                        "-expansion") " parameter was found on the command-line. "
                                    "The expansion is now " ITALICS("%s."),
                *expansions[expansion - 1]);
            }
        }
    }

    if(M_CheckParm("warp") || M_CheckParm("map"))
    {
        p = M_GetParms("warp", "map", NULL);

        if(*p != '\0')
        {
            if(game.mode == commercial)
            {
                if(strlen(p) == 5 && toupper(p[0]) == 'M' &&
                toupper(p[1]) == 'A' && toupper(p[2]) == 'P' &&
                isdigit((int)p[3]) && isdigit((int)p[4]))
                    startmap = (p[3] - '0') * 10 + p[4] - '0';
                else
                    startmap = strtol(p, NULL, 10);

                M_snprintf(lumpname, sizeof(lumpname), "MAP%02i", startmap);
            }
            else
            {
                if(strlen(p) == 4 && toupper(p[0]) == 'E' &&
                isdigit((int)p[1]) &&
                toupper(p[2]) == 'M' && isdigit((int)p[3]))
                {
                    startepisode = p[1] - '0';
                    startmap     = p[3] - '0';
                    M_snprintf(lumpname, sizeof(lumpname), "E%iM%i", startepisode, startmap);
                }
            }
            if(W_CheckNumForName(lumpname) >= 0)
            {
                autostart = true;

                if(startmap > 1)
                {
                    stat_cheatsentered = SafeAdd(stat_cheatsentered, 1);
                    M_SaveCVARs();
                }
            }
        }
    }

    if(M_CheckParm("dog"))
    {
        P_InitHelperDogs(1);

        C_Output(
        "A " BOLD("dog") " parameter was found on the command-line. "
                          "A friendly dog will enter the game with %s.",
        (M_StringCompare(playername, playername_default) ? "you" : playername));
    }
    else if(M_CheckParm("dogs"))
    {
        p = M_GetParm("dogs");

        if (*p != '\0')
        {
            const int dogs = strtol(p, NULL, 10);

            if(dogs == 1)
            {
                P_InitHelperDogs(1);

                C_Output(
                "A " BOLD("dogs") " parameter was found on the command-line. "
                                "A friendly dog will enter the game with %s.",
                (M_StringCompare(playername, playername_default) ? "you" : playername));
            }
            else if(dogs > 1)
            {
                P_InitHelperDogs(MIN(dogs, MAXFRIENDS));

                C_Output(
                "A " BOLD(
                "dogs") " parameter was found on the command-line. "
                        "Up to %i friendly dogs will enter the game with %s.",
                MIN(dogs, MAXFRIENDS),
                (M_StringCompare(playername, playername_default) ? "you" : playername));
            }
        }
        else
        {
            P_InitHelperDogs(MAXFRIENDS);

            C_Output("A " BOLD(
                    "dogs") " parameter was found on the command-line. "
                            "Up to %i friendly dogs will enter the game with %s.",
            MAXFRIENDS,
            (M_StringCompare(playername, playername_default) ? "you" : playername));
        }
    }

    M_Init();
    R_Init();
    P_Init();
    S_Init();
    HU_Init();
    ST_Init();
    AM_Init();
    C_Init();
    V_InitColorTranslation();

    if(M_CheckParm("loadgame"))
    {
        p = M_GetParm("loadgame");

        if(*p != '\0')
        {
            startloadgame = strtol(p, NULL, 10);
            if (startloadgame >= 0 && startloadgame < savegame_max)
            {
                menuactive   = false;
                I_InitKeyboard();

                if(alwaysrun)
                    C_StringCVAROutput(stringize(alwaysrun), "on");

                G_LoadGame(P_SaveGameFile(startloadgame));
            }
        }
    }
    else
        startloadgame = -1;

    D_InitTitleScreen();

    if(game.action != ga_loadgame)
    {
        if(autostart)
        {
            menuactive   = false;
            I_InitKeyboard();

            if(alwaysrun)
                C_StringCVAROutput(stringize(alwaysrun), "on");

            if(M_CheckParm("warp"))
                C_Output(
                "A " BOLD("warp") " parameter was found on the command-line. "
                                   "Warping %s to %s...",
                (M_StringCompare(playername, playername_default) ? "you" : playername),
                lumpname);
            else if(M_CheckParm("map"))
                C_Output(
                "A " BOLD("map") " parameter was found on the command-line. "
                                  "Warping %s to %s...",
                (M_StringCompare(playername, playername_default) ? "you" : playername),
                lumpname);
            else
                C_Output("Warping %s to %s...",
                (M_StringCompare(playername, playername_default) ? "you" : playername),
                lumpname);

            G_DeferredInitNew(startskill, startepisode, startmap);
        }
        else
        {
            menuactive   = false;
            D_FadeScreen(false);
            D_StartTitle(1);
        }
    }

    I_Sleep(500);
}

//
// D_DoomMain
//
void D_DoomMain(void)
{
    FS_Open();

    D_DoomMainSetup(); // CPhipps - setup out of main execution stack

    viewplayer = &player;
    memset(viewplayer, 0, sizeof(*viewplayer));

    R_ExecuteSetViewSize();

    // D_DoomLoop();       // never returns
}
