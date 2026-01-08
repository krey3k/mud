#pragma once

#include "render/r_data.h"
#include "render/v_video.h"

void V_FillRect(int screen,
int x,
int y,
int width,
int height,
int color1,
int color2,
bool left,
bool right,
const byte* tinttab1,
const byte* tinttab2);
void V_FillTransRect(int screen,
int x,
int y,
int width,
int height,
int color1,
int color2,
bool left,
bool right,
const byte* tinttab1,
const byte* tinttab2);
void V_FillSoftTransRect(int screen,
int x,
int y,
int width,
int height,
int color1,
int color2,
bool left,
bool right,
const byte* tinttab1,
const byte* tinttab2);

void V_DrawPatch(int x, int y, int screen, patch_t* patch);
void V_DrawWidePatch(int x, int y, int screen, patch_t* patch);
void V_DrawBigPatch(int x, int y, short width, short height, patch_t* patch);
void V_DrawMenuBorderPatch(int x, int y, patch_t* patch);
void V_DrawConsoleHeaderPatch(int x, int y, patch_t* patch, const int maxwidth, const int color1, const int color2);
void V_DrawConsoleSelectedTextPatch(const int x,
const int y,
const patch_t* patch,
const int width,
const int color,
const int backgroundcolor,
const bool italics,
const byte* tinttab);
void V_DrawConsoleTextPatch(const int x,
const int y,
const patch_t* patch,
const int width,
const int color,
const int backgroundcolor,
const bool italics,
const byte* tinttab);
void V_DrawOverlayTextPatch(byte* screen,
int screenwidth,
int x,
int y,
patch_t* patch,
int width,
int color,
int shadowcolor,
const byte* tinttab);
void V_DrawShadowPatch(int x, int y, patch_t* patch);
void V_DrawSolidShadowPatch(int x, int y, patch_t* patch);
void V_DrawSpectreShadowPatch(int x, int y, patch_t* patch);
bool V_IsEmptyPatch(patch_t* patch);
void V_DrawMenuPatch(int x, int y, patch_t* patch, bool highlight, int shadowwidth);
void V_DrawBigFontPatch(int x, int y, patch_t* patch, bool highlight, int shadowwidth);
void V_DrawHelpPatch(patch_t* patch);
void V_DrawFlippedPatch(int x, int y, patch_t* patch);
void V_DrawFlippedShadowPatch(int x, int y, patch_t* patch);
void V_DrawFlippedSolidShadowPatch(int x, int y, patch_t* patch);
void V_DrawFlippedSpectreShadowPatch(int x, int y, patch_t* patch);
void V_DrawFuzzPatch(int x, int y, patch_t* patch);
void V_DrawFlippedFuzzPatch(int x, int y, patch_t* patch);
void V_DrawNoGreenPatchWithShadow(int x, int y, patch_t* patch);
void V_DrawHUDPatch(int x, int y, patch_t* patch, const byte* tinttab);
void V_DrawHighlightedHUDNumberPatch(int x, int y, patch_t* patch, const byte* tinttab);
void V_DrawTranslucentHUDPatch(int x, int y, patch_t* patch, const byte* tinttab);
void V_DrawTranslucentHUDNumberPatch(int x, int y, patch_t* patch, const byte* tinttab);
void V_DrawTranslucentHighlightedHUDNumberPatch(int x, int y, patch_t* patch, const byte* tinttab);
void V_DrawAltHUDPatch(int x, int y, patch_t* patch, int from, int to, const byte* tinttab, int shadowcolor);
void V_DrawTranslucentAltHUDPatch(int x, int y, patch_t* patch, int from, int to, const byte* tinttab, int shadowcolor);
void V_DrawAltHUDWeaponPatch(int x, int y, patch_t* patch, int color, int shadowcolor, const byte* tinttab);
void V_DrawTranslucentAltHUDWeaponPatch(int x,
int y,
patch_t* patch,
int color,
int shadowcolor,
const byte* tinttab);
void V_DrawTranslucentNoGreenPatch(int x, int y, patch_t* patch);
void V_DrawTranslucentRedPatch(int x, int y, patch_t* patch);
void V_DrawFlippedTranslucentRedPatch(int x, int y, patch_t* patch);
void V_DrawPatchToTempScreen(int x, int y, patch_t* patch, byte* cr, int screenwidth);
void V_DrawAltHUDText(int x,
int y,
byte* screen,
patch_t* patch,
bool italics,
int color,
int shadowcolor,
int screenwidth,
const byte* tinttab);
void V_DrawTranslucentAltHUDText(int x,
int y,
byte* screen,
patch_t* patch,
bool italics,
int color,
int shadowcolor,
int screenwidth,
const byte* tinttab);
void V_DrawPagePatch(int screen, patch_t* patch);

void V_DrawPixel(int x, int y, byte color, bool highlight, bool shadow);

void V_LowGraphicDetail_2x2(byte* screen,
int screenwidth,
int left,
int top,
int width,
int height,
int pixelwidth,
int pixelheight);

void GetPixelSize(void);
void V_InvertScreen(void);

struct patch_s* V_LinearToTransPatch(const byte* data, int width, int height, int color_key);
