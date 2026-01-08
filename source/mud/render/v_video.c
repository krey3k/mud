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
#include "doom/d_iwad.h"
#include "doom/d_main.h"
#include "doom/doomstat.h"
#include "hud/hu_lib.h"
#include "math/math_colors.h"
#include "math/math_swap.h"
#include "utils/m_array.h"
#include "system/i_config.h"
#include "menu/m_menu.h"
#include "utils/m_misc.h"
#include "playsim/p_setup.h"
#include "render/r_draw.h"
#include "render/v_video.h"
#include "system/i_version.h"
#include "wad/w_wad.h"
#include "utils/z_zone.h"

byte* v_screens[V_NUMSCREENS];
byte* r_screens[R_NUMSCREENS];

int lowpixelwidth;
int lowpixelheight;

void (*postprocessfunc)(byte*, int, int, int, int, int, int, int);

byte* colortranslation[10];
byte cr_gold[256];
byte cr_none[256];

typedef struct
{
    const char* name;
    byte** lump;
} colortranslation_t;

static colortranslation_t colortranslations[] = { { "CRRED", &colortranslation[0] },
    { "CRGRAY", &colortranslation[1] }, { "CRGREEN", &colortranslation[2] },
    { "CRBLUE", &colortranslation[3] }, { "CRYELLOW", &colortranslation[4] },
    { "CRBLACK", &colortranslation[5] }, { "CRPURPLE", &colortranslation[6] },
    { "CRWHITE", &colortranslation[7] }, { "CRORANGE", &colortranslation[8] },
    { "", NULL } };

void V_InitColorTranslation(void)
{
    for(colortranslation_t* p = colortranslations; *p->name; p++)
        *p->lump = W_CacheLumpName(p->name);

    for(int i = 0; i < 256; i++)
    {
        cr_gold[i] = I_GoldTranslation(PLAYPAL, (byte)i);
        cr_none[i] = i;
    }
}

// Track allocated size for render screens to know when reallocation is needed
static int r_screens_allocated_area = 0;

//
// V_Init
//
void V_Init(void)
{
    byte* base = Z_Malloc(V_MAXSCREENAREA * V_NUMSCREENS, PU_STATIC, NULL);
    byte* rbase = Z_Malloc(render.max_screen_area * R_NUMSCREENS, PU_STATIC, NULL);

    for(int i = 0; i < V_NUMSCREENS; i++)
        v_screens[i] = &base[i * V_MAXSCREENAREA];

    for(int i = 0; i < R_NUMSCREENS; i++)
        r_screens[i] = &rbase[i * render.max_screen_area];

    r_screens_allocated_area = render.max_screen_area;
    v_mapscreen = *v_screens;
}

//
// V_ResizeRenderScreens
// Reallocates render screen buffers if render.max_screen_area has increased
//
void V_ResizeRenderScreens(void)
{
    // Only reallocate if we need more space
    if(render.max_screen_area <= r_screens_allocated_area)
        return;

    // Free old render screens
    if(r_screens[0])
        Z_Free(r_screens[0]);

    // Allocate new render screens
    byte* rbase = Z_Malloc(render.max_screen_area * R_NUMSCREENS, PU_STATIC, NULL);

    for(int i = 0; i < R_NUMSCREENS; i++)
        r_screens[i] = &rbase[i * render.max_screen_area];

    r_screens_allocated_area = render.max_screen_area;
}


char lbmname1[MAX_PATH];
char lbmpath1[MAX_PATH];
static char lbmname2[MAX_PATH];
char lbmpath2[MAX_PATH] = "";

#ifdef MUD_SOKOL_PORT
static bool V_SavePNG(SDL_Window* sdlwindow, const char* path)
{

    bool result = false;
    int width   = 0;
    int height  = 0;

    SDL_GetWindowSize(sdlwindow, &width, &height);

    if(width > 0 && height > 0)
    {
        SDL_Surface* screenshot = SDL_CreateRGBSurface(
        0, (vid_widescreen ? width : height * 4 / 3), height, 32, 0, 0, 0, 0);

        if(screenshot)
        {
            if(!SDL_RenderReadPixels(SDL_GetRenderer(sdlwindow), NULL, 0,
               screenshot->pixels, screenshot->pitch))
                result = !IMG_SavePNG(screenshot, path);

            SDL_FreeSurface(screenshot);
        }
    }

    return result;
}
#endif

bool V_ScreenShot(void)
{
    bool result;
    char mapname[128];
    char* temp1;
    char* temp2;
    int count = 0;

    if(consoleactive)
        M_StringCopy(mapname, "Console", sizeof(mapname));
    else if(helpscreen)
        M_StringCopy(mapname, "Help", sizeof(mapname));
    else if(menuactive)
        M_StringCopy(mapname, "Menu", sizeof(mapname));
    else if(automapactive)
        M_StringCopy(mapname, "Automap", sizeof(mapname));
    else if(paused)
        M_StringCopy(mapname, "Paused", sizeof(mapname));
    else
        switch(game.state)
        {
        case GS_INTERMISSION:
            M_StringCopy(mapname, "Intermission", sizeof(mapname));
            break;

        case GS_FINALE:
            M_StringCopy(mapname, "Finale", sizeof(mapname));
            break;

        case GS_TITLESCREEN:
            M_StringCopy(mapname, (titlesequence == 1 ? "Credits" : "Title"),
            sizeof(mapname));
            break;

        default:
            temp2 = titlecase(maptitle);
            M_StringCopy(mapname, temp2, sizeof(mapname));
            free(temp2);
            break;
        }

    if(M_StringStartsWith(mapname, "The "))
    {
        temp2 = M_SubString(mapname, 4, strlen(mapname) - 4);
        M_snprintf(mapname, sizeof(mapname), "%s, The", temp2);
        free(temp2);
    }
    else if(M_StringStartsWith(mapname, "A "))
    {
        temp2 = M_SubString(mapname, 2, strlen(mapname) - 2);
        M_snprintf(mapname, sizeof(mapname), "%s, A", temp2);
        free(temp2);
    }

    temp1 = makevalidfilename(mapname);

    do
    {
        if(!count)
            M_snprintf(lbmname1, sizeof(lbmname1), "%s.png", temp1);
        else
        {
            temp2 = commify(count);
            M_snprintf(lbmname1, sizeof(lbmname1), "%s (%s).png", temp1, temp2);
            free(temp2);
        }

        count++;
        M_snprintf(lbmpath1, sizeof(lbmpath1), "%s%s", screenshotfolder, lbmname1);
    } while(M_FileExists(lbmpath1));

    free(temp1);

#ifdef MUD_SOKOL_PORT
    return V_SavePNG(window, lbmpath1);
#else
    return false;
#endif
}
