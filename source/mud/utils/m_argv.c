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

#include "sokol_args.h"

bool M_CheckParm(const char* check)
{
    return sargs_exists(check);
}

const char *M_GetParm(const char* check)
{
    return sargs_value(check);
}

const char *M_GetParms(const char* check1, const char* check2, const char* check3)
{
    const char *value = NULL;
    if (check1) value = sargs_value(check1);
    if (value && *value != '\0') return value;
    if (check2) value = sargs_value(check2);
    if (value && *value != '\0') return value;
    // if we get here, return the last check
    // unconditionally
    value = check3 ? sargs_value(check3) : "";
    return value;
}   

