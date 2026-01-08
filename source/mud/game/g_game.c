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

#include "automap/am_map.h"
#include "console/c_console.h"
#include "doom/d_deh.h"
#include "doom/doomstat.h"
#include "intermission/f_finale.h"
#include "game/g_event.h"
#include "game/g_game.h"
#include "hud/hu_stuff.h"
#include "math/math_colors.h"
#include "system/i_controller.h"
#include "system/i_filesystem.h"
#include "system/i_input.h"
#include "system/i_system.h"
#include "system/i_timer.h"
#include "system/i_config.h"
#include "menu/m_menu.h"
#include "utils/m_misc.h"
#include "playsim/p_local.h"
#include "playsim/p_saveg.h"
#include "playsim/p_setup.h"
#include "playsim/p_tick.h"
#include "render/r_sky.h"
#include "sound/s_sound.h"
#include "hud/st_stuff.h"
#include "render/v_video.h"
#include "intermission/wi_stuff.h"

static void G_DoReborn(void);

static void G_DoNewGame(void);
static void G_DoCompleted(void);
static void G_DoWorldDone(void);
static void G_DoSaveGame(void);

char speciallumpname[6] = "";

bool paused;

bool viewactive;


bool resetinventory = false;

wbstartstruct_t wminfo; // parms for world map/intermission

int savegameslot;
char savename[MAX_PATH];
static char savedescription[SAVESTRINGSIZE];

gameaction_t loadaction = ga_nothing;

static void G_SetInitialWeapon(void)
{
    viewplayer->weaponowned[wp_fist]   = true;
    viewplayer->weaponowned[wp_pistol] = true;
    viewplayer->ammo[am_clip]          = initial_bullets;
    viewplayer->readyweapon =
    (!initial_bullets && weaponinfo[wp_pistol].ammotype != am_noammo ? wp_fist : wp_pistol);
    viewplayer->pendingweapon = viewplayer->readyweapon;

    for(ammotype_t i = 0; i < NUMAMMO; i++)
        viewplayer->maxammo[i] = maxammo[i];
}

//
// G_ResetPlayer
// [BH] Reset player's health, armor, weapons and ammo
//
static void G_ResetPlayer(void)
{
    viewplayer->health           = initial_health;
    viewplayer->armor            = 0;
    viewplayer->armortype        = armortype_none;
    viewplayer->preferredshotgun = wp_shotgun;
    viewplayer->fistorchainsaw   = wp_fist;
    viewplayer->backpack         = false;
    memset(viewplayer->weaponowned, false, sizeof(viewplayer->weaponowned));
    memset(viewplayer->ammo, 0, sizeof(viewplayer->ammo));
    resetinventory = false;

    G_SetInitialWeapon();
}

//
// G_DoLoadLevel
//
void G_DoLoadLevel(void)
{
    int ep;
    bool resetplayer;

    if(r_diskicon)
    {
        drawdisk     = true;
        drawdisktics = DRAWDISKTICS;
    }

    if(timer)
        P_SetTimer(timer);

    if(wipegamestate == GS_LEVEL)
        wipegamestate = GS_NONE; // force a wipe

    game.state = GS_LEVEL;

    if(viewplayer->playerstate == PST_DEAD)
        viewplayer->playerstate = PST_REBORN;

    viewplayer->damageinflicted = 0;
    viewplayer->damagereceived  = 0;
    viewplayer->cheated         = 0;
    memset(viewplayer->shotssuccessful, 0, sizeof(viewplayer->shotssuccessful));
    viewplayer->shotssuccessful_incinerator   = 0;
    viewplayer->shotssuccessful_calamityblade = 0;
    memset(viewplayer->shotsfired, 0, sizeof(viewplayer->shotsfired));
    viewplayer->shotsfired_incinerator     = 0;
    viewplayer->shotsfired_calamityblade   = 0;
    viewplayer->distancetraveled           = 0;
    viewplayer->gamessaved                 = 0;
    viewplayer->gamesloaded                = 0;
    viewplayer->itemspickedup_ammo_bullets = 0;
    viewplayer->itemspickedup_ammo_cells   = 0;
    viewplayer->itemspickedup_ammo_fuel    = 0;
    viewplayer->itemspickedup_ammo_rockets = 0;
    viewplayer->itemspickedup_ammo_shells  = 0;
    viewplayer->itemspickedup_armor        = 0;
    viewplayer->itemspickedup_health       = 0;
    viewplayer->itemspickedup_keys         = 0;
    viewplayer->itemspickedup_powerups     = 0;
    memset(viewplayer->monsterskilled, 0, sizeof(viewplayer->monsterskilled));
    viewplayer->prevmessage[0]    = '\0';
    viewplayer->prevmessagetics   = 0;
    viewplayer->infightcount      = 0;
    viewplayer->respawncount      = 0;
    viewplayer->resurrectioncount = 0;
    viewplayer->telefragcount     = 0;
    viewplayer->automapopened     = 0;
    viewplayer->monstersgibbed    = 0;

    prevmessage[0] = '\0';
    freeze         = false;

    ep = (game.mode == commercial ? (game.mission == pack_nerve ? 2 : 1) : game.episode);

    // [BH] Reset player's health, armor, weapons and ammo on pistol start
    if((resetplayer = (resetinventory || pistolstart || P_GetMapPistolStart(ep, game.map))))
        G_ResetPlayer();

    if(viewplayer->cheats & CF_CHOPPERS)
    {
        viewplayer->cheats &= ~CF_CHOPPERS;
        viewplayer->powers[pw_invulnerability] = 0;

        if(!(viewplayer->weaponowned[wp_chainsaw] = viewplayer->chainsawbeforechoppers))
            viewplayer->readyweapon = wp_fist;

        oldweaponsowned[wp_chainsaw] = viewplayer->chainsawbeforechoppers;
    }

    // MUD - TODO - This is here to resolve a heap-use-after-free from upstream
    // Doom Retro. Investigate in the future whether or not this is masking another
    // problem
    S_StopSounds();

    P_RemoveBloodSplats();

    // initialize the msecnode_t freelist. phares 03/25/98
    // any nodes in the freelist are gone by now, cleared
    // by Z_FreeTags() when the previous level ended or player
    // died.
    P_FreeSecNodeList();

    P_MapName(ep, game.map);

    P_SetupLevel(ep, game.map);

    // [BH] Reset player's health, armor, weapons and ammo on pistol start
    if(resetplayer && game.map != 1)
    {
        if(M_StringCompare(playername, playername_default))
            C_Warning(0, "You now have 100%% health, no armor, and only a pistol with 50 bullets.");
        else
            C_Warning(0, "%s now has 100%% health, no armor, and only a pistol with 50 bullets.",
            playername);
    }

    skycolumnoffset = 0;

    R_InitSkyMap();
    R_InitColumnFunctions();

    st_facecount = 0;

    game.action = ga_nothing;

    // clear cmd building stuff
    G_ClearInput();
    paused    = false;

    // [BH] clear these as well, since data from prev map can be copied over in G_BuildTiccmd()
    for(int i = 0; i < BACKUPTICS; i++)
        memset(&localcmds[i], 0, sizeof(ticcmd_t));

    P_SetPlayerViewHeight();

    stat_mapsstarted = SafeAdd(stat_mapsstarted, 1);

    I_UpdateBlitFunc(false);

    M_SetWindowCaption();

    if(automapactive)
        AM_Start(automapactive);

    ammohighlight   = 0;
    armorhighlight  = 0;
    healthhighlight = 0;

    ammodiff[am_clip]  = 0;
    ammodiff[am_shell] = 0;
    ammodiff[am_misl]  = 0;
    ammodiff[am_cell]  = 0;
    armordiff          = 0;
    healthdiff         = 0;

    if(r_screensize == r_screensize_max && animatedstats)
        P_AnimateAllStatsFromStart();
}

//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker(void)
{
    // Game state the last time G_Ticker was called.
    static gamestate_t oldgamestate;

    // do player reborn if needed
    if(viewplayer->playerstate == PST_REBORN)
        G_DoReborn();

    P_MapEnd();

    // do things to change the game state
    while(game.action != ga_nothing)
        switch(game.action)
        {
        case ga_loadlevel:
            G_DoLoadLevel();
            break;

        case ga_autoloadgame:
            M_StringCopy(savename, P_SaveGameFile(quicksaveslot), sizeof(savename));
            S_StopSounds();
            G_DoLoadGame();
            break;

        case ga_newgame:
            G_DoNewGame();
            break;

        case ga_loadgame:
            G_DoLoadGame();
            break;

        case ga_savegame:
        case ga_autosavegame:
            G_DoSaveGame();
            break;

        case ga_completed:
            G_DoCompleted();
            break;

        case ga_victory:
            F_StartFinale();
            break;

        case ga_worlddone:
            G_DoWorldDone();
            break;

        default:
            break;
        }

    // get commands, check consistency,
    // and build new consistency check
    memcpy(&viewplayer->cmd, &localcmds[game.time % BACKUPTICS], sizeof(ticcmd_t));

    // check for special buttons
    if(viewplayer->cmd.buttons & BT_SPECIAL)
    {
        switch(viewplayer->cmd.buttons & BT_SPECIALMASK)
        {
        case BTS_PAUSE:
            if((paused = !paused))
            {
                S_StopSounds();
                S_StartSound(NULL, sfx_swtchn);
                viewplayer->fixedcolormap = 0;
                I_SetPalette(PLAYPAL);
                I_UpdateBlitFunc(false);
                I_StopControllerRumble();

                if(windowfocused)
                    S_LowerMusicVolume();
            }
            else
            {
                S_ResumeMusic();
                S_StartSound(NULL, sfx_swtchx);
                I_SetPalette(&PLAYPAL[st_palette * 768]);

                if(windowfocused)
                    S_RestoreMusicVolume();

                if(reopenautomap)
                {
                    reopenautomap = false;
                    AM_Start(true);
                    viewactive = false;
                }
            }

            break;

        case BTS_SAVEGAME:
            game.action = ga_savegame;
            break;
        }

        viewplayer->cmd.buttons = 0;
    }

    // Have we just finished displaying an intermission screen?
    if(oldgamestate == GS_INTERMISSION && game.state != GS_INTERMISSION)
        WI_End();
    else if(oldgamestate == GS_LEVEL && game.state == GS_INTERMISSION)
        I_Sleep(500);

    oldgamestate = game.state;

    // do main actions
    switch(game.state)
    {
    case GS_LEVEL:
        P_Ticker();
        ST_Ticker();
        AM_Ticker();
        HU_Ticker();
        break;

    case GS_INTERMISSION:
        WI_Ticker();
        break;

    case GS_FINALE:
        F_Ticker();
        break;

    case GS_TITLESCREEN:
        D_PageTicker();
        break;

    default:
        break;
    }
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in p_mobj.c
//

//
// G_PlayerFinishLevel
// Called when the player completes a level.
//
static void G_PlayerFinishLevel(void)
{
    memset(viewplayer->powers, 0, sizeof(viewplayer->powers));
    memset(viewplayer->cards, 0, sizeof(viewplayer->cards));
    viewplayer->mo->flags &= ~MF_FUZZ; // cancel invisibility
    viewplayer->extralight    = 0;     // cancel gun flashes
    viewplayer->fixedcolormap = 0;     // cancel ir goggles
    viewplayer->damagecount   = 0;     // no palette changes
    viewplayer->bonuscount    = 0;
    st_palette                = 0; // [JN] Also no inner palette changes

    // [BH] switch to chainsaw if player has it and ends map with fists selected
    if(viewplayer->readyweapon == wp_fist && viewplayer->weaponowned[wp_chainsaw])
        viewplayer->readyweapon = wp_chainsaw;

    viewplayer->fistorchainsaw =
    (viewplayer->weaponowned[wp_chainsaw] ? wp_chainsaw : wp_fist);

    game.stats.totaltime += game.stats.maptime;
}

//
// G_PlayerReborn
// Called after the player dies
// almost everything is cleared and initialized
//
void G_PlayerReborn(void)
{
    int killcount   = viewplayer->killcount;
    int itemcount   = viewplayer->itemcount;
    int secretcount = viewplayer->secretcount;
    int deaths      = viewplayer->deaths;
    int suicides    = viewplayer->suicides;
    int cheats      = viewplayer->cheats;

    memset(viewplayer, 0, sizeof(*viewplayer));

    viewplayer->killcount   = killcount;
    viewplayer->itemcount   = itemcount;
    viewplayer->secretcount = secretcount;
    viewplayer->deaths      = deaths;
    viewplayer->suicides    = suicides;
    viewplayer->cheats      = cheats;

    // don't do anything immediately
    viewplayer->usedown    = true;
    viewplayer->attackdown = true;

    viewplayer->playerstate      = PST_LIVE;
    viewplayer->health           = initial_health;
    viewplayer->preferredshotgun = wp_shotgun;
    viewplayer->fistorchainsaw   = wp_fist;

    G_SetInitialWeapon();

    infight = false;
    shake   = 0;
}

//
// G_DoReborn
//
static void G_DoReborn(void)
{
    if(solonet)
        P_ResurrectPlayer(initial_health);
    else if(quicksaveslot >= 0 && autoload)
        game.action = ga_autoloadgame;
    else
    {
        game.action = ga_loadlevel;
        C_Input("restartmap");

        if(M_StringCompare(mapnum, "E1M4B") || M_StringCompare(mapnum, "E1M8B"))
            M_StringCopy(speciallumpname, mapnum, sizeof(speciallumpname));
    }
}

void G_ScreenShot(void)
{
    if(V_ScreenShot())
    {
        static char buffer[512];

        M_snprintf(buffer, sizeof(buffer), s_GSCREENSHOT, lbmname1);
        HU_SetPlayerMessage(buffer, false, false);
        message_dontfuckwithme = true;

        C_Output(BOLD("%s") " was saved.", lbmpath1);

        if(*lbmpath2)
            C_Output(BOLD("%s") " was also saved.", lbmpath2);
    }
    else
    {
        C_ShowConsole(false);
        C_Warning(0, "A screenshot couldn't be taken.");
    }
}

bool newpars = false;

// DOOM par times
int pars[10][10] = { { 0 }, { 0, 30, 75, 120, 90, 165, 180, 180, 165, 165 },
    { 0, 90, 90, 90, 120, 90, 360, 240, 135, 170 },
    { 0, 90, 45, 90, 150, 90, 90, 165, 105, 135 },

    // [BH] Episode 4, 5 and 6 par times
    { 0, 165, 255, 135, 150, 180, 390, 135, 360, 180 },
    { 0, 90, 150, 360, 420, 780, 420, 780, 300, 660 },
    { 0, 480, 300, 360, 240, 510, 840, 960, 390, 450 } };

// DOOM II par times
int cpars[100] = {
    30, 90, 120, 120, 90, 150, 120, 120, 270, 90,     // 01-10
    210, 150, 150, 150, 210, 150, 420, 150, 210, 150, // 11-20
    240, 150, 180, 150, 150, 300, 330, 420, 300, 180, // 21-30
    120, 30, 0                                        // 31-32
};

// [BH] No Rest For The Living par times
static const int npars[9] = { 75, 105, 120, 105, 210, 105, 165, 105, 135 };

// [BH] Legacy Of Rust par times
static const int lpars[] = { 30, 90, 120, 120, 90, 150, 120, 120, 270, 90, 210,
    150, 150, 150, 210, 150 };

//
// G_DoCompleted
//
bool secretexit;

void G_ExitLevel(void)
{
    secretexit = false;
    game.action = ga_completed;
}

void G_SecretExitLevel(void)
{
    secretexit = true;
    game.action = ga_completed;
}

int G_GetParTime(void)
{
    const int par = P_GetMapPar(game.episode, game.map);

    if(par)
        return par;
    else if(!newpars && !canmodify)
        return 0;
    else if(game.mode == commercial)
    {
        // [BH] have no par time for TNT and Plutonia
        if(game.mission == pack_nerve && game.map <= 9)
            return npars[game.map - 1];
        else if(game.mission == pack_tnt || game.mission == pack_plut)
            return 0;
        else
            return cpars[game.map - 1];
    }
    else if(game.episode <= 6 && game.map <= 9)
        return pars[game.episode][game.map];
    else
        return 0;
}

static void G_DoCompleted(void)
{
    const int nextmap       = P_GetMapNext(game.episode, game.map);
    const int secretnextmap = P_GetMapSecretNext(game.episode, game.map);

    P_LookForFriends();

    game.action = ga_nothing;

    I_UpdateBlitFunc(false);

    G_PlayerFinishLevel(); // take away cards and stuff

    G_ClearInput();

    if(automapactive)
        AM_Stop();

    if(game.mode != commercial)
        switch(game.map)
        {
        case 8:
            // [BH] this episode is complete, so select the next episode in the menu
            if((game.mode == registered && game.episode < 3) ||
            (game.mode == retail && game.episode < 4))
            {
                episode++;
                EpiDef.laston++;
                M_SaveCVARs();
            }

            break;

        case 9:
            viewplayer->didsecret = true;
            break;
        }

    wminfo.didsecret = viewplayer->didsecret;
    wminfo.epsd      = game.episode - 1;
    wminfo.last      = game.map - 1;

    if(game.mode == commercial)
    {
        if(secretexit && secretnextmap > 0)
            wminfo.next = secretnextmap - 1;
        else if(nextmap > 0)
            wminfo.next = nextmap - 1;
        else if(secretexit)
        {
            switch(game.map)
            {
            case 4:
                // [BH] exit to secret level in No Rest For The Living
                if(game.mission == pack_nerve)
                    wminfo.next = 8;

                break;

            case 15:
                wminfo.next = 30;
                break;

            case 31:
                wminfo.next = 31;
                break;
            }
        }
        else
        {
            switch(game.map)
            {
            case 9:
                // [BH] return to MAP05 after secret level in No Rest For The Living
                wminfo.next = (game.mission == pack_nerve ? 4 : game.map);
                break;

            case 31:
            case 32:
                wminfo.next = 15;
                break;

            default:
                wminfo.next = game.map;
                break;
            }
        }
    }
    else
    {
        if(secretexit && secretnextmap > 0)
            wminfo.next = secretnextmap - 1;
        else if(nextmap > 0)
            wminfo.next = nextmap - 1;
        else if(secretexit)
            wminfo.next = 8; // go to secret level
        else if(game.map == 9)
        {
            // returning from secret level
            switch(game.episode)
            {
            case 1:
            case 6:
                wminfo.next = 3;
                break;

            case 2:
                wminfo.next = 5;
                break;

            case 3:
            case 5:
                wminfo.next = 6;
                break;

            case 4:
                wminfo.next = 2;
                break;
            }
        }
        else
            wminfo.next = game.map; // go to next level
    }

    wminfo.maxkills  = game.stats.kills;
    wminfo.maxitems  = game.stats.items;
    wminfo.maxsecret = game.stats.secrets;
    wminfo.partime   = G_GetParTime() * TICRATE;
    wminfo.skills    = (game.stats.kills ? viewplayer->killcount : 1);
    wminfo.sitems    = (game.stats.items ? viewplayer->itemcount : 1);
    wminfo.ssecret   = viewplayer->secretcount;
    wminfo.stime     = game.stats.maptime;

    game.state     = GS_INTERMISSION;
    viewactive    = false;
    automapactive = false;

    stat_mapsfinished = SafeAdd(stat_mapsfinished, 1);
    M_SaveCVARs();

    if(!numconsolestrings ||
    (!M_StringCompare(console[numconsolestrings - 1].string, "exitmap")))
        C_Input("exitmap");

    WI_Start(&wminfo);
}

//
// G_WorldDone
//
void G_WorldDone(void)
{
    const char* intertext       = P_GetInterText(game.episode, game.map);
    const char* intersecrettext = P_GetInterSecretText(game.episode, game.map);

    game.action = ga_worlddone;

    if(secretexit)
        viewplayer->didsecret = true;

    if(*intertext || (*intersecrettext && secretexit) ||
    P_GetMapEndCast(game.episode, game.map) || P_GetMapEndGame(game.episode, game.map))
    {
        F_StartFinale();
        return;
    }

    if(game.mode == commercial)
    {
        if(game.mission == pack_nerve)
        {
            if(game.map == 8)
                F_StartFinale();
        }
        else
        {
            switch(game.map)
            {
            case 15:
            case 31:
                if(!secretexit)
                    break;

            case 6:
            case 11:
            case 20:
            case 30:
                F_StartFinale();
                break;
            }
        }
    }
    else if(game.map == 8)
        game.action = ga_victory;
}

static void G_DoWorldDone(void)
{
    game.state = GS_LEVEL;
    game.map   = wminfo.next + 1;
    G_DoLoadLevel();
    viewactive = true;

    if(quicksaveslot >= 0 && autosave)
        game.action = ga_autosavegame;
}

void G_LoadGame(const char* name)
{
    M_StringCopy(savename, name, sizeof(savename));
    game.action = ga_loadgame;
}

void G_DoLoadGame(void)
{
    int savedmaptime;

    I_SetPalette(PLAYPAL);

    loadaction = game.action;
    game.action = ga_nothing;

    if(numconsolestrings == 1 ||
    !M_StringStartsWith(console[numconsolestrings - 1].string, "load "))
        C_Input("load %s", savename);

    if(!(save_stream = FS_OpenFile(savename, FS_READ, FS_TRUE)))
    {
        menuactive = false;
        C_ShowConsole(false);
        C_Warning(0, BOLD("%s") " couldn't be loaded.", savename);
        loadaction = ga_nothing;
        return;
    }

    if(!P_ReadSaveGameHeader(savedescription))
    {
        FS_CloseFile(save_stream);
        loadaction = ga_nothing;
        return;
    }

    savedmaptime = game.stats.maptime;

    // load a base level
    G_InitNew(game.skill, game.episode, game.map);

    game.stats.maptime = savedmaptime;

    // unarchive all the modifications
    P_UnarchivePlayer();
    P_UnarchiveWorld();
    P_UnarchiveThinkers();
    P_UnarchiveSpecials();
    P_UnarchiveMap();

    P_RestoreTargets();

    P_MapEnd();

    if(musinfo.currentitem != -1)
        S_ChangeMusInfoMusic(musinfo.currentitem, true);

    if(!P_ReadSaveGameEOF())
        I_Error("%s is invalid.", savename);

    P_ReadSaveGameFooter();

    FS_CloseFile(save_stream);

    if(setsizeneeded)
        R_ExecuteSetViewSize();

    // draw the pattern into the back screen
    if(v_viewwidth != video.screen_width)
        R_FillBackScreen();

    st_facecount = 0;

    if(game.mode != commercial)
    {
        episode       = game.episode;
        EpiDef.laston = game.episode - 1;
    }

    skilllevel    = game.skill + 1;
    NewDef.laston = game.skill;

    viewplayer->gamesloaded++;
    stat_gamesloaded = SafeAdd(stat_gamesloaded, 1);
    M_SaveCVARs();

    if(consoleactive)
    {
        C_Output(BOLD("%s") " loaded.", savename);
        C_HideConsoleFast();
    }

    ammohighlight   = 0;
    armorhighlight  = 0;
    healthhighlight = 0;

    ammodiff[am_clip]  = 0;
    ammodiff[am_shell] = 0;
    ammodiff[am_misl]  = 0;
    ammodiff[am_cell]  = 0;
    armordiff          = 0;
    healthdiff         = 0;

    if(r_screensize == r_screensize_max && animatedstats)
        P_AnimateAllStatsFromStart();
}

void G_LoadedGameMessage(void)
{
    if(*savedescription)
    {
        static char buffer[1024];
        char* temp1 = titlecase(savedescription);

        if(loadaction == ga_autoloadgame)
        {
            M_snprintf(buffer, sizeof(buffer), s_GGAUTOLOADED, temp1);
            C_Output(buffer);
            HU_SetPlayerMessage(buffer, false, false);
        }
        else
        {
            fs_file_info status;
            struct tm* timestamp;
            int hour;

            M_snprintf(buffer, sizeof(buffer), s_GGLOADED, temp1);
            C_Output(buffer);
            HU_SetPlayerMessage(buffer, false, false);

            if (FS_GetInfo(&status, P_SaveGameFile(savegameslot), FS_TRUE) != FS_SUCCESS)
                I_Error("Save game file %s is corrupt.", P_SaveGameFile(savegameslot));

            // last modified time?
            time_t seconds = status.lastModifiedTime;
            timestamp = localtime(&seconds);
            hour      = timestamp->tm_hour;

            C_Output("It was previously saved at %i:%02i%s on %s, %s %i, %i.",
            (hour ? hour - 12 * (hour > 12) : 12), timestamp->tm_min,
            (hour < 12 ? "(AM)" : "(PM)"), daynames[timestamp->tm_wday],
            monthnames[timestamp->tm_mon], timestamp->tm_mday, 1900 + timestamp->tm_year);

            if(game.prevskill != sk_none && game.skill != game.prevskill)
            {
                char* temp2 = titlecase(*skilllevels[game.skill]);

                M_StringReplaceAll(temp2, ".", "", false);
                M_StringReplaceAll(temp2, "!", "", false);

                C_Warning(0, "The skill level is now " ITALICS("%s."), temp2);
                free(temp2);
            }
        }

        message_dontfuckwithme = true;
        free(temp1);
    }

    loadaction = ga_nothing;
}

//
// G_SaveGame
// Called by the menu task.
// Description is a 256 byte text string
//
void G_SaveGame(const int slot, const char* description, const char* name)
{
    M_StringCopy(savename, name, sizeof(savename));
    savegameslot = slot;
    M_StringCopy(savedescription, description, sizeof(savedescription));
    sendsave = true;

    if(r_diskicon)
    {
        drawdisk     = true;
        drawdisktics = DRAWDISKTICS;
    }
}

static void G_DoSaveGame(void)
{
    char* temp_savegame_file = P_TempSaveGameFile();
    char* savegame_file =
    (consoleactive || !*savedescription ? savename : P_SaveGameFile(savegameslot));

    // Open the savegame file for writing. We write to a temporary file
    // and then rename it at the end if it was successfully written.
    // This prevents an existing savegame from being overwritten by
    // a corrupted one, or if a savegame buffer overrun occurs.
    if(!(save_stream = FS_OpenFile(temp_savegame_file, FS_WRITE, FS_TRUE)))
    {
        menuactive = false;
        C_ShowConsole(false);
        C_Warning(0, BOLD("%s") " couldn't be saved.", savegame_file);
    }
    else
    {
        char* backup_savegame_file = M_StringJoin(savegame_file, ".bak", NULL);

        if(game.action == ga_autosavegame)
        {
            M_UpdateSaveGameName(quicksaveslot);
            M_StringCopy(savedescription, savegamestrings[quicksaveslot],
            sizeof(savedescription));
        }

        P_WriteSaveGameHeader(savedescription);

        P_ArchivePlayer();
        P_ArchiveWorld();
        P_ArchiveThinkers();
        P_ArchiveSpecials();
        P_ArchiveMap();

        P_WriteSaveGameEOF();

        P_WriteSaveGameFooter();

        // Finish up, close the savegame file.
        FS_CloseFile(save_stream);

        // Now rename the temporary savegame file to the actual savegame
        // file, backing up the old savegame if there was one there.
        remove(backup_savegame_file);
        rename(savegame_file, backup_savegame_file);
        rename(temp_savegame_file, savegame_file);

        free(backup_savegame_file);

        if(savegameslot >= 0)
            savegames = true;

        if(!numconsolestrings ||
        !M_StringStartsWith(console[numconsolestrings - 1].string, "save "))
            C_Input("save %s", savegame_file);

        if(!*savedescription)
            M_StringCopy(savedescription, maptitle, sizeof(savedescription));

        if(consoleactive)
            C_Output(BOLD("%s") " was saved.", savename);
        else
        {
            static char buffer[1024];
            char* temp = titlecase(savedescription);

            M_snprintf(buffer, sizeof(buffer),
            (game.action == ga_autosavegame ? s_GGAUTOSAVED : s_GGSAVED), temp);
            C_Output(buffer);
            HU_SetPlayerMessage(buffer, false, false);
            message_dontfuckwithme = true;
            free(temp);

            if(game.action != ga_autosavegame)
                S_StartSound(NULL, sfx_swtchx);
        }

        viewplayer->gamessaved++;
        stat_gamessaved = SafeAdd(stat_gamessaved, 1);
        M_SaveCVARs();

        // draw the pattern into the back screen
        if(v_viewwidth != video.screen_width)
            R_FillBackScreen();
    }

    game.action = ga_nothing;
}

static skill_t d_skill;
static int d_episode;
static int d_map;

void G_DeferredInitNew(skill_t skill, int ep, int map)
{
    d_skill    = skill;
    d_episode  = ep;
    d_map      = map;
    game.action = ga_newgame;
    infight    = false;
    game.stats.totaltime  = 0;

    if(skill == sk_baby)
        stat_skilllevel_imtooyoungtodie = SafeAdd(stat_skilllevel_imtooyoungtodie, 1);
    else if(skill == sk_easy)
        stat_skilllevel_heynottoorough = SafeAdd(stat_skilllevel_heynottoorough, 1);
    else if(skill == sk_medium)
        stat_skilllevel_hurtmeplenty = SafeAdd(stat_skilllevel_hurtmeplenty, 1);
    else if(skill == sk_hard)
        stat_skilllevel_ultraviolence = SafeAdd(stat_skilllevel_ultraviolence, 1);
    else
        stat_skilllevel_nightmare = SafeAdd(stat_skilllevel_nightmare, 1);

    M_SaveCVARs();
}

//
// G_DeferredLoadLevel
// [BH] Called when the IDCLEV cheat is used.
//
void G_DeferredLoadLevel(skill_t skill, int ep, int map)
{
    d_skill     = skill;
    d_episode   = ep;
    d_map       = map;
    game.action  = ga_loadlevel;
    infight     = false;
    sector_list = NULL;

    for(int i = 0; i < NUMPOWERS; i++)
        if(viewplayer->powers[i] > 0)
            viewplayer->powers[i] = 0;
}

static void G_DoNewGame(void)
{
    I_SetPalette(PLAYPAL);

    st_facecount = ST_STRAIGHTFACECOUNT;
    G_InitNew(d_skill, d_episode, d_map);
    game.action = ga_nothing;
    infight    = false;
}

// killough 04/10/98: New function to fix bug which caused DOOM
// lockups when idclev was used in conjunction with -fast.
void G_SetFastParms(bool fast_pending)
{
    static bool fast; // remembers fast state

    if(fast != fast_pending) // only change if necessary
    {
        for(int i = 0; i < nummobjtypes; i++)
            if(mobjinfo[i].altspeed != NO_ALTSPEED)
                SWAP(mobjinfo[i].speed, mobjinfo[i].altspeed);

        if((fast = fast_pending))
        {
            for(int i = 0; i < numstates; i++)
                if((states[i].flags & STATEF_SKILL5FAST) && states[i].tics != 1)
                    states[i].tics >>= 1; // don't change 1->0 since it causes cycles
        }
        else
        {
            for(int i = 0; i < numstates; i++)
                if(states[i].flags & STATEF_SKILL5FAST)
                    states[i].tics <<= 1;
        }
    }
}

void G_SetMovementSpeed(int scale)
{
    forwardmove[0] = FORWARDMOVE0 * scale / 100;
    forwardmove[1] = MIN(FORWARDMOVE1 * scale / 100, 127);
    sidemove[0]    = SIDEMOVE0 * scale / 100;
    sidemove[1]    = SIDEMOVE1 * scale / 100;
}

//
// G_InitNew
// Can be called by the startup code or the menu task.
//
void G_InitNew(skill_t skill, int ep, int map)
{
    if(paused)
    {
        paused = false;
        S_ResumeMusic();
    }

    if(skill > sk_nightmare)
        skill = sk_nightmare;

    if(ep < 1)
        ep = 1;

    if(!customepisodes)
    {
        if(game.mode == retail)
        {
            if(ep > 4)
                ep = 4;
        }
        else if(game.mode == shareware)
        {
            if(ep > 1)
                ep = 1; // only start episode 1 on shareware
        }
    }

    // [BH] Fix <https://doomwiki.org/wiki/Demon_speed_bug>.
    G_SetFastParms(fastparm || skill == sk_nightmare);

    // force player to be initialized upon first level load
    viewplayer->playerstate = PST_REBORN;

    paused        = false;
    automapactive = false;
    viewactive    = true;
    game.episode   = ep;
    game.map       = map;
    game.skill     = skill;

    if(numconsolestrings <= 1 ||
    (!M_StringCompare(console[numconsolestrings - 2].string, "newgame") &&
    !M_StringStartsWith(console[numconsolestrings - 2].string, "map ") &&
    !M_StringStartsWith(console[numconsolestrings - 1].string, "load ") &&
    !M_StringStartsWith(console[numconsolestrings - 1].string, "Warping ") && !autostart))
        C_Input("newgame");

    G_DoLoadLevel();
}
