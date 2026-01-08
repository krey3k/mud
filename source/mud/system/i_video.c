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

#if defined(X11)
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#endif

#include <math.h>

#include "automap/am_map.h"
#include "console/c_cmds.h"
#include "console/c_console.h"
#include "doom/d_deh.h"
#include "doom/d_main.h"
#include "doom/doomstat.h"
#include "game/g_game.h"
#include "hud/hu_stuff.h"
#include "math/math_colors.h"
#include "system/i_input.h"
#include "system/i_system.h"
#include "system/i_timer.h"
#include "doom/d_cheat.h"
#include "system/i_config.h"
#include "menu/m_menu.h"
#include "utils/m_misc.h"
#include "math/math_random.h"
#include "sound/s_sound.h"
#include "hud/st_stuff.h"
#include "render/r_main.h"
#include "render/v_draw.h"
#include "render/v_video.h"
#include "system/i_version.h"
#include "wad/w_wad.h"

#if defined(_WIN32)
void I_InitWindows32(void);
#endif

video_t video = {
    .display_width         = V_DEFAULT_DISPLAY_WIDTH,
    .display_height        = V_DEFAULT_DISPLAY_HEIGHT,
    .screen_width          = 0,
    .screen_height         = V_VANILLAHEIGHT * 2,
    .screen_area           = 0,
    .widescreen_delta      = 0,
    .max_widescreen_delta  = 0,
    .wide_fov_delta        = 0,
    .window_x              = 0,
    .window_y              = 0,
    .window_width          = 0,
    .window_height         = 0,
    .window_border_width   = 0,
    .window_border_height  = 0,
};

static int V_WIDESCREENWIDTH;

// Global render state
render_state_t render = {0};

// Allocated buffer sizes - these track how much memory is currently allocated
int r_alloc_max_width = 0;
int r_alloc_max_height = 0;
int r_alloc_max_screen_area = 0;
int r_alloc_lookdirs = 0;

// Forward declarations for buffer reallocation functions
void R_ResizePlaneBuffers(void);
void R_ResizeThingsBuffers(void);
void R_ResizeMainBuffers(void);
void R_ResizeDrawBuffers(void);
void R_ResizeClipSegs(void);

//
// R_ResizeRenderState
// Initialize or resize render state to a new scale factor (1-6).
// Handles all buffer allocations and recalculations.
// Returns false if new_scale is invalid.
//
// Scale factor determines resolution multiplier:
//   scale=1: 320x200 base (vanilla DOOM resolution)
//   scale=2: 640x400 base (default, 2x vanilla)
//   scale=6: 1920x1200 base (maximum supported)
//
bool R_ResizeRenderState(int new_scale)
{
    // Validate scale is within supported range
    if(new_scale < 1 || new_scale > R_MAX_SCALE)
        return false;

    // Early out if scale hasn't changed
    if(new_scale == render.scale)
        return true;

    // Calculate new dimensions for this scale
    // Width: vanilla_width * 2 (for pixel doubling) * 6 (for max widescreen)
    const int new_max_width       = (V_VANILLAWIDTH * new_scale) * 2 * 6;
    const int new_max_height      = ((V_VANILLAHEIGHT * new_scale) + 1) * 2;
    const int new_max_screen_area = new_max_width * new_max_height;
    // lookdirs: half vanilla height * scale * 2 (up/down) + 1 (center)
    const int new_lookdirs        = (V_VANILLAHEIGHT / 2 * new_scale) * 2 + 1;

    // Check if we need to reallocate buffers
    const bool need_width_realloc  = (new_max_width > r_alloc_max_width);
    const bool need_height_realloc = (new_max_height > r_alloc_max_height);
    const bool need_area_realloc   = (new_max_screen_area > r_alloc_max_screen_area);
    const bool need_lookdirs_realloc = (new_lookdirs > r_alloc_lookdirs);

    // Update render state values
    render.scale = new_scale;

    // Vanilla dimensions (base resolution before pixel doubling)
    render.vanilla_width  = V_VANILLAWIDTH * new_scale;
    render.vanilla_height = V_VANILLAHEIGHT * new_scale;

    // Actual dimensions (aspect-ratio corrected: 6/5 for non-square pixels)
    render.actual_vanilla_height = render.vanilla_height * 6 / 5;
    render.actual_height         = render.actual_vanilla_height * 2;

    // Status bar dimensions
    render.vanilla_sbar_height = V_VANILLASBARHEIGHT * new_scale;
    render.sbar_height         = render.vanilla_sbar_height * 2;

    // Widescreen vanilla width (16:9 aspect)
    render.wide_vanilla_width = render.actual_vanilla_height * 16 / 9;

    // Non-widescreen dimensions (pixel-doubled)
    render.nonwide_width        = render.vanilla_width * 2;
    render.nonwide_aspect_ratio = 4.0f / 3.0f;

    // Maximum dimensions (derived from nonwide_width)
    render.max_width          = new_max_width;
    render.max_height         = new_max_height;
    render.max_screen_area    = new_max_screen_area;
    render.max_wide_fov_delta = V_MAXWIDEFOVDELTA;

    // Look direction limits (half vanilla height scaled)
    render.lookdir_max = (V_VANILLAHEIGHT / 2) * new_scale;
    render.lookdirs    = new_lookdirs;

    // Default screen dimensions
    render.screen_width  = render.nonwide_width;
    render.screen_height = render.vanilla_height * 2;
    render.screen_area   = render.screen_width * render.screen_height;

    // Widescreen parameters (will be updated later)
    render.widescreen_delta     = 0;
    render.max_widescreen_delta = 0;
    render.wide_fov_delta       = 0;

    // View window defaults
    render.view_width    = 0;
    render.view_height   = 0;
    render.view_window_x = 0;
    render.view_window_y = 0;

    // Update allocation tracking
    if(need_width_realloc)
        r_alloc_max_width = new_max_width;
    if(need_height_realloc)
        r_alloc_max_height = new_max_height;
    if(need_area_realloc)
        r_alloc_max_screen_area = new_max_screen_area;
    if(need_lookdirs_realloc)
        r_alloc_lookdirs = new_lookdirs;

    // Reallocate buffers as needed
    if(need_width_realloc || need_height_realloc || need_lookdirs_realloc)
    {
        R_ResizePlaneBuffers();
        R_ResizeThingsBuffers();
        R_ResizeMainBuffers();
    }

    if(need_width_realloc)
    {
        R_ResizeClipSegs();
    }

    if(need_height_realloc || need_area_realloc)
    {
        R_ResizeDrawBuffers();
    }

    if(need_area_realloc)
    {
        V_ResizeRenderScreens();
    }

    // Update screen dimensions using current window state
    R_UpdateScreenDimensions();

    return true;
}

static int R_WIDESCREENWIDTH;

// Forward declaration - defined after dest_rect
void R_UpdateScreenDimensions(void);

bool nowidescreen = false;

int V_MAPWIDTH;
int V_MAPHEIGHT = V_VANILLAHEIGHT * 2;
int V_MAPAREA;
int V_MAPBOTTOM;

static bool manuallypositioning;

SDL_Color screencolors[256];
byte* PLAYPAL;

byte* v_mapscreen;

static bool nearestlinear;
static int v_upscaledwidth;
static int v_upscaledheight;

int r_upscaledwidth;
int r_upscaledheight;

static bool software;

static int displayindex;
static int numdisplays;
static SDL_Rect displays[vid_display_max];

// Fullscreen width and height
static int screenwidth;
static int screenheight;

// Window width and height (now in video struct)

static int displaywidth;
static int displayheight;

static byte gammatable[V_GAMMALEVELS][256];

const float gammalevels[V_GAMMALEVELS] = {
    // Darker
    0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f,

    // No gamma correction
    1.0f,

    // Lighter
    1.10f, 1.20f, 1.30f, 1.40f, 1.50f, 1.60f, 1.70f, 1.80f, 1.90f, 2.0f
};

int gammaindex;

SDL_Rect src_rect;
SDL_Rect dest_rect;
static SDL_Rect map_rect;

int framespersecond = 0;
int refreshrate;

//
// R_UpdateScreenDimensions
// Updates render state widescreen-related members based on current dest_rect.
// Called by R_ResizeRenderState and I_GetScreenDimensions.
//
void R_UpdateScreenDimensions(void)
{
    // Guard against uninitialized dest_rect (during startup)
    if(dest_rect.w == 0 || dest_rect.h == 0)
    {
        render.screen_width         = render.nonwide_width;
        render.wide_fov_delta       = 0;
        render.widescreen_delta     = 0;
        render.max_widescreen_delta = 53;
        render.screen_area          = render.screen_width * render.screen_height;
        return;
    }

    if(vid_widescreen)
    {
        render.screen_width = BETWEEN(render.nonwide_width,
            ((dest_rect.w * render.actual_height / dest_rect.h + 1) & ~3), render.max_width);
        R_WIDESCREENWIDTH = render.screen_width;

        // r_fov * 0.82 is vertical FOV for 4:3 aspect ratio
        render.wide_fov_delta = MIN(
            (int)(atan(dest_rect.w / (dest_rect.h / tan(r_fov * 0.82 * M_PI / 360.0))) * 360.0 / M_PI) -
            r_fov - 2,
            render.max_wide_fov_delta);
        render.widescreen_delta     = render.screen_width / 4 - render.vanilla_width / 2;
        render.max_widescreen_delta = MAX(53, render.widescreen_delta);
    }
    else
    {
        render.screen_width         = render.nonwide_width;
        R_WIDESCREENWIDTH           = render.nonwide_width;
        render.wide_fov_delta       = 0;
        render.widescreen_delta     = 0;
        render.max_widescreen_delta = 53;
    }

    render.screen_area = render.screen_width * render.screen_height;
}

//
// I_RefreshRenderState
// Lightweight refresh of rendering state after r_scale changes.
// Does not recreate window or restart graphics subsystem.
//
void I_RefreshRenderState(void)
{
    // Recalculate upscaled texture dimensions
    int width, height;

    if(vid_fullscreen)
    {
        width  = displays[displayindex].w;
        height = displays[displayindex].h;
    }
    else
    {
        width  = video.window_width;
        height = video.window_height;
    }

    if(width * render.actual_height < height * render.screen_width)
        height = width * render.actual_height / render.screen_width;
    else
        width = height * render.screen_width / render.actual_height;

    r_upscaledwidth  = (width + render.screen_width - 1) / render.screen_width;
    r_upscaledheight = (height + render.screen_height - 1) / render.screen_height;

    // Trigger view size recalculation
    setsizeneeded = true;

    // Skip player sprite interpolation for one frame
    if(r_playersprites)
        skippsprinterp = true;
}

// Window border dimensions (now in video struct)

static void GetUpscaledTextureSize(int width, int height)
{
    if(width * V_ACTUALHEIGHT < height * video.screen_width)
        height = width * V_ACTUALHEIGHT / video.screen_width;
    else
        width = height * video.screen_width / V_ACTUALHEIGHT;

    v_upscaledwidth  = (width + video.screen_width - 1) / video.screen_width;
    v_upscaledheight = (height + video.screen_height - 1) / video.screen_height;

    if(width * render.actual_height < height * render.screen_width)
        height = width * render.actual_height / render.screen_width;
    else
        width = height * render.screen_width / render.actual_height;

    r_upscaledwidth  = (width + render.screen_width - 1) / render.screen_width;
    r_upscaledheight = (height + render.screen_height - 1) / render.screen_height;
}

void (*blitfunc)(void);

void I_CapFPS(const int cap)
{
    const uint64_t targettime = 1000000 / cap;
    static uint64_t startingtime;

    while(true)
    {
        const uint64_t currenttime = I_GetTimeUS();
        const uint64_t elapsedtime = currenttime - startingtime;

        if(elapsedtime >= targettime)
        {
            startingtime = currenttime;
            break;
        }
        else
        {
            const uint64_t remainingtime = targettime - elapsedtime;

            if(remainingtime > 1000)
                I_Sleep(((int)remainingtime - 1000) / 1000);
        }
    }
}

#ifdef MUD_SOKOL_PORT
#if defined(_WIN32)
void I_WindowResizeBlit(void)
{
    if(vid_showfps)
        CalculateFPS();

    SDL_LockTexture(texture, &src_rect, &buffer->pixels, &buffer->pitch);
    SDL_LowerBlit(surface, &src_rect, buffer, &src_rect);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);

    if(nearestlinear)
    {
        SDL_SetRenderTarget(renderer, texture_upscaled);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, texture_upscaled, NULL, NULL);
    }
    else
        SDL_RenderCopy(renderer, texture, NULL, NULL);

    SDL_RenderPresent(renderer);
}
#endif
#endif

static void I_Blit(void)
{
#ifdef MUD_SOKOL_PORT
    UpdateGrab();

    SDL_LockTexture(texture, &src_rect, &buffer->pixels, &buffer->pitch);
    SDL_LowerBlit(surface, &src_rect, buffer, &src_rect);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
    SDL_RenderPresent(renderer);
#endif
}

static void I_Blit_NearestLinear(void)
{
#ifdef MUD_SOKOL_PORT
    UpdateGrab();

    SDL_LockTexture(texture, &src_rect, &buffer->pixels, &buffer->pitch);
    SDL_LowerBlit(surface, &src_rect, buffer, &src_rect);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, texture_upscaled);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, texture_upscaled, NULL, &dest_rect);
    SDL_RenderPresent(renderer);
#endif
}

static void I_Blit_ShowFPS(void)
{
#ifdef MUD_SOKOL_PORT
    UpdateGrab();
    CalculateFPS();

    SDL_LockTexture(texture, &src_rect, &buffer->pixels, &buffer->pitch);
    SDL_LowerBlit(surface, &src_rect, buffer, &src_rect);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
    SDL_RenderPresent(renderer);
#endif
}

static void I_Blit_NearestLinear_ShowFPS(void)
{
#ifdef MUD_SOKOL_PORT
    UpdateGrab();
    CalculateFPS();

    SDL_LockTexture(texture, &src_rect, &buffer->pixels, &buffer->pitch);
    SDL_LowerBlit(surface, &src_rect, buffer, &src_rect);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, texture_upscaled);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, texture_upscaled, NULL, &dest_rect);
    SDL_RenderPresent(renderer);
#endif
}

static void I_Blit_Shake(void)
{
#ifdef MUD_SOKOL_PORT
    const int x = dest_rect.x;
    const int y = dest_rect.y;

    UpdateGrab();

    SDL_LockTexture(texture, &src_rect, &buffer->pixels, &buffer->pitch);
    SDL_LowerBlit(surface, &src_rect, buffer, &src_rect);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);

    dest_rect.x += M_BigRandomInt(-2, 2);
    dest_rect.y += M_BigRandomInt(-2, 2);

    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);

    dest_rect.x = x;
    dest_rect.y = y;

    SDL_RenderPresent(renderer);
#endif
}

static void I_Blit_NearestLinear_Shake(void)
{
#ifdef MUD_SOKOL_PORT
    const int x = dest_rect.x;
    const int y = dest_rect.y;

    UpdateGrab();

    SDL_LockTexture(texture, &src_rect, &buffer->pixels, &buffer->pitch);
    SDL_LowerBlit(surface, &src_rect, buffer, &src_rect);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, texture_upscaled);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_SetRenderTarget(renderer, NULL);

    dest_rect.x += M_BigRandomInt(-2, 2);
    dest_rect.y += M_BigRandomInt(-2, 2);

    SDL_RenderCopy(renderer, texture_upscaled, NULL, &dest_rect);

    dest_rect.x = x;
    dest_rect.y = y;

    SDL_RenderPresent(renderer);
#endif
}

static void I_Blit_ShowFPS_Shake(void)
{
#ifdef MUD_SOKOL_PORT
    const int x = dest_rect.x;
    const int y = dest_rect.y;

    UpdateGrab();
    CalculateFPS();

    SDL_LockTexture(texture, &src_rect, &buffer->pixels, &buffer->pitch);
    SDL_LowerBlit(surface, &src_rect, buffer, &src_rect);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);

    dest_rect.x += M_BigRandomInt(-2, 2);
    dest_rect.y += M_BigRandomInt(-2, 2);

    SDL_RenderCopy(renderer, texture_upscaled, NULL, &dest_rect);

    dest_rect.x = x;
    dest_rect.y = y;

    SDL_RenderPresent(renderer);
#endif
}

static void I_Blit_NearestLinear_ShowFPS_Shake(void)
{
#ifdef MUD_SOKOL_PORT
    const int x = dest_rect.x;
    const int y = dest_rect.y;

    UpdateGrab();
    CalculateFPS();

    SDL_LockTexture(texture, &src_rect, &buffer->pixels, &buffer->pitch);
    SDL_LowerBlit(surface, &src_rect, buffer, &src_rect);
    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, texture_upscaled);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_SetRenderTarget(renderer, NULL);

    dest_rect.x += M_BigRandomInt(-2, 2);
    dest_rect.y += M_BigRandomInt(-2, 2);

    SDL_RenderCopy(renderer, texture_upscaled, NULL, &dest_rect);

    dest_rect.x = x;
    dest_rect.y = y;

    SDL_RenderPresent(renderer);
#endif
}

void I_UpdateBlitFunc(const bool shaking)
{
    if(nearestlinear && (displayheight % V_VANILLAHEIGHT))
    {
        if(shaking && !software)
            blitfunc = (vid_showfps ? &I_Blit_NearestLinear_ShowFPS_Shake :
                                      &I_Blit_NearestLinear_Shake);
        else
            blitfunc = (vid_showfps ? &I_Blit_NearestLinear_ShowFPS : &I_Blit_NearestLinear);
    }
    else
    {
        if(shaking && !software)
            blitfunc = (vid_showfps ? &I_Blit_ShowFPS_Shake : &I_Blit_Shake);
        else
            blitfunc = (vid_showfps ? &I_Blit_ShowFPS : &I_Blit);
    }
}

static byte* gammalevel;
static float red;
static float green;
static float blue;
static float saturation;
static float contrast;
float brightness;

void I_UpdateColors(void)
{
    gammalevel = gammatable[gammaindex];
    red        = 255.0f * vid_red / 100.0f;
    green      = 255.0f * vid_green / 100.0f;
    blue       = 255.0f * vid_blue / 100.0f;
    saturation = (vid_saturation + 100.0f) / 100.0f;
    contrast = (259.0f * (vid_contrast + 255.0f)) / (255.0f * (259.0f - vid_contrast));
    brightness = (vid_brightness + 110.0f) / 110.0f;

    I_SetPalette(&PLAYPAL[st_palette * 768]);

#ifdef MUD_SOKOL_PORT
    if(!vid_pillarboxes)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
#endif
}

//
// I_SetPalette
//
void I_SetPalette(const byte* playpal)
{
    for(int i = 0; i < 256; i++)
    {
        // gamma correction and red/green/blue intensity
        byte r = BETWEEN(0, (int)(gammalevel[playpal[0]] + red), 255);
        byte g = BETWEEN(0, (int)(gammalevel[playpal[1]] + green), 255);
        byte b = BETWEEN(0, (int)(gammalevel[playpal[2]] + blue), 255);

        // saturation
        const float p = sqrtf(r * r * 0.299f + g * g * 0.587f + b * b * 0.114f);

        r = BETWEEN(0, (int)(p + (r - p) * saturation), 255);
        g = BETWEEN(0, (int)(p + (g - p) * saturation), 255);
        b = BETWEEN(0, (int)(p + (b - p) * saturation), 255);

        // contrast and brightness
        // Zero out the bottom two bits of each channel - the PC VGA
        // controller only supports 6 bits of accuracy.
        screencolors[i].r =
        (BETWEEN(0, (int)((128 + (r - 128) * contrast) * brightness), 255) & ~3);
        screencolors[i].g =
        (BETWEEN(0, (int)((128 + (g - 128) * contrast) * brightness), 255) & ~3);
        screencolors[i].b =
        (BETWEEN(0, (int)((128 + (b - 128) * contrast) * brightness), 255) & ~3);
        screencolors[i].a = 0xFF;

        playpal += 3;
    }

#ifdef MUD_SOKOL_PORT
    SDL_SetPaletteColors(palette, colors, 0, 256);

    if(vid_pillarboxes)
        SDL_SetRenderDrawColor(renderer, colors[BLACK].r, colors[BLACK].g,
        colors[BLACK].b, SDL_ALPHA_OPAQUE);
#endif
}

static void GetDisplays(void)
{
    numdisplays    = 1;
    displays[0].w  = video.display_width;
    displays[0].h  = video.display_height;

    if((double)displays[0].w / displays[0].h <= V_NONWIDEASPECTRATIO)
    {
        nowidescreen   = true;
        vid_widescreen = false;
    }
}

void GetWindowPosition(void)
{
    int x = 0;
    int y = 0;

    if(M_StringCompare(vid_windowpos, vid_windowpos_centered) ||
    M_StringCompare(vid_windowpos, vid_windowpos_centred))
    {
        video.window_x = 0;
        video.window_y = 0;
    }
    else if(sscanf(vid_windowpos, "(%10i,%10i)", &x, &y) != 2)
    {
        video.window_x = 0;
        video.window_y = 0;
        vid_windowpos  = vid_windowpos_centered;
        M_SaveCVARs();
    }
    else
    {
        video.window_x = BETWEEN(displays[displayindex].x, x,
        displays[displayindex].x + displays[displayindex].w - 50);
        video.window_y = BETWEEN(displays[displayindex].y, y,
        displays[displayindex].y + displays[displayindex].h - 50);
    }
}

void GetWindowSize(void)
{
    char width[11]  = "";
    char height[11] = "";

    if(sscanf(vid_windowsize, "%10[^x]x%10[^x]", width, height) == 2)
    {
        char* temp1 = uncommify(width);
        char* temp2 = uncommify(height);

        video.window_width  = strtol(temp1, NULL, 10);
        video.window_height = strtol(temp2, NULL, 10);
        free(temp1);
        free(temp2);
    }
    else
    {
        video.window_height = video.screen_height + video.window_border_height;
        video.window_width  = video.screen_height * 16 / 10 + video.window_border_width;
        vid_windowsize      = vid_windowsize_default;
        M_SaveCVARs();
    }
}

static bool ValidScreenMode(const int width, const int height)
{
    return true;
}

void GetScreenResolution(void)
{
    if(M_StringCompare(vid_screenresolution, vid_screenresolution_desktop))
    {
        screenwidth  = 0;
        screenheight = 0;
    }
    else
    {
        int width;
        int height;

        if(sscanf(vid_screenresolution, "%10ix%10i", &width, &height) != 2 ||
        !ValidScreenMode(width, height))
        {
            screenwidth          = 0;
            screenheight         = 0;
            vid_screenresolution = vid_screenresolution_desktop;
            M_SaveCVARs();
        }
        else
        {
            screenwidth  = width;
            screenheight = height;
        }
    }
}

static char* getaspectratio(int width, int height)
{
    const int hcf = gcd(width, height);
    static char ratio[10];

    width /= hcf;
    height /= hcf;

    if(width == 8)
    {
        width = 16;
        height *= 2;
    }

    M_snprintf(ratio, sizeof(ratio), "%i:%i", width, height);
    return ratio;
}

static void PositionOnCurrentDisplay(void)
{
#ifdef MUD_SOKOL_PORT
    manuallypositioning = true;

    if(video.window_x || video.window_y)
        SDL_SetWindowPosition(window, video.window_x, video.window_y);
    else
        SDL_SetWindowPosition(window,
        displays[displayindex].x + (displays[displayindex].w - video.window_width) / 2,
        displays[displayindex].y + (displays[displayindex].h - video.window_height) / 2);
#endif
}

void I_SetMotionBlur(const int percent)
{
#ifdef MUD_SOKOL_PORT
    if(percent)
    {
        SDL_SetSurfaceAlphaMod(surface, SDL_ALPHA_OPAQUE - 128 * percent / 100);
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    }
    else
    {
        SDL_SetSurfaceAlphaMod(surface, SDL_ALPHA_OPAQUE);
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
    }
#endif
}

static void SetVideoMode(const bool createwindow, const bool output)
{
#ifdef MUD_SOKOL_PORT
    int rendererflags = SDL_RENDERER_TARGETTEXTURE;
    int windowflags   = (SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
#endif
    int width, height;
#ifdef MUD_SOKOL_PORT
    SDL_RendererInfo rendererinfo;
    const char* displayname = SDL_GetDisplayName((displayindex = vid_display - 1));
#endif
    bool instead = false;

    if(displayindex >= numdisplays)
    {
        if(output)
            C_Warning(1, "Display %i wasn't found.", vid_display);

#ifdef MUD_SOKOL_PORT
        displayname = SDL_GetDisplayName((displayindex = vid_display_default - 1));
#endif
        instead = true;
    }

#ifdef MUD_SOKOL_PORT
    if(output)
    {
        if(numdisplays == 1)
        {
            if(displayname)
                C_Output("\"%s\" is being used%s.", displayname,
                (instead ? " instead" : ""));
        }
        else
        {
            if(displayname)
                C_Output("\"%s\" (display %i of %i) is being used%s.", displayname,
                displayindex + 1, numdisplays, (instead ? " instead" : ""));
            else
                C_Output("Display %i of %i is being used%s.", displayindex + 1,
                numdisplays, (instead ? " instead" : ""));
        }
    }
#endif

    if(nowidescreen && output)
    {
        consolecmds[C_GetIndex(stringize(vid_widescreen))].flags |= CF_READONLY;
        C_Warning(1,
        "The aspect ratio of display %i is too low to show " ITALICS(DOOMRETRO_NAME) " in widescreen.",
        displayindex + 1);
    }

#ifdef MUD_SOKOL_PORT
    if(vid_vsync)
        rendererflags |= SDL_RENDERER_PRESENTVSYNC;
#endif

    if(M_StringCompare(vid_scalefilter, vid_scalefilter_nearest_linear))
        nearestlinear = true;
    else
    {
        nearestlinear = false;

        if(!M_StringCompare(vid_scalefilter, vid_scalefilter_linear) &&
        !M_StringCompare(vid_scalefilter, vid_scalefilter_nearest))
        {
            vid_scalefilter = vid_scalefilter_default;
            M_SaveCVARs();
        }

#ifdef MUD_SOKOL_PORT
        SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid_scalefilter, SDL_HINT_OVERRIDE);
#endif
    }

#ifdef MUD_SOKOL_PORT
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, vid_scaleapi, SDL_HINT_OVERRIDE);
#endif

    software = M_StringCompare(vid_scaleapi, vid_scaleapi_software);

    GetWindowPosition();
    GetWindowSize();
    GetScreenResolution();

    if(vid_fullscreen)
    {
        if(!screenwidth && !screenheight)
        {
            width  = displays[displayindex].w;
            height = displays[displayindex].h;

            if(!width || !height)
                I_Error("Graphics couldn't be %s.",
                (english == english_american ? "initialized" : "initialised"));

            if(output)
            {
                char* temp1 = commify(width);
                char* temp2 = commify(height);

                C_Output("The native desktop resolution of %sx%s with an "
                         "aspect ratio of %s is being used.",
                temp1, temp2, getaspectratio(width, height));

                free(temp1);
                free(temp2);
            }
        }
        else
        {
            width  = screenwidth;
            height = screenheight;

            if(output)
            {
                char* temp1 = commify(width);
                char* temp2 = commify(height);

                C_Output("A resolution of %sx%s with an aspect ratio of %s is "
                         "being used.",
                temp1, temp2, getaspectratio(width, height));

                free(temp1);
                free(temp2);
            }
        }
    }
    else
    {
        if(video.window_height > displays[displayindex].h)
        {
            video.window_height = displays[displayindex].h - video.window_border_height;
            video.window_width  = video.window_height * 4 / 3;
            M_SaveCVARs();
        }

        width  = video.window_width;
        height = video.window_height;

        if(!video.window_x && !video.window_y)
        {
            if(output)
            {
                char* temp1 = commify(width);
                char* temp2 = commify(height);

                C_Output("A %sx%s resizable window is %s on the screen.", temp1, temp2,
                (english == english_american ? vid_windowpos_centered : vid_windowpos_centred));

                free(temp1);
                free(temp2);
            }
        }
        else
        {
            if(output)
            {
                char* temp1 = commify(width);
                char* temp2 = commify(height);

                C_Output("A %sx%s resizable window is at (%i, %i).", temp1,
                temp2, video.window_x, video.window_y);

                free(temp1);
                free(temp2);
            }
        }
    }

    GetUpscaledTextureSize(width, height);

    displaywidth  = video.display_width;
    displayheight = video.display_height;

    if(output)
    {
        char* temp1 = commify(render.screen_width);
        char* temp2 = commify(render.screen_height);
        char* temp3 = commify(width);
        char* temp4 = commify(height);

        C_Output("A software renderer is used to render every frame.");

        if(nearestlinear)
        {
            char* temp5 = commify((int64_t)r_upscaledwidth * render.screen_width);
            char* temp6 = commify((int64_t)r_upscaledheight * render.screen_height);

            C_Output("Every frame is scaled up from %sx%s to %sx%s using "
                     "nearest-%s interpolation "
                     "and then back down to %sx%s using linear filtering.",
            temp1, temp2, temp5, temp6,
            (english == english_american ? "neighbor" : "neighbour"), temp3, temp4);

            free(temp5);
            free(temp6);
        }
        else if(M_StringCompare(vid_scalefilter, vid_scalefilter_linear) && !software)
            C_Output("Every frame is scaled up from %sx%s to %sx%s using "
                     "linear filtering.",
            temp1, temp2, temp3, temp4);
        else
            C_Output("Every frame is scaled up from %sx%s to %sx%s using "
                     "nearest-%s interpolation.",
            temp1, temp2, temp3, temp4,
            (english == english_american ? "neighbor" : "neighbour"));

        free(temp1);
        free(temp2);
        free(temp3);
        free(temp4);
    }

#ifdef MUD_SOKOL_PORT
    if(!SDL_GetRendererInfo(renderer, &rendererinfo))
    {
        if(M_StringCompare(rendererinfo.name, vid_scaleapi_opengl))
        {
            int major;
            int minor;

            SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);

            if(major * 10 + minor < 21)
            {
                C_Warning(1, ITALICS(DOOMRETRO_NAME) " requires at least " ITALICS("OpenGL v2.1."));

#if defined(_WIN32)
                vid_scaleapi = vid_scaleapi_direct3d;
                M_SaveCVARs();

                SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,
                vid_scaleapi_direct3d, SDL_HINT_OVERRIDE);

                if(output)
                    C_Output("This scaling is now done using hardware "
                             "acceleration with " ITALICS("Direct3D 11."));
#endif
            }
            else
            {
                if(output)
                    C_Output("This scaling is done using hardware "
                             "acceleration with " ITALICS("OpenGL v%i.%i."),
                    major, minor);

                if(!M_StringCompare(vid_scaleapi, vid_scaleapi_opengl))
                {
                    vid_scaleapi = vid_scaleapi_opengl;
                    M_SaveCVARs();
                }
            }
        }
#if defined(_WIN32)
        else if(M_StringCompare(rendererinfo.name, vid_scaleapi_direct3d))
        {
            if(output)
                C_Output("This scaling is done using hardware acceleration "
                         "with " ITALICS("Direct3D 9."));

            if(!M_StringCompare(vid_scaleapi, vid_scaleapi_direct3d))
            {
                vid_scaleapi = vid_scaleapi_direct3d;
                M_SaveCVARs();
            }
        }
#else
        else if(M_StringCompare(rendererinfo.name, vid_scaleapi_opengles))
        {
            if(output)
                C_Output("This scaling is done using hardware acceleration "
                         "with " ITALICS("OpenGL ES."));
        }
        else if(M_StringCompare(rendererinfo.name, vid_scaleapi_opengles2))
        {
            if(output)
                C_Output("This scaling is done using hardware acceleration "
                         "with " ITALICS("OpenGL ES 2."));
        }
#endif
        else if(M_StringCompare(rendererinfo.name, vid_scaleapi_software))
        {
            software      = true;
            nearestlinear = false;

            SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY,
            vid_scalefilter_nearest, SDL_HINT_OVERRIDE);

            if(output)
                C_Output("This scaling is also done in software.");

            if(!M_StringCompare(vid_scaleapi, vid_scaleapi_software))
            {
                vid_scaleapi = vid_scaleapi_software;
                M_SaveCVARs();
            }

            if(output &&
            (M_StringCompare(vid_scalefilter, vid_scalefilter_linear) ||
            M_StringCompare(vid_scalefilter, vid_scalefilter_nearest_linear)))
                C_Warning(1, "Linear filtering can't be used in software.");
        }

        refreshrate = 0;

        if(rendererinfo.flags & SDL_RENDERER_PRESENTVSYNC)
        {
            SDL_DisplayMode displaymode;

            if(!SDL_GetWindowDisplayMode(window, &displaymode))
            {
                refreshrate = displaymode.refresh_rate;

#if !defined(__APPLE__)
                if(vid_vsync == vid_vsync_adaptive && M_StringStartsWith(vid_scaleapi, "opengl"))
                    if(SDL_GL_SetSwapInterval(-1) < 0)
                        C_Warning(1, "Adaptive vsync is not supported.");
#endif

                if(refreshrate < vid_capfps || !vid_capfps)
                {
                    if(output)
                        C_Output("The framerate is synced with the display's "
                                 "refresh rate of %iHz.",
                        refreshrate);
                }
                else
                {
                    if(output)
                    {
                        char* temp = commify(vid_capfps);

                        C_Output("The framerate is capped at %s FPS.", temp);
                        free(temp);
                    }
                }
            }
        }
        else
        {
            if(output)
            {
                if(vid_vsync)
                {
                    if(M_StringCompare(rendererinfo.name, vid_scaleapi_software))
                        C_Warning(1,
                        "The framerate can't be synced with the "
                        "display's refresh rate in software.");
                    else
                        C_Warning(1,
                        "The framerate can't be synced with the "
                        "display's refresh rate using this graphics card.");
                }

                if(vid_capfps)
                {
                    char* temp = commify(vid_capfps);

                    C_Output("The framerate is capped at %s FPS.", temp);
                    free(temp);
                }
                else
                    C_Output("The framerate is uncapped.");
            }
        }
    }
#endif

    if(output)
    {
        wadfile_t* playpalwad = lumpinfo[W_CheckNumForName("PLAYPAL")]->wadfile;

        if(D_IsResourceWAD(playpalwad->path))
            C_Output(
            "The 256-%s palette from the " BOLD(
            "PLAYPAL") " lump in the IWAD " BOLD("%s") " is being used.",
            (english == english_american ? "color" : "colour"),
            lumpinfo[W_GetLastNumForName("PLAYPAL")]->wadfile->path);
        else
            C_Output("The 256-%s palette from the " BOLD(
                     "PLAYPAL") " lump in the %s " BOLD("%s") " is being used.",
            (english == english_american ? "color" : "colour"),
            (playpalwad->type == IWAD ? "IWAD" : "PWAD"), playpalwad->path);

        if(gammaindex == 10)
            C_Output("There is no gamma correction.");
        else
        {
            char text[128];
            int len;

            M_snprintf(text, sizeof(text), "The gamma correction level is %.2f.", r_gamma);
            len = (int)strlen(text);

            if(text[len - 2] == '0' && text[len - 3] == '0')
            {
                text[len - 2] = '.';
                text[len - 1] = '\0';
            }

            C_Output(text);
        }
    }

    I_UpdateColors();

    src_rect.w = video.screen_width;
    src_rect.h = video.screen_height;
}

static void I_GetScreenDimensions(void)
{
    int width;
    int height;

    if(vid_fullscreen)
    {
        width  = displays[displayindex].w;
        height = displays[displayindex].h;
    }
    else
    {
        GetWindowSize();

        width  = video.window_width;
        height = video.window_height;
    }

    if(vid_widescreen)
    {
        dest_rect.w = width;

        if(vid_aspectratio == vid_aspectratio_auto)
        {
            dest_rect.h = height;
            dest_rect.x = 0;
            dest_rect.y = 0;
        }
        else
        {
            if(vid_aspectratio == vid_aspectratio_16_9)
            {
                if((dest_rect.h = width * 9 / 16) > height)
                {
                    dest_rect.w = height * 16 / 9;
                    dest_rect.h = height;
                }
            }
            else if(vid_aspectratio == vid_aspectratio_16_10)
            {
                if((dest_rect.h = width * 10 / 16) > height)
                {
                    dest_rect.w = height * 16 / 10;
                    dest_rect.h = height;
                }
            }
            else if(vid_aspectratio == vid_aspectratio_21_9)
            {
                if((dest_rect.h = width * 9 / 21) > height)
                {
                    dest_rect.w = height * 21 / 9;
                    dest_rect.h = height;
                }
            }
            else if(vid_aspectratio == vid_aspectratio_32_9)
            {
                if((dest_rect.h = width * 9 / 32) > height)
                {
                    dest_rect.w = height * 32 / 9;
                    dest_rect.h = height;
                }
            }

            dest_rect.x = (width - dest_rect.w) / 2;
            dest_rect.y = (height - dest_rect.h) / 2;
        }

        video.screen_width         = BETWEEN(V_NONWIDEWIDTH,
            ((dest_rect.w * V_ACTUALHEIGHT / dest_rect.h + 1) & ~3), V_MAXWIDTH);
        V_WIDESCREENWIDTH          = video.screen_width;

        // r_fov * 0.82 is vertical FOV for 4:3 aspect ratio
        video.wide_fov_delta = MIN(
        (int)(atan(dest_rect.w / (dest_rect.h / tan(r_fov * 0.82 * M_PI / 360.0))) * 360.0 / M_PI) -
        r_fov - 2,
        V_MAXWIDEFOVDELTA);
        video.widescreen_delta     = video.screen_width / 4 - V_VANILLAWIDTH / 2;
        video.max_widescreen_delta = MAX(53, video.widescreen_delta);

        R_UpdateScreenDimensions();
    }
    else
    {
        dest_rect.w = V_NONWIDEWIDTH;
        dest_rect.h = V_ACTUALHEIGHT;
        dest_rect.x = 0;
        dest_rect.y = 0;

        video.screen_width         = V_NONWIDEWIDTH;
        V_WIDESCREENWIDTH          = BETWEEN(V_NONWIDEWIDTH,
           ((width * V_ACTUALHEIGHT / height + 1) & ~3), V_MAXWIDTH);
        video.wide_fov_delta       = 0;
        video.widescreen_delta     = 0;
        video.max_widescreen_delta = 53;

        R_UpdateScreenDimensions();
    }

    video.screen_area = video.screen_width * video.screen_height;

    GetPixelSize();
}

void I_RestartGraphics(const bool recreatewindow)
{
#ifdef MUD_SOKOL_PORT
    if(recreatewindow)
        SDL_DestroyWindow(window);
#endif

    I_GetScreenDimensions();

    SetVideoMode(recreatewindow, false);

    AM_SetAutomapSize(r_screensize);

    v_mapscreen = *v_screens;

    M_SetWindowCaption();

    C_ResetWrappedLines();

    setsizeneeded = true;

    if(r_playersprites)
        skippsprinterp = true;
}

void I_ToggleFullscreen(const bool output)
{
#ifdef MUD_SOKOL_PORT
    if(SDL_SetWindowFullscreen(window,
       (vid_fullscreen ? 0 : (vid_borderlesswindow ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN))) <
    0)
    {
        menuactive = false;
        C_ShowConsole(false);
        C_Warning(0, "Unable to switch to %s.", (vid_fullscreen ? "a window" : "fullscreen"));
        return;
    }

    vid_fullscreen = !vid_fullscreen;
    I_RestartGraphics(vid_fullscreen && !vid_borderlesswindow);
    S_StartSound(NULL, sfx_stnmov);
    M_SaveCVARs();

    if(nearestlinear)
        I_UpdateBlitFunc(viewplayer && viewplayer->damagecount);

    if(vid_fullscreen)
    {
        if(output)
            C_StringCVAROutput(stringize(vid_fullscreen), "on");
    }
    else
    {
        if(output)
            C_StringCVAROutput(stringize(vid_fullscreen), "off");

        SDL_SetWindowSize(window, video.window_width, video.window_height);

        displaywidth  = video.window_width;
        displayheight = video.window_height;

        PositionOnCurrentDisplay();
    }
#endif
}

static void I_InitPaletteTables(void)
{
    for(int i = 0; i < V_GAMMALEVELS; i++)
        for(int j = 0; j < 256; j++)
            gammatable[i][j] =
            (byte)(powf(j / 255.0f, 1.0f / gammalevels[i]) * 255.0f + 0.5f);
}

void I_SetGamma(const float value)
{
    for(gammaindex = 0;
    gammaindex < V_GAMMALEVELS && gammalevels[gammaindex] != value; gammaindex++)
        ;

    if(gammaindex == V_GAMMALEVELS)
        for(gammaindex = 0; gammalevels[gammaindex] != r_gamma_default; gammaindex++)
            ;
}

void I_ShutdownGraphics(void)
{
#ifdef MUD_SOKOL_PORT
    SDL_FreePalette(palette);
    SDL_FreeSurface(surface);
    SDL_FreeSurface(buffer);
    SDL_DestroyTexture(texture);
    SDL_DestroyTexture(texture_upscaled);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
#endif
}

void I_InitGraphics(void)
{
#ifdef MUD_SOKOL_PORT
    performancecounter   = SDL_GetPerformanceCounter();
    performancefrequency = SDL_GetPerformanceFrequency();
#endif

    PLAYPAL = W_CacheLumpName("PLAYPAL");
    I_InitTintTables(PLAYPAL);
    I_InitColors(PLAYPAL);

    I_InitPaletteTables();
    I_SetGamma(r_gamma);

#if !defined(_WIN32) && defined(MUD_SOKOL_PORT)
    if(*vid_driver)
        SDL_setenv("SDL_VIDEODRIVER", vid_driver, true);
#endif

    GetDisplays();

#if defined(_DEBUG)
    vid_fullscreen = false;
#endif

    I_GetScreenDimensions();

#ifdef MUD_SOKOL_PORT
#if defined(_WIN32)
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1", SDL_HINT_OVERRIDE);
#endif

    SDL_SetHintWithPriority(SDL_HINT_RENDER_BATCHING, "0", SDL_HINT_OVERRIDE);

    if(vid_fullscreen)
        SDL_ShowCursor(false);
#endif

    SetVideoMode(true, true);

    if(vid_fullscreen)
        SetShowCursor(false);

#ifdef MUD_SOKOL_PORT
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
#endif

#if defined(_WIN32)
    I_InitWindows32();
#endif

#ifdef MUD_SOKOL_PORT
    SDL_SetWindowTitle(window, DOOMRETRO_NAME);
#endif

    I_UpdateBlitFunc(false);
    memset(v_screens[0], nearestblack, video.screen_area);
    blitfunc();

    I_StopTextInput();

    I_Sleep(1000);
}
