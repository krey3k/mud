
//
// i_input.c - Input handling for keyboard, mouse, and events
//

#include "system/i_input.h"
#include "console/c_console.h"
#include "doom/doomdef.h"
#include "doom/doomkeys.h"
#include "doom/doomstat.h"
#include "game/g_game.h"
#include "system/i_controller.h"
#include "system/i_video.h"
#include "system/i_config.h"
#include "system/i_controls.h"
#include "menu/m_menu.h"

#include "thread.h"

// Include sokol_app for sapp_event
#include "sokol_app.h"

// STC vec for event queue
#define i_key sapp_event
#include <stc/vec.h>

//
// Global state
//

#define MAX_SOKOL_KEY SAPP_KEYCODE_MENU + 1
static bool sokol_key_state[MAX_SOKOL_KEY];

static vec_sapp_event event_queue = { 0 };

static bool capslock;
static int mousepointerx;
static int mousepointery;

bool usingcontroller = false;
bool usingmouse      = false;
bool windowfocused   = true;

int keydown     = 0;
int keydown2    = 0;
bool nokeyevent = false;

bool altdown   = false;
bool waspaused = false;

// Bit mask of mouse button state
static unsigned int mousebuttonstate;

static bool textinput = false;

static const int buttons[MAXMOUSEBUTTONS + 1] = { 0x0000, 0x0001, 0x0004,
    0x0002, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080 };


static void UpdateGrab(void);

static void I_ClearKeyState()
{
    memset(sokol_key_state, 0, sizeof(sokol_key_state));
}

void I_StartTextInput()
{
    textinput = true;
}

void I_StopTextInput()
{
    textinput = false;
}

//
// Event Queue Functions
//

//
// I_QueueEvent
//
// Queues a sapp_event for later processing
//
void I_InputQueueEvent(const sapp_event* event)
{
    if(!event)
        return;

    vec_sapp_event_push(&event_queue, *event);
}

static inline int I_TranslateKeyCode(sapp_keycode keycode)
{
    switch(keycode)
    {
    case SAPP_KEYCODE_INVALID:
        return 0;
    case SAPP_KEYCODE_SPACE:
        return KEY_SPACE;
    case SAPP_KEYCODE_APOSTROPHE:
        return '\'';
    case SAPP_KEYCODE_COMMA:
        return ',';
    case SAPP_KEYCODE_MINUS:
        return '-';
    case SAPP_KEYCODE_PERIOD:
        return ',';
    case SAPP_KEYCODE_SLASH:
        return '/';
    // Number Keys
    case SAPP_KEYCODE_0:
        return '0';
    case SAPP_KEYCODE_1:
        return '1';
    case SAPP_KEYCODE_2:
        return '2';
    case SAPP_KEYCODE_3:
        return '3';
    case SAPP_KEYCODE_4:
        return '4';
    case SAPP_KEYCODE_5:
        return '5';
    case SAPP_KEYCODE_6:
        return '6';
    case SAPP_KEYCODE_7:
        return '7';
    case SAPP_KEYCODE_8:
        return '8';
    case SAPP_KEYCODE_9:
        return '9';
    case SAPP_KEYCODE_SEMICOLON:
        return ';';
    case SAPP_KEYCODE_EQUAL:
        return '=';
    case SAPP_KEYCODE_A:
    case SAPP_KEYCODE_B:
    case SAPP_KEYCODE_C:
    case SAPP_KEYCODE_D:
    case SAPP_KEYCODE_E:
    case SAPP_KEYCODE_F:
    case SAPP_KEYCODE_G:
    case SAPP_KEYCODE_H:
    case SAPP_KEYCODE_I:
    case SAPP_KEYCODE_J:
    case SAPP_KEYCODE_K:
    case SAPP_KEYCODE_L:
    case SAPP_KEYCODE_M:
    case SAPP_KEYCODE_N:
    case SAPP_KEYCODE_O:
    case SAPP_KEYCODE_P:
    case SAPP_KEYCODE_Q:
    case SAPP_KEYCODE_R:
    case SAPP_KEYCODE_S:
    case SAPP_KEYCODE_T:
    case SAPP_KEYCODE_U:
    case SAPP_KEYCODE_V:
    case SAPP_KEYCODE_W:
    case SAPP_KEYCODE_X:
    case SAPP_KEYCODE_Y:
    case SAPP_KEYCODE_Z:
        return tolower(keycode);
    case SAPP_KEYCODE_LEFT_BRACKET:
        return '[';
    case SAPP_KEYCODE_BACKSLASH:
        return '\\';
    case SAPP_KEYCODE_RIGHT_BRACKET:
        return ']';
    case SAPP_KEYCODE_GRAVE_ACCENT:
        return '`';
    case SAPP_KEYCODE_WORLD_1:
    case SAPP_KEYCODE_WORLD_2:
        return 0;
    case SAPP_KEYCODE_ESCAPE:
        return KEY_ESCAPE;
    case SAPP_KEYCODE_ENTER:
        return KEY_ENTER;
    case SAPP_KEYCODE_TAB:
        return KEY_TAB;
    case SAPP_KEYCODE_BACKSPACE:
        return KEY_BACKSPACE;
    case SAPP_KEYCODE_INSERT:
        return KEY_INSERT;
    case SAPP_KEYCODE_DELETE:
        return KEY_DELETE;
    case SAPP_KEYCODE_RIGHT:
        return KEY_RIGHTARROW;
    case SAPP_KEYCODE_LEFT:
        return KEY_LEFTARROW;
    case SAPP_KEYCODE_DOWN:
        return KEY_DOWNARROW;
    case SAPP_KEYCODE_UP:
        return KEY_UPARROW;
    case SAPP_KEYCODE_PAGE_UP:
        return KEY_PAGEUP;
    case SAPP_KEYCODE_PAGE_DOWN:
        return KEY_PAGEDOWN;
    case SAPP_KEYCODE_HOME:
        return KEY_HOME;
    case SAPP_KEYCODE_END:
        return KEY_END;
    case SAPP_KEYCODE_CAPS_LOCK:
        return KEY_CAPSLOCK;
    case SAPP_KEYCODE_SCROLL_LOCK:
        return KEY_SCROLLLOCK;
    case SAPP_KEYCODE_NUM_LOCK:
        return KEY_NUMLOCK;
    case SAPP_KEYCODE_PRINT_SCREEN:
        return KEY_PRINTSCREEN;
    case SAPP_KEYCODE_PAUSE:
        return KEY_PAUSE;
    case SAPP_KEYCODE_F1:
        return KEY_F1;
    case SAPP_KEYCODE_F2:
        return KEY_F2;
    case SAPP_KEYCODE_F3:
        return KEY_F3;
    case SAPP_KEYCODE_F4:
        return KEY_F4;
    case SAPP_KEYCODE_F5:
        return KEY_F5;
    case SAPP_KEYCODE_F6:
        return KEY_F6;
    case SAPP_KEYCODE_F7:
        return KEY_F7;
    case SAPP_KEYCODE_F8:
        return KEY_F8;
    case SAPP_KEYCODE_F9:
        return KEY_F9;
    case SAPP_KEYCODE_F10:
        return KEY_F10;
    case SAPP_KEYCODE_F11:
        return KEY_F11;
    case SAPP_KEYCODE_F12:
        return KEY_F12;
    case SAPP_KEYCODE_F13:
    case SAPP_KEYCODE_F14:
    case SAPP_KEYCODE_F15:
    case SAPP_KEYCODE_F16:
    case SAPP_KEYCODE_F17:
    case SAPP_KEYCODE_F18:
    case SAPP_KEYCODE_F19:
    case SAPP_KEYCODE_F20:
    case SAPP_KEYCODE_F21:
    case SAPP_KEYCODE_F22:
    case SAPP_KEYCODE_F23:
    case SAPP_KEYCODE_F24:
    case SAPP_KEYCODE_F25:
        return 0;
    case SAPP_KEYCODE_KP_0:
        return KEYP_0;
    case SAPP_KEYCODE_KP_1:
        return KEYP_1;
    case SAPP_KEYCODE_KP_2:
        return KEYP_2;
    case SAPP_KEYCODE_KP_3:
        return KEYP_3;
    case SAPP_KEYCODE_KP_4:
        return KEYP_4;
    case SAPP_KEYCODE_KP_5:
        return KEYP_5;
    case SAPP_KEYCODE_KP_6:
        return KEYP_6;
    case SAPP_KEYCODE_KP_7:
        return KEYP_7;
    case SAPP_KEYCODE_KP_8:
        return KEYP_8;
    case SAPP_KEYCODE_KP_9:
        return KEYP_9;
    case SAPP_KEYCODE_KP_DECIMAL:
        return '.';
    case SAPP_KEYCODE_KP_DIVIDE:
        return '/';
    case SAPP_KEYCODE_KP_MULTIPLY:
        return '*';
    case SAPP_KEYCODE_KP_SUBTRACT:
        return '-';
    case SAPP_KEYCODE_KP_ADD:
        // note same as non-shifted on main keyboard
        return '=';
    case SAPP_KEYCODE_KP_ENTER:
        return KEY_ENTER;
    case SAPP_KEYCODE_KP_EQUAL:
        return '=';
    case SAPP_KEYCODE_LEFT_SHIFT:
    case SAPP_KEYCODE_RIGHT_SHIFT:
        return KEY_SHIFT;
    case SAPP_KEYCODE_LEFT_CONTROL:
    case SAPP_KEYCODE_RIGHT_CONTROL:
        return KEY_CTRL;
    case SAPP_KEYCODE_LEFT_ALT:
    case SAPP_KEYCODE_RIGHT_ALT:
        return KEY_ALT;
    case SAPP_KEYCODE_LEFT_SUPER:
    case SAPP_KEYCODE_RIGHT_SUPER:
    case SAPP_KEYCODE_MENU:
    default:
        return 0;
    }
}

static void I_ProcessKeyEvent(const sapp_event* event)
{
    if(event->type == SAPP_EVENTTYPE_KEY_DOWN && nokeyevent)
    {
        nokeyevent = false;
        return;
    }
    else
    {
        keydown  = 0;
        keydown2 = 0;
    }

    int doomkey = I_TranslateKeyCode(event->key_code);
    if(!doomkey)
    {
        return;
    }

    sokol_key_state[event->key_code] = event->type == SAPP_EVENTTYPE_KEY_DOWN;

    event_t ev = { .type = event->type == SAPP_EVENTTYPE_KEY_DOWN ? ev_keydown : ev_keyup, 0 };
    ev.data1 = doomkey;
    D_PostEvent(&ev);
}

static void I_ProcessCharEvent(const sapp_event* event)
{
    if(!textinput)
    {
        return;
    }

    event_t ev = { .type = ev_textinput, 0 };

    // event->char_code is a UTF-32/UCS-4 code point (uint32_t)
    uint32_t codepoint = event->char_code;

    // Convert UTF-32 to char for compatibility with existing event system
    // For ASCII range (0-127), directly use the value
    if(codepoint <= 0x7F)
    {
        ev.data1 = (int)codepoint;
    }
    else if(codepoint <= 0xFF)
    {
        // Extended ASCII/Latin-1 range
        ev.data1 = (int)codepoint;
    }
    else
    {
        // For characters outside basic range, you may need UTF-8 encoding
        // or handle differently depending on your needs
        // For now, just use the lower byte as a fallback
        ev.data1 = (int)(codepoint & 0xFF);
    }

    D_PostEvent(&ev);
}


//
// I_ProcessEventQueue
//
// Processes all queued sapp_events
//
void I_InputProcessEventQueue(void)
{
    UpdateGrab();
    
    c_foreach(it, vec_sapp_event, event_queue)
    {
        const sapp_event* ev = it.ref;

        if(ev->type == SAPP_EVENTTYPE_KEY_DOWN || ev->type == SAPP_EVENTTYPE_KEY_UP)
        {
            I_ProcessKeyEvent(ev);
        }
        if(ev->type == SAPP_EVENTTYPE_CHAR)
        {
            I_ProcessCharEvent(ev);
        }
        else if(ev->type == SAPP_EVENTTYPE_MOUSE_DOWN)
        {
            mousebuttonstate |= (1 << ev->mouse_button);
            D_PostEvent(&(event_t){
            .type  = ev_mouse,
            .data1 = mousebuttonstate,
            });
        }
        else if(ev->type == SAPP_EVENTTYPE_MOUSE_UP)
        {
            keydown = 0;
            mousebuttonstate &= ~(1 << ev->mouse_button);
            D_PostEvent(&(event_t){ .type = ev_mouse, .data1 = mousebuttonstate });
        }
        else if(ev->type == SAPP_EVENTTYPE_MOUSE_MOVE)
        {
            // MUD: Fixme, delta seems different than raw SDL?
            int x = (int)(ev->mouse_dx * 12.0f);
            int y = (int)(ev->mouse_dy * 12.0f);

            SmoothMouse(&x, &y);

            if(m_acceleration)
            {
                x = AccelerateMouse(x);
                y = AccelerateMouse(y);
            }

            D_PostEvent(&(event_t){
            .type = ev_mouse, .data1 = mousebuttonstate, .data2 = x, .data3 = y });
        }
    }

    // Clear the queue after processing
    vec_sapp_event_clear(&event_queue);
}

//
// I_InitEventQueue
//
// Initializes the event queue
//
static void I_InitEventQueue(void)
{
    event_queue = vec_sapp_event_init();
}

//
// I_ShutdownEventQueue
//
// Cleans up the event queue
//
static void I_ShutdownEventQueue(void)
{
    vec_sapp_event_drop(&event_queue);
}

//
// Mouse Grab Functions
//

//
// MouseShouldBeGrabbed
//
// Determines if the mouse cursor should be grabbed/hidden
//
bool MouseShouldBeGrabbed(void)
{
    // if the window doesn't have focus, never grab it
    if(!windowfocused)
        return false;

    // if not fullscreen, only grab the mouse when not playing a game
    if(!vid_fullscreen)
        return (game.state == GS_LEVEL && !menuactive && !consoleactive);

    // when menu is active, release the mouse
    if(((menuactive && !helpscreen) || consoleactive || game.state == GS_TITLESCREEN) &&
    m_pointer && usingmouse && !usingcontroller)
        return false;

    return true;
}

//
// SetShowCursor
//
// Shows or hides the mouse cursor
//
void SetShowCursor(bool show)
{
    sapp_lock_mouse(!show);
}

//
// Keyboard Translation
//

//
// keystate
//
// Returns true if the specified key is currently pressed
//
bool keystate(const int key)
{
#ifdef MUD_SOKOL_PORT
    const uint8_t* state = SDL_GetKeyboardState(NULL);

    return state[SDL_GetScancodeFromKey(key)];
#else
    return false;
#endif
}

//
// Caps Lock Management
//

#if defined(_WIN32)
//
// ToggleCapsLockState
//
// Toggles the caps lock state on Windows
//
void ToggleCapsLockState(void)
{
    keybd_event(VK_CAPITAL, 0x45, 0, (uintptr_t)0);
    keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_KEYUP, (uintptr_t)0);
}

#elif defined(X11)
//
// SetCapsLockState
//
// Sets the caps lock state on X11 systems
//
void SetCapsLockState(bool enabled)
{
    Display* display = XOpenDisplay(0);

    XkbLockModifiers(display, XkbUseCoreKbd, 2, enabled * 2);
    XFlush(display);
    XCloseDisplay(display);
}
#endif

//
// GetCapsLockState
//
// Returns the current caps lock state
//
bool GetCapsLockState(void)
{
#if defined(_WIN32)
    return (GetKeyState(VK_CAPITAL) & 0xFFFF);
#elif defined(MUD_SOKOL_PORT)
    return (SDL_GetModState() & KMOD_CAPS);
#else
    return false;
#endif
}

//
// I_ShutdownKeyboard
//
// Cleanup keyboard state on shutdown
//
static void I_ShutdownKeyboard(void)
{
#if defined(_WIN32)
    if(keyboardalwaysrun == KEY_CAPSLOCK && !capslock && GetCapsLockState())
        ToggleCapsLockState();
#elif defined(X11)
    if(keyboardalwaysrun == KEY_CAPSLOCK)
        SetCapsLockState(false);
#endif
}

//
// Event Handling
//

//
// I_GetEvent
//
// Poll and process SDL events
//
//
// I_GetEvent
//
// Poll and process SDL events
//
void I_GetEvent(void)
{
#ifdef MUD_SOKOL_PORT
    SDL_Event SDLEvent = { 0 };
    SDL_Event* Event   = &SDLEvent;

    SDL_PumpEvents();

    while(SDL_PollEvent(Event))
    {
        event_t ev = { 0 };

#if !defined(_WIN32)
        static bool enterdown;
#endif

        switch(Event->type)
        {
        case SDL_KEYDOWN:
        {
            const SDL_Scancode scancode = Event->key.keysym.scancode;

            if(nokeyevent)
            {
                nokeyevent = false;
                break;
            }

            if(scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_0 &&
            !SDL_IsTextInputActive())
                ev.data1 = translatekey[keypad[scancode - SDL_SCANCODE_KP_1]];
            else if(scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_RALT)
                ev.data1 = translatekey[scancode];

            ev.data2 = Event->key.keysym.sym;

            if(ev.data2 < SDLK_SPACE || ev.data2 > SDLK_z)
                ev.data2 = 0;

            altdown = (Event->key.keysym.mod & KMOD_ALT);

            if(ev.data1)
            {
                ev.type = ev_keydown;

                if(altdown && ev.data1 == KEY_F4)
                    M_QuitResponse('y');

                if(!isdigit(ev.data2))
                {
                    idclev = false;
                    idmus  = false;
                }

                if(idbehold && keys[ev.data2])
                {
                    idbehold = false;
                    HU_ClearMessages();
                    C_Cheat(cheat_powerup[6].sequence);
                    C_Output(s_STSTR_BEHOLD);
                }

#if !defined(_WIN32)
                // Handle ALT+ENTER on non-Windows systems
                if(altdown && ev.data1 == KEY_ENTER && !enterdown)
                {
                    enterdown = true;
                    I_ToggleFullscreen(true);

                    return;
                }
#endif

                D_PostEvent(&ev);

                if(game.state != GS_LEVEL)
                    I_SaveMousePointerPosition();

                usingmouse      = false;
                usingcontroller = false;
            }

            break;
        }

        case SDL_KEYUP:
        {
            const SDL_Scancode scancode = Event->key.keysym.scancode;

            if(scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_0 &&
            !SDL_IsTextInputActive())
                ev.data1 = translatekey[keypad[scancode - SDL_SCANCODE_KP_1]];
            else if(scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_RALT)
                ev.data1 = translatekey[scancode];

            altdown  = (Event->key.keysym.mod & KMOD_ALT);
            keydown  = 0;
            keydown2 = 0;

#if !defined(_WIN32)
            // Handle ALT+ENTER on non-Windows systems
            if(ev.data1 == KEY_ENTER)
                enterdown = false;
#endif

            if(ev.data1)
            {
                ev.type = ev_keyup;
                D_PostEvent(&ev);
            }

            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        {
            const int button = buttons[Event->button.button];

            idclev = false;
            idmus  = false;

            if(idbehold)
            {
                HU_ClearMessages();
                idbehold = false;
                C_Cheat(cheat_powerup[6].sequence);
                C_Output(s_STSTR_BEHOLD);
            }

            mousebuttonstate |= button;

            if((menuactive || consoleactive) && button == MOUSE_RIGHTBUTTON)
                usingmouse = false;

            break;
        }

        case SDL_MOUSEBUTTONUP:
            keydown = 0;
            mousebuttonstate &= ~buttons[Event->button.button];
            break;

        case SDL_MOUSEWHEEL:
            keydown  = 0;
            ev.type  = ev_mousewheel;
            ev.data1 = Event->wheel.y;

            if(menuactive || consoleactive)
                usingmouse = false;

            D_PostEvent(&ev);
            break;

        case SDL_TEXTINPUT:
        {
            char* text = (char*)SDL_iconv_utf8_ucs4(Event->text.text);

            ev.data1 =
            (text[0] ? text[0] : Event->text.text[strlen(Event->text.text) - 1]);
            ev.type = ev_textinput;
            D_PostEvent(&ev);
            break;
        }

        case SDL_CONTROLLERDEVICEADDED:
            I_InitController();
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            I_ShutdownController();
            break;

        case SDL_QUIT:
            if(!quitting)
            {
                keydown = 0;
                C_HideConsoleFast();

                if(paused)
                {
                    paused    = false;
                    waspaused = true;
                }

                M_OpenMainMenu();
                S_StartSound(NULL, sfx_swtchn);
                M_QuitDOOM(0);
            }

            break;

        case SDL_WINDOWEVENT:
            if(Event->window.windowID == windowid)
            {
                switch(Event->window.event)
                {
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    if(!windowfocused)
                    {
                        windowfocused = true;
                        paused        = false;
                        S_ResumeMusic();

                        if(!mapwindow)
                            S_StartSound(NULL, sfx_swtchx);

                        I_InitKeyboard();

                        if(reopenautomap)
                        {
                            reopenautomap = false;
                            AM_Start(true);
                            viewactive = false;
                        }
                    }

                    I_SetPriority(true);
                    break;

                case SDL_WINDOWEVENT_FOCUS_LOST:
                case SDL_WINDOWEVENT_MINIMIZED:
                    windowfocused = false;

                    if(!s_musicinbackground)
                        S_PauseMusic();

                    if(game.state == GS_LEVEL && !menuactive && !consoleactive && !paused)
                        sendpause = true;
                    else if(!mapwindow)
                        S_StartSound(NULL, sfx_swtchn);

                    I_ShutdownKeyboard();
                    I_SetPriority(false);
                    break;

                case SDL_WINDOWEVENT_EXPOSED:
                    SDL_SetPaletteColors(palette, colors, 0, 256);
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    if(!vid_fullscreen)
                    {
                        char* temp1 = commify((video.window_width = Event->window.data1));
                        char* temp2 = commify((video.window_height = Event->window.data2));
                        char size[16];

                        M_snprintf(size, sizeof(size), "%sx%s", temp1, temp2);
                        vid_windowsize = M_StringDuplicate(size);
                        M_SaveCVARs();

                        displaywidth  = video.window_width;
                        displayheight = video.window_height;

                        free(temp1);
                        free(temp2);

                        I_RestartGraphics(false);
                    }

                    break;

                case SDL_WINDOWEVENT_MOVED:
                    if(!vid_fullscreen && !manuallypositioning)
                    {
                        char pos[16];

                        video.window_x = Event->window.data1;
                        video.window_y = Event->window.data2;
                        M_snprintf(pos, sizeof(pos), "(%i,%i)", video.window_x, video.window_y);
                        vid_windowpos = M_StringDuplicate(pos);
                        vid_display   = SDL_GetWindowDisplayIndex(window) + 1;
                        M_SaveCVARs();
                    }

                    manuallypositioning = false;
                    break;
                }
            }

            break;
        }
    }
#endif // MUD_SOKOL_PORT
}

//
// Mouse Functions
//

//
// I_SaveMousePointerPosition
//
// Save current mouse pointer coordinates
//
void I_SaveMousePointerPosition(void)
{
#ifdef MUD_SOKOL_PORT
    SDL_GetMouseState(&mousepointerx, &mousepointery);
#endif
}

//
// I_RestoreMousePointerPosition
//
// Restore saved mouse pointer coordinates
//
void I_RestoreMousePointerPosition(void)
{
#ifdef MUD_SOKOL_PORT
    SDL_WarpMouseInWindow(window, mousepointerx, mousepointery);
#endif
}

//
// SmoothMouse
//
// Apply mouse smoothing to movement
//
void SmoothMouse(int* x, int* y)
{
    const fixed_t adjustment = FixedDiv(fractionaltic, FRACUNIT + fractionaltic);
    static int xx, yy;

    *x += xx;
    xx = FixedMul(*x, adjustment);
    *x -= xx;

    *y += yy;
    yy = FixedMul(*y, adjustment);
    *y -= yy;
}

//
// AccelerateMouse
//
// Apply mouse acceleration to movement value
//
int AccelerateMouse(int value)
{
    return (value < -10 ? value * 2 + 10 : (value < 10 ? value : value * 2 - 10));
}

//
// I_ReadMouse
//
// Read and process mouse input
//
void I_ReadMouse(void)
{
#ifdef MUD_SOKOL_PORT
    int x, y;
    static unsigned int prevmousebuttonstate;

    SDL_GetRelativeMouseState(&x, &y);

    if(x || y || mousebuttonstate != prevmousebuttonstate ||
    (mousebuttonstate && (menuactive || consoleactive)))
    {
        event_t ev = { ev_mouse, mousebuttonstate, 0, 0 };

        if(((menuactive && !helpscreen) || consoleactive || game.state == GS_TITLESCREEN) &&
         m_pointer)
        {
            if(x || y)
            {
                usingmouse      = true;
                usingcontroller = false;
            }

            SDL_GetMouseState(&x, &y);

            if(vid_widescreen)
            {
                ev.data2 = (x - dest_rect.x) * video.screen_width / dest_rect.w / 2;
                ev.data3 = (y - dest_rect.y) * video.screen_height / dest_rect.h / 2;
            }
            else
            {
                ev.data2 = x * V_WIDESCREENWIDTH / displaywidth / 2;
                ev.data3 = y * video.screen_height / displayheight / 2;
            }
        }
        else
        {
            SmoothMouse(&x, &y);

            if(m_acceleration)
            {
                ev.data2 = AccelerateMouse(x);
                ev.data3 = AccelerateMouse(y);
            }
            else
            {
                ev.data2 = x;
                ev.data3 = y;
            }
        }

        prevmousebuttonstate = mousebuttonstate;
        D_PostEvent(&ev);
    }
#endif
}

//
// UpdateGrab
//
// Update mouse grab state
//
static void UpdateGrab(void)
{
    bool grab = MouseShouldBeGrabbed();
    static bool currently_grabbed;

    if(grab == currently_grabbed)
        return;

    if(grab && !currently_grabbed)
        SetShowCursor(false);
    else if(!grab && currently_grabbed)
        SetShowCursor(true);

    currently_grabbed = grab;
}

//
// Keyboard Initialization
//

//
// I_InitKeyboard
//
// Initialize keyboard state
//
void I_InitKeyboard(void)
{
    if(keyboardalwaysrun == KEY_CAPSLOCK)
    {
        capslock = GetCapsLockState();

#if defined(_WIN32)
        if(alwaysrun != capslock)
            ToggleCapsLockState();
#elif defined(X11)
        if(alwaysrun && !capslock)
            SetCapsLockState(true);
        else if(!alwaysrun && capslock)
            SetCapsLockState(false);
#endif
    }
}

//
// Main Input Functions
//

//
// I_InitInput
//
// Initialize all input systems
//
void I_InitInput(void)
{
    I_ClearKeyState();
    I_InitEventQueue();
    I_InitController();
}

//
// I_ShutdownInput
//
// Shutdown all input systems
//
void I_ShutdownInput(void)
{
    I_ShutdownEventQueue();
    I_ShutdownKeyboard();
    I_ShutdownController();
}
