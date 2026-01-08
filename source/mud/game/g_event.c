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

#if defined(_WIN32)
#include <windows.h>
#endif

#include "automap/am_map.h"
#include "console/c_console.h"
#include "doom/d_deh.h"
#include "doom/d_main.h"
#include "doom/doomstat.h"
#include "intermission/f_finale.h"
#include "game/g_event.h"
#include "game/g_game.h"
#include "hud/hu_stuff.h"
#include "math/math_colors.h"
#include "system/i_controller.h"
#include "system/i_input.h"
#include "system/i_timer.h"
#include "system/i_config.h"
#include "system/i_controls.h"
#include "menu/m_menu.h"
#include "utils/m_misc.h"
#include "playsim/p_local.h"
#include "sound/s_sound.h"
#include "hud/st_stuff.h"
#include "render/v_video.h"

fixed_t forwardmove[] = { FORWARDMOVE0, FORWARDMOVE1 };
fixed_t sidemove[]    = { SIDEMOVE0, SIDEMOVE1 };
fixed_t angleturn[]   = { 640, 1280, 320 }; // + slow turn

static const int* keyboardweapons[NUMWEAPONKEYS] = { &keyboardweapon1,
    &keyboardweapon2, &keyboardweapon3, &keyboardweapon4, &keyboardweapon5,
    &keyboardweapon6, &keyboardweapon7 };

static const int* keyboardweapons2[NUMWEAPONKEYS] = { &keyboardweapon1_2,
    &keyboardweapon2_2, &keyboardweapon3_2, &keyboardweapon4_2,
    &keyboardweapon5_2, &keyboardweapon6_2, &keyboardweapon7_2 };

static const int* keyboardweapons3[NUMWEAPONKEYS + 2] = { &keyboardfists,
    &keyboardpistol, &keyboardshotgun, &keyboardchaingun, &keyboardrocketlauncher,
    &keyboardplasmarifle, &keyboardbfg9000, &keyboardchainsaw, &keyboardsupershotgun };

static const int* keyboardweapons4[NUMWEAPONKEYS + 2] = { &keyboardfists2,
    &keyboardpistol2, &keyboardshotgun2, &keyboardchaingun2, &keyboardrocketlauncher2,
    &keyboardplasmarifle2, &keyboardbfg90002, &keyboardchainsaw2, &keyboardsupershotgun2 };

static const int* mouseweapons[NUMWEAPONKEYS] = { &mouseweapon1, &mouseweapon2,
    &mouseweapon3, &mouseweapon4, &mouseweapon5, &mouseweapon6, &mouseweapon7 };

static const int* mouseweapons2[NUMWEAPONKEYS + 2] = { &mousefists,
    &mousepistol, &mouseshotgun, &mousechaingun, &mouserocketlauncher,
    &mouseplasmarifle, &mousebfg9000, &mousechainsaw, &mousesupershotgun };

static const int* controllerweapons[NUMWEAPONKEYS] = { &controllerweapon1,
    &controllerweapon2, &controllerweapon3, &controllerweapon4,
    &controllerweapon5, &controllerweapon6, &controllerweapon7 };

static const int* controllerweapons2[NUMWEAPONKEYS + 2] = { &controllerfists,
    &controllerpistol, &controllershotgun, &controllerchaingun,
    &controllerrocketlauncher, &controllerplasmarifle, &controllerbfg9000,
    &controllerchainsaw, &controllersupershotgun };

char keyactionlist[NUMKEYS][255] = { "" };
static int turnheld; // for accelerative turning

static bool mousearray[MAXMOUSEBUTTONS + 3];
bool* mousebuttons                             = &mousearray[1]; // allow [-1]
char mouseactionlist[MAXMOUSEBUTTONS + 2][255] = { "" };

static int mousex;
static int mousey;

bool sendpause = false;       // send a pause event next tic
bool sendsave  = false;       // send a save event next tic

bool usefreelook = false;

// Mouse sensitivity and look constants
#define MOUSE_TURN_SPEED    0x08
#define MOUSE_SENS_DIVISOR  10.0f
#define FREELOOK_RANGE      96
#define MOUSE_MOVE_DIVISOR  2
#define CONTROLLER_Y_MAX    ((float)SHRT_MAX)

// G_Responder constants
#define MOUSEWAIT_TICKS     5
#define CONTROLLERWAIT_TICKS 8
#define CONTROLLER_REPEAT_DELAY 7
#define PAGETIC_SKIP        10

// Helper: Check if any input source is active for a given action
static inline bool G_InputActive(const int key1, const int key2, const int mouse, const unsigned int controller)
{
    return (game.keydown[key1] || game.keydown[key2] || mousebuttons[mouse] || (controllerbuttons & controller));
}

// Helper: Update menu spin direction
static inline void G_UpdateMenuSpin(const int angleturn)
{
    if(!menuactive)
        menuspindirection = SIGN(angleturn);
}

// Helper: Apply controller angle turn
static inline void G_ApplyControllerTurn(ticcmd_t* cmd, const int thumbX, const float sensitivity)
{
    cmd->angleturn -= FixedMul(CONTROLLERANGLETURN, (fixed_t)(sensitivity * thumbX));
    G_UpdateMenuSpin(cmd->angleturn);
}

// Helper: Handle title screen events
static bool G_HandleTitleScreenEvent(const event_t* ev)
{
    bool shouldHandle = false;

    if(ev->type == ev_keydown)
        shouldHandle = (!keydown && 
                        (ev->data1 < KEY_F1 || ev->data1 > KEY_F12) &&
                        ev->data1 != KEY_BACKSPACE && ev->data1 != KEY_ALT &&
                        !((ev->data1 == KEY_ENTER || ev->data1 == KEY_TAB) && altdown) &&
                        ev->data1 != keyboardscreenshot && ev->data1 != keyboardscreenshot2);
    else if(ev->type == ev_mouse)
        shouldHandle = (mousewait < I_GetTime() && ev->data1 && !(ev->data1 & MOUSE_RIGHTBUTTON));
    else if(ev->type == ev_controller)
        shouldHandle = (controllerwait < I_GetTime() && controllerbuttons);

    if(!shouldHandle)
        return false;

    // Handle alwaysrun toggle
    if(ev->type == ev_keydown &&
       (ev->data1 == keyboardalwaysrun || ev->data1 == keyboardalwaysrun2))
    {
        keydown = ev->data1;
        G_ToggleAlwaysRun(ev_keydown);
    }
    else if(ev->type == ev_mouse && ev->data1 == mousealwaysrun)
        G_ToggleAlwaysRun(ev_mouse);
    else
    {
        keydown           = ev->data1;
        controllerbuttons = 0;
        mousewait         = I_GetTime() + MOUSEWAIT_TICKS;
        controllerwait    = mousewait + CONTROLLERWAIT_TICKS - MOUSEWAIT_TICKS;
        pagetic = PAGETICS;
        M_OpenMainMenu();
        S_StartSound(NULL, sfx_swtchn);
    }

    return true;
}

// Helper: Handle screenshot event on title screen
static bool G_HandleScreenshotEvent(const event_t* ev)
{
    if(!menuactive && !consoleactive && ev->type == ev_keyup &&
       (ev->data1 == keyboardscreenshot || ev->data1 == keyboardscreenshot2))
    {
        S_StartSound(NULL, sfx_scrsht);
        memset(v_screens[0], nearestwhite, video.screen_area);
        D_FadeScreen(true);

        return true;
    }

    return false;
}

// Helper: Handle mousewheel weapon cycling
static void G_HandleMouseWheelWeapon(const int wheelButton, const int wheelDirection)
{
    (void)wheelDirection; // unused, kept for potential future use
    
    if(mousenextweapon == wheelButton)
        G_NextWeapon();
    else if(mouseprevweapon == wheelButton)
        G_PrevWeapon();
    else if(mouseactionlist[wheelButton][0])
        C_ExecuteInputString(mouseactionlist[wheelButton]);
    else
        mousebuttons[wheelButton] = true;
}

// Helper: Handle different event types (keyboard, mouse, controller)
static bool G_HandleEvent(const event_t* ev)
{
    int key;

    switch(ev->type)
    {
    case ev_keydown:
        key = ev->data1;

        if((key == keyboardprevweapon || key == keyboardprevweapon2) &&
        !menuactive && !paused && !freeze)
            G_PrevWeapon();
        else if((key == keyboardnextweapon || key == keyboardnextweapon2) &&
        !menuactive && !paused && !freeze)
            G_NextWeapon();
        else if(key == KEY_PAUSE && !menuactive && !keydown && !idclevtics)
        {
            keydown   = KEY_PAUSE;
            sendpause = true;

            if(vid_motionblur)
                I_SetMotionBlur(0);

            D_FadeScreen(false);
        }
        else if((key == keyboardalwaysrun || key == keyboardalwaysrun2) && !keydown)
        {
            keydown = key;
            G_ToggleAlwaysRun(ev_keydown);
        }
        else if(key < NUMKEYS)
        {
            game.keydown[key] = true;

            if(keyactionlist[key][0])
                C_ExecuteInputString(keyactionlist[key]);
        }

        return true; // eat events

    case ev_keyup:
        if(ev->data1 < NUMKEYS)
            game.keydown[ev->data1] = false;

        return false; // always let key up events filter down

    case ev_mouse:
    {
        const int mousebutton = ev->data1;

        for(int i = 0, j = 1; i < MAXMOUSEBUTTONS; i++, j <<= 1)
            mousebuttons[i] = !!(mousebutton & j);

        if(mousebuttons[mousealwaysrun])
            G_ToggleAlwaysRun(ev_mouse);

        if(mouseactionlist[mousebutton][0] && !freeze)
            C_ExecuteInputString(mouseactionlist[mousebutton]);

        if(!automapactive && !menuactive && !paused && !freeze)
        {
            if(mousenextweapon < MAXMOUSEBUTTONS && mousebuttons[mousenextweapon])
                G_NextWeapon();
            else if(mouseprevweapon < MAXMOUSEBUTTONS && mousebuttons[mouseprevweapon])
                G_PrevWeapon();
        }

        if(!automapactive || am_followmode)
        {
            mousex = (int)(ev->data2 * m_sensitivity / MOUSE_SENS_DIVISOR);
            mousey = (int)(-ev->data3 * m_sensitivity / MOUSE_SENS_DIVISOR);
        }

        return true; // eat events
    }

    case ev_mousewheel:
        if(!automapactive && !menuactive && !paused && !freeze)
        {
            if(ev->data1 < 0)
                G_HandleMouseWheelWeapon(MOUSE_WHEELDOWN, -1);
            else if(ev->data1 > 0)
                G_HandleMouseWheelWeapon(MOUSE_WHEELUP, 1);
        }

        return true; // eat events

    case ev_controller:
        if(!automapactive && !menuactive && !paused)
        {
            static uint64_t wait;
            const uint64_t time = I_GetTime();

            if(wait < time)
            {
                if((controllerbuttons & controllernextweapon) && !freeze)
                {
                    wait = time + CONTROLLER_REPEAT_DELAY;

                    if(!controllerpress || controllerwait < time)
                    {
                        G_NextWeapon();
                        controllerpress = false;
                    }
                }
                else if((controllerbuttons & controllerprevweapon) && !freeze)
                {
                    wait = time + CONTROLLER_REPEAT_DELAY;

                    if(!controllerpress || controllerwait < time)
                    {
                        G_PrevWeapon();
                        controllerpress = false;
                    }
                }
                else if(controllerbuttons & controlleralwaysrun)
                {
                    wait = time + CONTROLLER_REPEAT_DELAY;

                    if(!controllerpress || controllerwait < time)
                    {
                        G_ToggleAlwaysRun(ev_controller);
                        controllerpress = false;
                    }
                }
            }
        }

        return true; // eat events

    default:
        return false;
    }
}

void G_RemoveChoppers(void)
{
    viewplayer->cheats &= ~CF_CHOPPERS;
    viewplayer->powers[pw_invulnerability] =
    (viewplayer->invulnbeforechoppers ? 1 : STARTFLASHING);
    viewplayer->weaponowned[wp_chainsaw] = viewplayer->chainsawbeforechoppers;
    oldweaponsowned[wp_chainsaw]         = viewplayer->chainsawbeforechoppers;
}

// Helper: Check if weapon can be selected
static bool G_CanSelectWeapon(const weapontype_t weapon)
{
    return viewplayer->weaponowned[weapon] &&
           (viewplayer->ammo[weaponinfo[weapon].ammotype] >= weaponinfo[weapon].ammopershot ||
            infiniteammo);
}

// Helper: Update weapon-specific player state
static void G_UpdateWeaponState(const weapontype_t weapon)
{
    if(weapon == wp_fist)
    {
        if(viewplayer->powers[pw_strength])
            S_StartSound(NULL, sfx_getpow);
        viewplayer->fistorchainsaw = wp_fist;
    }
    else if(weapon == wp_chainsaw)
        viewplayer->fistorchainsaw = wp_chainsaw;
    else if(weapon == wp_shotgun || weapon == wp_supershotgun)
        viewplayer->preferredshotgun = weapon;
}

void G_NextWeapon(void)
{
    const weapontype_t pendingweapon = viewplayer->pendingweapon;
    const weapontype_t readyweapon   = viewplayer->readyweapon;
    weapontype_t i = (pendingweapon == wp_nochange ? readyweapon : pendingweapon);

    if(viewplayer->health <= 0)
        return;

    do
    {
        i = weaponinfo[i].nextweapon;

        if(i == wp_fist && viewplayer->weaponowned[wp_chainsaw] &&
        !viewplayer->powers[pw_strength])
            i = wp_chainsaw;
    } while(!G_CanSelectWeapon(i));

    if(i != readyweapon)
    {
        viewplayer->pendingweapon = i;
        G_UpdateWeaponState(i);
    }

    if((viewplayer->cheats & CF_CHOPPERS) && i != wp_chainsaw)
        G_RemoveChoppers();
}

void G_PrevWeapon(void)
{
    const weapontype_t pendingweapon = viewplayer->pendingweapon;
    const weapontype_t readyweapon   = viewplayer->readyweapon;
    weapontype_t i = (pendingweapon == wp_nochange ? readyweapon : pendingweapon);

    if(viewplayer->health <= 0)
        return;

    do
    {
        i = weaponinfo[i].prevweapon;

        if(i == wp_fist && viewplayer->weaponowned[wp_chainsaw] &&
        !viewplayer->powers[pw_strength])
            i = wp_bfg;
    } while(!G_CanSelectWeapon(i));

    if(i != readyweapon)
    {
        viewplayer->pendingweapon = i;
        G_UpdateWeaponState(i);
    }

    if((viewplayer->cheats & CF_CHOPPERS) && i != wp_chainsaw)
        G_RemoveChoppers();
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs.
//
void G_BuildTiccmd(ticcmd_t* cmd)
{
    bool strafe;
    int run;
    int forward = 0;
    int side    = 0;

    // Cache input states to avoid redundant function calls
    bool inputRight;
    bool inputLeft;
    bool inputForward;
    bool inputBack;
    bool inputStrafeRight;
    bool inputStrafeLeft;
    bool inputJump;
    bool inputFire;
    bool inputUse;

    // [BH] This needs to be reset every tic, even if automap open and follow mode off
    memset(cmd, 0, sizeof(ticcmd_t));

    if(automapactive && !am_followmode && viewplayer->health > 0)
        return;

    strafe = G_InputActive(keyboardstrafe, keyboardstrafe2, mousestrafe, controllerstrafe);
    run    = (G_InputActive(keyboardrun, keyboardrun2, mouserun, controllerrun) ^ alwaysrun);
    usefreelook = (freelook || G_InputActive(keyboardfreelook, keyboardfreelook2, mousefreelook, controllerfreelook));

    // Cache all input states once
    inputRight       = G_InputActive(keyboardright, keyboardright2, mouseright, controllerright);
    inputLeft        = G_InputActive(keyboardleft, keyboardleft2, mouseleft, controllerleft);
    inputForward     = G_InputActive(keyboardforward, keyboardforward2, mouseforward, controllerforward);
    inputBack        = G_InputActive(keyboardback, keyboardback2, mouseback, controllerback);
    inputStrafeRight = G_InputActive(keyboardstraferight, keyboardstraferight2, mousestraferight, controllerstraferight);
    inputStrafeLeft  = G_InputActive(keyboardstrafeleft, keyboardstrafeleft2, mousestrafeleft, controllerstrafeleft);
    inputJump        = G_InputActive(keyboardjump, keyboardjump2, mousejump, controllerjump);
    inputFire        = G_InputActive(keyboardfire, keyboardfire2, mousefire, controllerfire);
    inputUse         = G_InputActive(keyboarduse, keyboarduse2, mouseuse, (controlleruse | controlleruse2));

    // use two stage accelerative turning on the keyboard
    if(game.keydown[keyboardright] || game.keydown[keyboardright2] ||
    game.keydown[keyboardleft] || game.keydown[keyboardleft2] ||
    (controllerbuttons & (controllerleft | controllerright)))
        turnheld++;
    else
        turnheld = 0;

    // let movement keys cancel each other out
    if(strafe)
    {
        if(inputRight)
            side += sidemove[run];

        if(inputLeft)
            side -= sidemove[run];
    }
    else
    {
        if(inputRight)
        {
            cmd->angleturn -= angleturn[(turnheld < SLOWTURNTICS ? 2 : run)];
            G_UpdateMenuSpin(cmd->angleturn);
        }
        else if(controllerthumbRX > 0)
            G_ApplyControllerTurn(cmd, controllerthumbRX, controllerhorizontalsensitivity);

        if(inputLeft)
        {
            cmd->angleturn += angleturn[(turnheld < SLOWTURNTICS ? 2 : run)];
            G_UpdateMenuSpin(cmd->angleturn);
        }
        else if(controllerthumbRX < 0)
            G_ApplyControllerTurn(cmd, controllerthumbRX, controllerhorizontalsensitivity);
    }

    if(controllerthumbRY)
    {
        if(usefreelook && joy_thumbsticks == 2)
        {
            if(!automapactive)
            {
                cmd->lookdir = (int)(FREELOOK_RANGE * (controllerthumbRY / CONTROLLER_Y_MAX) * controllerverticalsensitivity);

                if(!joy_invertyaxis)
                    cmd->lookdir = -cmd->lookdir;
            }
        }
        else if(joy_thumbsticks == 1)
        {
            cmd->lookdir = 0;
            forward = (int)(forwardmove[run] * (controllerthumbRY / CONTROLLER_Y_MAX));
        }
    }

    if(inputForward)
        forward += forwardmove[run];
    else if(controllerthumbLY < 0)
        forward -= (int)(forwardmove[run] * (controllerthumbLY / CONTROLLER_Y_MAX));

    if(inputBack)
        forward -= forwardmove[run];
    else if(controllerthumbLY > 0)
        forward -= (int)(forwardmove[run] * (controllerthumbLY / CONTROLLER_Y_MAX));

    if(inputStrafeRight)
        side += sidemove[run];
    else if(controllerthumbLX > 0)
    {
        if(joy_thumbsticks == 2)
            side += (int)(sidemove[run] * (controllerthumbLX / CONTROLLER_Y_MAX));
        else
            G_ApplyControllerTurn(cmd, controllerthumbLX, controllerhorizontalsensitivity);
    }

    if(inputStrafeLeft)
        side -= sidemove[run];
    else if(controllerthumbLX < 0)
    {
        if(joy_thumbsticks == 2)
            side += (int)(sidemove[run] * (controllerthumbLX / CONTROLLER_Y_MAX));
        else
            G_ApplyControllerTurn(cmd, controllerthumbLX, controllerhorizontalsensitivity);
    }

    if(inputJump && !nojump)
        cmd->buttons |= BT_JUMP;

    // buttons
    if(!freeze)
    {
        if(inputFire)
            cmd->buttons |= BT_ATTACK;

        if(inputUse)
            cmd->buttons |= BT_USE;
    }

    if(!idclev && !idmus)
    {
        for(int i = 0; i < NUMWEAPONKEYS; i++)
        {
            const int key  = *keyboardweapons[i];
            const int key2 = *keyboardweapons2[i];

            if(game.keydown[key] && !keydown)
            {
                keydown = key;
                cmd->buttons |= (BT_CHANGE | (i << BT_WEAPONSHIFT));
                break;
            }
            else if(game.keydown[key2] && !keydown)
            {
                keydown = key2;
                cmd->buttons |= (BT_CHANGE | (i << BT_WEAPONSHIFT));
                break;
            }
            else if(mousebuttons[*mouseweapons[i]])
            {
                if(viewplayer->readyweapon != i ||
                (i == wp_fist && viewplayer->weaponowned[wp_chainsaw]) ||
                (i == wp_shotgun && viewplayer->weaponowned[wp_supershotgun]))
                {
                    cmd->buttons |= (BT_CHANGE | (i << BT_WEAPONSHIFT));
                    mousebuttons[*mouseweapons[i]] = false;
                    break;
                }
            }
            else if(controllerbuttons & *controllerweapons[i])
            {
                if(viewplayer->readyweapon != i ||
                (i == wp_fist && viewplayer->weaponowned[wp_chainsaw]) ||
                (i == wp_shotgun && viewplayer->weaponowned[wp_supershotgun]))
                {
                    cmd->buttons |= (BT_CHANGE | (i << BT_WEAPONSHIFT));
                    break;
                }
            }
        }

        if(!(cmd->buttons & BT_CHANGE))
            for(int i = 0; i < NUMWEAPONKEYS + 2; i++)
            {
                const int key  = *keyboardweapons3[i];
                const int key2 = *keyboardweapons4[i];

                if(game.keydown[key] && !keydown)
                {
                    keydown = key;
                    cmd->buttons |= (BT_CHANGE | (i << BT_WEAPONSHIFT));
                    cmd->buttons |= BT_NOBEST;
                    break;
                }
                else if(game.keydown[key2] && !keydown)
                {
                    keydown = key2;
                    cmd->buttons |= (BT_CHANGE | (i << BT_WEAPONSHIFT));
                    cmd->buttons |= BT_NOBEST;
                    break;
                }
                else if(mousebuttons[*mouseweapons2[i]])
                {
                    if(viewplayer->readyweapon != i ||
                    (i == wp_fist && viewplayer->weaponowned[wp_chainsaw]) ||
                    (i == wp_shotgun && viewplayer->weaponowned[wp_supershotgun]))
                    {
                        cmd->buttons |= (BT_CHANGE | (i << BT_WEAPONSHIFT));
                        cmd->buttons |= BT_NOBEST;
                        mousebuttons[*mouseweapons2[i]] = false;
                        break;
                    }
                }
                else if(controllerbuttons & *controllerweapons2[i])
                {
                    if(viewplayer->readyweapon != i ||
                    (i == wp_fist && viewplayer->weaponowned[wp_chainsaw]) ||
                    (i == wp_shotgun && viewplayer->weaponowned[wp_supershotgun]))
                    {
                        cmd->buttons |= (BT_CHANGE | (i << BT_WEAPONSHIFT));
                        cmd->buttons |= BT_NOBEST;
                        break;
                    }
                }
            }
    }

    if(mousex)
    {
        if(strafe)
            side += mousex / MOUSE_MOVE_DIVISOR;
        else
        {
            cmd->angleturn -= mousex * MOUSE_TURN_SPEED;
            G_UpdateMenuSpin(cmd->angleturn);
        }

        mousex = 0;
    }

    if(mousey)
    {
        if(usefreelook && !automapactive)
            cmd->lookdir = (m_invertyaxis ? -mousey : mousey);
        else if(!m_novertical)
            forward += mousey / MOUSE_MOVE_DIVISOR;

        mousey = 0;
    }

    if(forward)
        cmd->forwardmove += BETWEEN(-MAXPLMOVE, forward, MAXPLMOVE);

    if(side)
        cmd->sidemove += BETWEEN(-MAXPLMOVE, side, MAXPLMOVE);

    // special buttons
    if(sendpause)
    {
        sendpause    = false;
        cmd->buttons = (BT_SPECIAL | BTS_PAUSE);
    }

    if(sendsave)
    {
        sendsave     = false;
        cmd->buttons = (BT_SPECIAL | BTS_SAVEGAME);
    }
}

void G_ToggleAlwaysRun(evtype_t type)
{
    char temp[255];
    const int oldcaretpos    = caretpos;
    const int oldselectstart = selectstart;
    const int oldselectend   = selectend;

#if defined(_WIN32)
    alwaysrun = (keyboardalwaysrun == KEY_CAPSLOCK && type == ev_keydown ?
    (GetKeyState(VK_CAPITAL) & 0x0001) :
    !alwaysrun);
#else
    (void)type; // unused on non-Windows
    alwaysrun = !alwaysrun;
#endif

    M_StringCopy(temp, consoleinput, sizeof(temp));
    C_StringCVAROutput(stringize(alwaysrun), (alwaysrun ? "on" : "off"));
    M_StringCopy(consoleinput, temp, sizeof(consoleinput));

    caretpos    = oldcaretpos;
    selectstart = oldselectstart;
    selectend   = oldselectend;

    if(!consoleactive)
    {
        if(alwaysrun)
        {
            HU_SetPlayerMessage(s_ALWAYSRUNON, false, false);
            C_Output(s_ALWAYSRUNON);
        }
        else
        {
            HU_SetPlayerMessage(s_ALWAYSRUNOFF, false, false);
            C_Output(s_ALWAYSRUNOFF);
        }

        message_dontfuckwithme = true;
    }

    M_SaveCVARs();
}

//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//
bool G_Responder(const event_t* ev)
{
    // any other key pops up menu if on title screen
    if(game.action == ga_nothing && game.state == GS_TITLESCREEN)
    {
        if(!menuactive && !consoleactive && !fadecount && G_HandleTitleScreenEvent(ev))
            return true;
        else if(G_HandleScreenshotEvent(ev))
            return true;

        return false;
    }

    if(game.state == GS_LEVEL)
    {
        if(ST_Responder(ev))
            return true; // status window ate it

        if(AM_Responder(ev))
            return true; // automap ate it
    }

    if(game.state == GS_FINALE && F_Responder(ev))
        return true; // finale ate the event

    mousebuttons[MOUSE_WHEELUP]   = false;
    mousebuttons[MOUSE_WHEELDOWN] = false;

    return G_HandleEvent(ev);
}

//
// G_ClearInput
// Clears input state - used when loading levels
//
void G_ClearInput(void)
{
    memset(game.keydown, 0, sizeof(game.keydown));
    mousex    = 0;
    mousey    = 0;
    sendpause = false;
    sendsave  = false;
    memset(mousearray, 0, sizeof(mousearray));
}
