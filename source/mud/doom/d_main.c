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
#include "console/c_cmds.h"
#include "console/c_console.h"
#include "doom/d_deh.h"
#include "doom/d_loop.h"
#include "doom/d_main.h"
#include "doom/doomstat.h"
#include "intermission/f_finale.h"
#include "render/r_wipe.h"
#include "game/g_game.h"
#include "hud/hu_stuff.h"
#include "math/math_colors.h"
#include "system/i_input.h"
#include "math/math_swap.h"
#include "system/i_system.h"
#include "system/i_timer.h"
#include "system/i_config.h"
#include "menu/m_menu.h"
#include "utils/m_misc.h"
#include "render/r_main.h"
#include "sound/s_sound.h"
#include "hud/st_stuff.h"
#include "render/v_draw.h"
#include "render/v_video.h"
#include "wad/w_wad.h"
#include "intermission/wi_stuff.h"

#define FADECOUNT 8
#define FADETICS 25

bool advancetitle;
bool dowipe = false;
static bool forcewipe;

static byte fadescreen[V_MAXSCREENAREA];
int fadecount = 0;
bool realframe;

//
// TITLE LOOP
//
int titlesequence = 0;
int pagetic       = 3 * TICRATE;

static patch_t* pagelump;
patch_t* creditlump;
patch_t* titlelump;

//
// D_PostEvent
//
void D_PostEvent(event_t* ev)
{
    if(dowipe || !windowfocused)
        return;

    if(M_Responder(ev))
        return; // menu ate the event

    if(C_Responder(ev))
        return; // console ate the event

    G_Responder(ev);
}

//
// D_FadeScreen
//
void D_FadeScreen(bool screenshot)
{
    if((!fade && !screenshot) || togglingvanilla || fadecount)
        return;

    memcpy(fadescreen, v_screens[0], video.screen_area);
    fadecount = FADECOUNT;
}

//
// D_UpdateFade
//
static void D_UpdateFade(void)
{
    static byte* tinttab;
    static uint64_t fadewait;
    const uint64_t tics = I_GetTimeMS();

    if(fadewait < tics)
    {
        byte* tinttabs[FADECOUNT + 1] = { tinttab90, tinttab80, tinttab70,
            tinttab60, tinttab50, tinttab40, tinttab30, tinttab20, tinttab10 };

        fadewait = tics + FADETICS;
        tinttab  = tinttabs[fadecount--];
    }

    for(int i = 0; i < video.screen_area; i++)
    {
        byte* dot = *v_screens + i;

        *dot = tinttab[(*dot << 8) + fadescreen[i]];
    }
}

//
// D_FadeScreenToBlack
//
void D_FadeScreenToBlack(void)
{
    byte* palette = &PLAYPAL[(menuactive ? 0 : st_palette * 768)];

    if(!fade)
        return;

    for(brightness = 0.95f; brightness >= 0.0f; brightness -= 0.05f)
    {
        I_SetPalette(palette);
        I_SetMusicVolume((int)((float)s_musicvolume * brightness));
        blitfunc();
        I_CapFPS(60);
    }

    memset(v_screens[0], nearestblack, video.screen_area);
    blitfunc();
}

//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_TITLESCREEN;

void D_Display(void)
{
    static bool pausedstate;
    static gamestate_t oldgamestate = GS_NONE;
    static int saved_gametime       = -1;
    uint64_t nowtime;
    uint64_t wipestart;
    bool done;

    // TODO: FIXME
    memset(v_screens[0], 255, video.screen_area);    

    if(vid_capfps != TICRATE && (realframe = (game.time > saved_gametime)))
        saved_gametime = game.time;

    // change the view size if needed
    if(setsizeneeded)
    {
        R_ExecuteSetViewSize();
        oldgamestate = GS_NONE; // force background redraw
    }

    if(drawdisk)
        HU_DrawDisk();

    // save the current screen if about to wipe
    if((dowipe = (game.state != wipegamestate || forcewipe)))
    {
        fadecount = 0;

        if(melt)
            Wipe_StartScreen();
        else
            D_FadeScreen(false);

        if(forcewipe)
            forcewipe = false;
        else
        {
            menuactive = false;
            R_ExecuteSetViewSize();
        }
    }

    if(game.state != GS_LEVEL)
    {
        if(game.state != oldgamestate)
            I_SetPalette(PLAYPAL);

        switch(game.state)
        {
        case GS_INTERMISSION:
            WI_Drawer();
            break;

        case GS_FINALE:
            F_Drawer();
            break;

        case GS_TITLESCREEN:
            D_PageDrawer();
            break;

        default:
            break;
        }
    }
    else
    {
        HU_Erase();

        // draw the view directly
        R_RenderPlayerView();

        if(automapactive)
            AM_Drawer();

        if(!menuactive)
        {
            ST_Drawer((v_viewheight == video.screen_height), true);

            // see if the border needs to be initially drawn
            if(oldgamestate != GS_LEVEL && v_viewwidth != video.screen_width)
                R_FillBackScreen();

            // see if the border needs to be updated to the screen
            if(!automapactive)
            {
                if(v_viewwidth != video.screen_width)
                    R_DrawViewBorder();

                if(r_detail == r_detail_low)
                    postprocessfunc(v_screens[0], video.screen_width, v_viewwindowx,
                    v_viewwindowy * video.screen_width, v_viewwindowx + v_viewwidth,
                    (v_viewwindowy + v_viewheight) * video.screen_width, lowpixelwidth, lowpixelheight);
            }

            HU_Drawer();
        }
    }

    oldgamestate = wipegamestate = game.state;

    // draw pause pic
    if((pausedstate = paused))
    {
        M_DrawMenuBackground();

        if(M_PAUSE)
        {
            patch_t* patch = W_CacheLumpName("M_PAUSE");

            V_DrawMenuPatch((V_VANILLAWIDTH - SHORT(patch->width)) / 2,
            (V_VANILLAHEIGHT - SHORT(patch->height)) / 2, patch, false, video.screen_width);
        }
        else
            M_DrawCenteredString((V_VANILLAHEIGHT - 16) / 2, s_M_PAUSED);
    }

    if(loadaction != ga_nothing)
        G_LoadedGameMessage();

    if (vid_showfps)
    {
        double frame_time = sapp_frame_duration();  // seconds per frame
        framespersecond = (int) (frame_time > 0.0) ? (1.0 / frame_time) : 0.0;    
    }        

    if(!dowipe || !melt)
    {
        if(!paused && !menuactive)
        {
            if(vid_showfps && !dowipe && framespersecond)
                C_UpdateFPSOverlay();

            if(game.state == GS_LEVEL)
            {
                if(timer)
                    C_UpdateTimerOverlay();

                if(viewplayer->cheats & CF_MYPOS)
                    C_UpdatePlayerPositionOverlay();

                if((pathoverlay = (am_path && automapactive)))
                    C_UpdatePathOverlay();

                if(am_playerstats && (automapactive))
                    C_UpdatePlayerStatsOverlay();
            }
        }

        if(consoleheight)
            C_Drawer();

        // menus go directly to the screen
        M_Drawer();

        if(drawdisk)
            HU_DrawDisk();

        if(fadecount)
            D_UpdateFade();

        // normal update
        blitfunc();

        if((!vid_capfps || vid_capfps > 60 || (vid_vsync && refreshrate > 60)) &&
        (game.state != GS_LEVEL || menuactive || consoleactive || paused))
            I_CapFPS(60);
        else if(vid_capfps >= TICRATE && !vid_vsync)
            I_CapFPS(vid_capfps);

        return;
    }

    // wipe update
    Wipe_EndScreen();
    wipestart = I_GetTime() - 1;

    do
    {
        int64_t tics;

        do
        {
            nowtime = I_GetTime();
            tics    = nowtime - wipestart;
            I_Sleep(1);
        } while(tics <= 0);

        wipestart = nowtime;
        done      = Wipe_ScreenWipe();

        blitfunc();

    } while(!done);
}

//
// D_DoomTick
//
void D_DoomTick(void)
{
    I_InputProcessEventQueue();

    TryRunTics(); // will run at least one tic

    D_Display(); // update display, next frame, with current state
}

//
// D_PageTicker
//
void D_PageTicker(void)
{
    static uint64_t pagewait;
    uint64_t pagetime;

    if(menuactive || consoleactive || !windowfocused)
        return;

    if(pagewait < (pagetime = I_GetTime()))
    {
        pagetic--;
        pagewait = pagetime;
    }

    if(pagetic < 0)
    {
        advancetitle = true;
    }
}

//
// D_PageDrawer
//
void D_PageDrawer(void)
{
    V_DrawPagePatch(0, pagelump);
}

//
// This cycles through the title sequence.
//
void D_DoAdvanceTitle(void)
{
    viewplayer->playerstate = PST_LIVE; // not reborn
    advancetitle            = false;
    paused                  = false;
    game.action              = ga_nothing;
    game.state               = GS_TITLESCREEN;

    if(titlesequence == 1)
    {
        static bool flag = true;

        if(flag)
        {
            flag = false;
            I_InitKeyboard();

            if(alwaysrun)
                C_StringCVAROutput(stringize(alwaysrun), "on");
        }

        if(pagelump == creditlump)
            forcewipe = true;

        pagelump = titlelump;
        pagetic  = PAGETICS;

        M_SetWindowCaption();
        S_StartMusic(game.mode == commercial ? mus_dm2ttl : mus_intro);

        if(devparm)
            C_ShowConsole(false);
    }
    else if(titlesequence == 2)
    {
        forcewipe = true;
        pagelump  = creditlump;
        pagetic   = PAGETICS;
    }

    if(++titlesequence > 2)
        titlesequence = 1;
}

//
// D_StartTitle
//
void D_StartTitle(int page)
{
    game.action    = ga_nothing;
    titlesequence = page;

    advancetitle = true;
}

//
// D_InitTitleScreen
// Loads resources for the title screen
//
void D_InitTitleScreen(void)
{
    titlelump = W_CacheLumpName("TITLEPIC");
    creditlump = W_CacheLumpName("CREDIT");
}