//----------------------------------------------------------------------------
//
//  Copyright (c) 2023 Andrew Apted
//
//  Permission is hereby granted, free of charge, to any person obtaining
//  a copy of this software and associated documentation files (the
//  "Software"), to deal in the Software without restriction, including
//  without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to
//  permit persons to whom the Software is furnished to do so, subject to
//  the following conditions:
//
//  The above copyright notice and this permission notice shall be
//  included in all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
//  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
//  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//----------------------------------------------------------------------------

#pragma once

#include "render/r_defs.h"

#include <math.h>

// Used during both node construction and level load
static inline fixed_t GetOffset(const vertex_t* v1, const vertex_t* v2)
{
    const fixed_t dx = (v1->x - v2->x) >> FRACBITS;
    const fixed_t dy = (v1->y - v2->y) >> FRACBITS;

    return ((fixed_t)(sqrt((double)dx * dx + (double)dy * dy)) << FRACBITS);
}

void BSP_BuildNodes(void);
