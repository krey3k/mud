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

#if defined(_WIN32)
#include <windows.h>
#endif

#include "doom/doomtype.h"


// Screen width and height.
#define V_VANILLAWIDTH 320
#define V_VANILLAHEIGHT 200

#define V_ACTUALVANILLAHEIGHT (V_VANILLAHEIGHT * 6 / 5)
#define V_ACTUALHEIGHT (V_ACTUALVANILLAHEIGHT * 2)

#define V_VANILLASBARHEIGHT 32
#define V_SBARHEIGHT (V_VANILLASBARHEIGHT * 2)

#define V_WIDEVANILLAWIDTH (V_ACTUALVANILLAHEIGHT * 16 / 9)

#define V_NONWIDEWIDTH (V_VANILLAWIDTH * 2)
#define V_NONWIDEASPECTRATIO (4.0 / 3.0)

#define V_MAXWIDTH (V_NONWIDEWIDTH * 6)
#define V_MAXHEIGHT ((V_VANILLAHEIGHT + 1) * 2)
#define V_MAXSCREENAREA (V_MAXWIDTH * V_MAXHEIGHT)

#define V_MAXWIDEFOVDELTA 32

#define MAXMOUSEBUTTONS 8

#define V_GAMMALEVELS 21

// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick synchronous operations are performed here.
// Can call D_PostEvent.
void I_StartTic(void);

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode
void I_InitGraphics(void);
void I_RestartGraphics(const bool recreatewindow);
void I_CapFPS(const int cap);
void I_ShutdownGraphics(void);

void GetWindowPosition(void);
void GetWindowSize(void);
void GetScreenResolution(void);

// Takes full 8 bit values.
void I_SetPalette(const byte* playpal);

void I_UpdateBlitFunc(const bool shaking);

void I_ToggleFullscreen(const bool output);
void I_UpdateColors(void);
void I_SetMotionBlur(const int percent);

void I_SetGamma(const float value);

#if defined(_WIN32)
void I_WindowResizeBlit(void);
#endif

extern void (*blitfunc)(void);

extern bool nowidescreen;

extern int shakedamage;

extern int V_MAPWIDTH;
extern int V_MAPHEIGHT;
extern int V_MAPAREA;
extern int V_MAPBOTTOM;

extern int gammaindex;
extern const float gammalevels[V_GAMMALEVELS];
extern float brightness;

// Default display dimensions
#define V_DEFAULT_DISPLAY_WIDTH 1280
#define V_DEFAULT_DISPLAY_HEIGHT 720

// Video system state
typedef struct video_s
{
    // Display dimensions (render target size)
    int display_width;
    int display_height;

    // Screen dimensions and derived values
    int screen_width;
    int screen_height;
    int screen_area;

    // Widescreen parameters
    int widescreen_delta;
    int max_widescreen_delta;
    int wide_fov_delta;

    // Window position and size
    int window_x;
    int window_y;
    int window_width;
    int window_height;

    // Window border dimensions
    int window_border_width;
    int window_border_height;
} video_t;

extern video_t video;

typedef struct
{
    byte r;
    byte g;
    byte b;
    byte a;
} SDL_Color;
typedef struct
{
    int x, y;
    int w, h;
} SDL_Rect;

extern byte* v_mapscreen;

extern byte* PLAYPAL;
extern SDL_Color screencolors[256];

extern int framespersecond;
extern int refreshrate;

// Maximum supported scale factor (1-6)
// Scale 1 = 320x200 (vanilla DOOM), Scale 2 = 640x400 (default), etc.
#define R_MAX_SCALE 6

//
// render_state_t
// Runtime render state structure containing all resolution-dependent values.
// All values are recalculated by R_ResizeRenderState() when the scale changes.
//
// Dimension Hierarchy:
//   vanilla_*    -> Base DOOM resolution (320x200) multiplied by scale
//   actual_*     -> Aspect-ratio corrected (6/5 factor for non-square pixels)
//   nonwide_*    -> Pixel-doubled (2x) for rendering
//   max_*        -> Maximum possible dimensions (6x widescreen expansion)
//   screen_*     -> Current actual screen dimensions
//   view_*       -> 3D viewport within the screen (excludes HUD/status bar)
//
typedef struct render_state_s
{
    // Resolution scale factor (1-6)
    // Multiplier applied to vanilla DOOM dimensions (320x200)
    int scale;

    // Vanilla dimensions: 320*scale x 200*scale
    // Base resolution before any pixel doubling or aspect correction
    int vanilla_width;
    int vanilla_height;

    // Actual dimensions (aspect-ratio corrected for non-square pixels)
    // actual_vanilla_height = vanilla_height * 6 / 5
    // actual_height = actual_vanilla_height * 2 (pixel-doubled)
    int actual_vanilla_height;
    int actual_height;

    // Status bar dimensions: 32*scale (vanilla), 64*scale (doubled)
    int vanilla_sbar_height;
    int sbar_height;

    // Widescreen vanilla width for 16:9 aspect ratio
    int wide_vanilla_width;

    // Non-widescreen (4:3) pixel-doubled dimensions
    int nonwide_width;       // vanilla_width * 2
    float nonwide_aspect_ratio;  // 4.0/3.0

    // Maximum dimensions for buffer allocation (supports all widescreens)
    // max_width = nonwide_width * 6 (supports ultra-wide monitors)
    int max_width;
    int max_height;
    int max_screen_area;
    int max_wide_fov_delta;

    // Look direction (freelook) limits
    // lookdir_max = half vanilla height * scale
    // lookdirs = total look positions (up + down + center)
    int lookdir_max;
    int lookdirs;

    // Current screen dimensions (may vary with widescreen settings)
    int screen_width;
    int screen_height;
    int screen_area;

    // Widescreen adjustment parameters
    int widescreen_delta;      // Pixels added for widescreen
    int max_widescreen_delta;  // Maximum widescreen expansion
    int wide_fov_delta;        // FOV adjustment for widescreen

    // 3D viewport within the screen (excludes status bar when visible)
    int view_width;
    int view_height;
    int view_window_x;
    int view_window_y;
} render_state_t;

extern render_state_t render;

//
// R_ResizeRenderState
// Initialize or resize render state to a given scale factor (1-6).
// Handles all buffer allocations and recalculations for the rendering system.
// Returns true if successful, false if the scale is invalid.
//
bool R_ResizeRenderState(int new_scale);

//
// R_UpdateScreenDimensions
// Update render state widescreen-related members based on current window dimensions.
// Called automatically by R_ResizeRenderState, can also be called manually after
// window resize or widescreen mode changes.
//
void R_UpdateScreenDimensions(void);

//
// I_RefreshRenderState
// Lightweight refresh of rendering state after r_scale changes.
// Does not recreate window or restart graphics subsystem.
// Use this for runtime scale changes when I_RestartGraphics is too heavy.
//
void I_RefreshRenderState(void);

// Allocated buffer sizes (set by R_ResizeRenderState)
// These track the current allocation sizes to avoid unnecessary reallocations
extern int r_alloc_max_width;       // Width-dependent buffer allocation size
extern int r_alloc_max_height;      // Height-dependent buffer allocation size
extern int r_alloc_max_screen_area; // Area-dependent buffer allocation size
extern int r_alloc_lookdirs;        // Look direction table allocation size
