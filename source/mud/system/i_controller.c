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

#include "system/i_controller.h"
#include "console/c_console.h"
#include "doom/doomstat.h"
#include "system/i_input.h"
#include "system/i_config.h"
#include "utils/m_misc.h"

#include "minigamepad.h"


static bool controllerrumbles = false;

int controllerbuttons   = 0;
short controllerthumbLX = 0;
short controllerthumbLY = 0;
short controllerthumbRX = 0;
short controllerthumbRY = 0;
float controllerhorizontalsensitivity;
float controllerverticalsensitivity;
short controllerleftdeadzone;
short controllerrightdeadzone;

const float GAMEPAD_AXIS_MAX = 32767.0f;


int barrelrumbletics = 0;
int damagerumbletics = 0;
int pickuprumbletics = 0;
int weaponrumbletics = 0;
int idlechainsawrumblestrength;
int restoredrumblestrength;

char* selectbutton = "A";

static mg_gamepads gamepads;

// Process analog stick with deadzone and cubic response curve
static void ProcessAnalogStick(float x, float y, short deadzone, short* outX, short* outY)
{
    if(!outX || !outY)
        return;

    float magnitude = sqrtf(x * x + y * y);

    if(magnitude > deadzone)
    {
        // Clamp magnitude to max range
        if(magnitude > GAMEPAD_AXIS_MAX)
            magnitude = GAMEPAD_AXIS_MAX;

        // Apply deadzone and normalize
        magnitude = (magnitude - deadzone) / (GAMEPAD_AXIS_MAX - deadzone);

        // Apply cubic curve for finer control at low speeds
        float normalizedmagnitude = powf(magnitude, 3.0f);

        // Calculate output values preserving direction
        *outX = (short)(normalizedmagnitude * x / magnitude);
        *outY = (short)(normalizedmagnitude * y / magnitude);
    }
    else
    {
        // Below deadzone - zero output
        *outX = 0;
        *outY = 0;
    }
}

static char* GetControllerName(void)
{
    mg_gamepad* gamepad = gamepads.list.head;

    if(gamepad && gamepad->name[0])
        return M_StringJoin("A controller called \"", gamepad->name, "\" is connected.", NULL);
    else
        return "A controller is connected.";
}

void I_InitController(void)
{
    mg_gamepads_init(&gamepads);

    I_SetControllerLeftDeadZone();
    I_SetControllerRightDeadZone();
    I_SetControllerHorizontalSensitivity();
    I_SetControllerVerticalSensitivity();
}

void I_ShutdownController(void)
{
    mg_gamepads_free(&gamepads);

    C_Warning(1, "The controller was disconnected!");
}

void I_ControllerRumble(const short low, const short high)
{
    if(!controllerrumbles || !usingcontroller)
        return;
}

void I_ReadController(void)
{
    static int prevcontrollerbuttons;

    prevcontrollerbuttons = controllerbuttons;
    controllerbuttons     = 0;

    mg_gamepad* gamepad = gamepads.list.head;

    if(gamepad)
    {
        // Update gamepad state from hardware
        mg_gamepads_update(&gamepads, NULL);

        // Map face buttons (A, B, X, Y)
        if(gamepad->buttons[MG_BUTTON_SOUTH].current)
            controllerbuttons |= CONTROLLER_A;
        if(gamepad->buttons[MG_BUTTON_EAST].current)
            controllerbuttons |= CONTROLLER_B;
        if(gamepad->buttons[MG_BUTTON_WEST].current)
            controllerbuttons |= CONTROLLER_X;
        if(gamepad->buttons[MG_BUTTON_NORTH].current)
            controllerbuttons |= CONTROLLER_Y;

        // Map system buttons
        if(gamepad->buttons[MG_BUTTON_BACK].current)
            controllerbuttons |= CONTROLLER_BACK;
        if(gamepad->buttons[MG_BUTTON_GUIDE].current)
            controllerbuttons |= CONTROLLER_GUIDE;
        if(gamepad->buttons[MG_BUTTON_START].current)
            controllerbuttons |= CONTROLLER_START;

        // Map thumbstick buttons
        if(gamepad->buttons[MG_BUTTON_LEFT_STICK].current)
            controllerbuttons |= CONTROLLER_LEFT_THUMB;
        if(gamepad->buttons[MG_BUTTON_RIGHT_STICK].current)
            controllerbuttons |= CONTROLLER_RIGHT_THUMB;

        // Map shoulder buttons
        if(gamepad->buttons[MG_BUTTON_LEFT_SHOULDER].current)
            controllerbuttons |= CONTROLLER_LEFT_SHOULDER;
        if(gamepad->buttons[MG_BUTTON_RIGHT_SHOULDER].current)
            controllerbuttons |= CONTROLLER_RIGHT_SHOULDER;

        // Map D-pad buttons
        if(gamepad->buttons[MG_BUTTON_DPAD_UP].current)
            controllerbuttons |= CONTROLLER_DPAD_UP;
        if(gamepad->buttons[MG_BUTTON_DPAD_DOWN].current)
            controllerbuttons |= CONTROLLER_DPAD_DOWN;
        if(gamepad->buttons[MG_BUTTON_DPAD_LEFT].current)
            controllerbuttons |= CONTROLLER_DPAD_LEFT;
        if(gamepad->buttons[MG_BUTTON_DPAD_RIGHT].current)
            controllerbuttons |= CONTROLLER_DPAD_RIGHT;

        // Map trigger buttons
        if(gamepad->buttons[MG_BUTTON_LEFT_TRIGGER].current)
            controllerbuttons |= CONTROLLER_LEFT_TRIGGER;
        if(gamepad->buttons[MG_BUTTON_RIGHT_TRIGGER].current)
            controllerbuttons |= CONTROLLER_RIGHT_TRIGGER;

        // Read analog stick axes (convert from normalized [-1,1] to SDL range)
        float lx = gamepad->axes[MG_AXIS_LEFT_X].value * GAMEPAD_AXIS_MAX;
        float ly = gamepad->axes[MG_AXIS_LEFT_Y].value * GAMEPAD_AXIS_MAX;

        float rx = gamepad->axes[MG_AXIS_RIGHT_X].value * GAMEPAD_AXIS_MAX;
        float ry = gamepad->axes[MG_AXIS_RIGHT_Y].value * GAMEPAD_AXIS_MAX;

        // Swap thumbsticks if configured (for left-handed players)
        if(joy_swapthumbsticks)
        {
            float temp;
            temp = lx;
            lx   = rx;
            rx   = temp;
            temp = ly;
            ly   = ry;
            ry   = temp;
        }

        // Process analog sticks with appropriate mode
        if(joy_analog)
        {
            // Analog mode: apply deadzone and cubic response curve for smooth control
            ProcessAnalogStick(lx, ly, controllerleftdeadzone, &controllerthumbLX, &controllerthumbLY);
            ProcessAnalogStick(rx, ry, controllerrightdeadzone, &controllerthumbRX, &controllerthumbRY);
        }
        else
        {
            // Digital mode: treat sticks as binary directional inputs
            controllerthumbLX =
            (ABS(lx) > controllerleftdeadzone ? SIGN(lx) * GAMEPAD_AXIS_MAX : 0);
            controllerthumbLY =
            (ABS(ly) > controllerleftdeadzone ? SIGN(ly) * GAMEPAD_AXIS_MAX : 0);
            controllerthumbRX =
            (ABS(rx) > controllerrightdeadzone ? SIGN(rx) * GAMEPAD_AXIS_MAX : 0);
            controllerthumbRY =
            (ABS(ry) > controllerrightdeadzone ? SIGN(ry) * GAMEPAD_AXIS_MAX : 0);
        }
    }
    else
    {
        controllerthumbLX = 0;
        controllerthumbLY = 0;
        controllerthumbRX = 0;
        controllerthumbRY = 0;
    }

    // Post controller event if any input detected
    if(controllerthumbLX || controllerthumbLY || controllerthumbRX ||
    controllerthumbRY || controllerbuttons != prevcontrollerbuttons)
    {
        event_t ev = { ev_controller, 0, 0, 0 };

        // Switch from mouse to controller when controller input detected
        if(game.state != GS_LEVEL && usingmouse)
        {
            I_SaveMousePointerPosition();
            usingmouse = false;
        }

        keydown         = 0;
        usingcontroller = true;
        D_PostEvent(&ev);
    }
}

void I_StopControllerRumble(void)
{
    if(!controllerrumbles)
        return;
}

void I_SetControllerHorizontalSensitivity(void)
{
    controllerhorizontalsensitivity =
    2.0f * joy_sensitivity_horizontal / joy_sensitivity_horizontal_max;
}

void I_SetControllerVerticalSensitivity(void)
{
    controllerverticalsensitivity = 2.0f * joy_sensitivity_vertical / joy_sensitivity_vertical_max;
}

void I_SetControllerLeftDeadZone(void)
{

    controllerleftdeadzone = (short)(joy_deadzone_left * GAMEPAD_AXIS_MAX / 100.0f);
}

void I_SetControllerRightDeadZone(void)
{
    controllerrightdeadzone = (short)(joy_deadzone_right * GAMEPAD_AXIS_MAX / 100.0f);
}
