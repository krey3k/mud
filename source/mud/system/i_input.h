#pragma once

#if defined(_WIN32)
#include <windows.h>
#endif

#include "doom/doomtype.h"
#include <sokol_app.h>

void I_InitInput();
void I_ShutdownInput();

void I_ReadMouse(void);
void I_ReadController(void);
void I_SaveMousePointerPosition(void);
void I_RestoreMousePointerPosition(void);
bool MouseShouldBeGrabbed(void);
void SetShowCursor(bool show);

void I_StartTextInput();
void I_StopTextInput();

void I_InitKeyboard(void);
bool GetCapsLockState(void);

// Event queue functions
void I_InputQueueEvent(const sapp_event* event);
void I_InputProcessEventQueue(void);
void I_UpdateGrabFromMainThread(void);

#if defined(_WIN32)
void ToggleCapsLockState(void);
#elif defined(X11)
void SetCapsLockState(bool enabled);
#endif

int AccelerateMouse(int value);
void SmoothMouse(int* x, int* y);

bool keystate(const int key);

extern bool sendpause;
extern bool waspaused;

extern bool altdown;
extern int keydown;
extern int keydown2;

extern bool nokeyevent;

extern bool usingcontroller;
extern bool usingmouse;
extern bool windowfocused;
