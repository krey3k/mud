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

#include "playsim/p_bsp.h"

#include "console/c_console.h"
#include "doom/doomstat.h"
#include "doom/doomtype.h"
#include "math/math_bbox.h"
#include "math/math_fixed.h"
#include "playsim/p_local.h"
#include "system/i_system.h"
#include "system/i_timer.h"
#include "utils/m_misc.h"
#include "utils/z_zone.h"

#define DIST_EPSILON (FRACUNIT / 64)

// this value is a trade-off.  lower values will build nodes faster,
// but higher values allow picking better BSP partitions (and hence
// produce better BSP trees).
#define FAST_THRESHOLD 128

// reducing splits is important to get good BSP trees.  making this
// value really low produces trees which have a *lot* more nodes
// (I am not sure exactly why).  higher values are okay.
#define SPLIT_COST 11

// maximum BSP tree depth - should be enough for any reasonable map.
// using explicit stack instead of recursion to prevent stack overflow.
#define MAX_BSP_DEPTH 256

typedef struct Nanode nanode_t;

struct Nanode
{
    // when non-NULL, this is actually a leaf of the BSP tree
    seg_t* segs;

    // final index number of this node / leaf
    int index;

    // partition line (start coord, delta to end)
    fixed_t x, y, dx, dy;

    // right and left children
    struct Nanode* right;
    struct Nanode* left;

    // temporary storage for child results during iterative write
    unsigned int children_result[2];
};


vertex_t* BSP_NewVertex(fixed_t x, fixed_t y)
{
    vertex_t* vert = Z_Malloc(sizeof(vertex_t), PU_NANOBSP, NULL);
    vert->x        = x;
    vert->y        = y;
    return vert;
}

seg_t* BSP_NewSeg(void)
{
    seg_t* seg = Z_Malloc(sizeof(seg_t), PU_NANOBSP, NULL);
    memset(seg, 0, sizeof(*seg));
    return seg;
}

nanode_t* BSP_NewNode(void)
{
    nanode_t* node = Z_Malloc(sizeof(nanode_t), PU_NANOBSP, NULL);
    memset(node, 0, sizeof(*node));
    return node;
}

//----------------------------------------------------------------------------

void BSP_CalcOffset(seg_t* seg)
{
    line_t* ld = seg->linedef;

    // compute which side of the linedef the seg is on
    int side;

    if(abs(ld->dx) > abs(ld->dy))
        side = ((ld->dx < 0) == (seg->v2->x - seg->v1->x < 0)) ? 0 : 1;
    else
        side = ((ld->dy < 0) == (seg->v2->y - seg->v1->y < 0)) ? 0 : 1;

    seg->offset = GetOffset(seg->v1, side ? ld->v2 : ld->v1);
}

void BSP_BoundingBox(seg_t* soup, fixed_t* bbox)
{
    bbox[BOXLEFT]   = INT_MAX;
    bbox[BOXRIGHT]  = INT_MIN;
    bbox[BOXBOTTOM] = INT_MAX;
    bbox[BOXTOP]    = INT_MIN;

    seg_t* S;
    for(S = soup; S != NULL; S = S->next)
    {
        bbox[BOXLEFT]   = MIN(bbox[BOXLEFT], S->v1->x);
        bbox[BOXLEFT]   = MIN(bbox[BOXLEFT], S->v2->x);
        bbox[BOXBOTTOM] = MIN(bbox[BOXBOTTOM], S->v1->y);
        bbox[BOXBOTTOM] = MIN(bbox[BOXBOTTOM], S->v2->y);

        bbox[BOXRIGHT] = MAX(bbox[BOXRIGHT], S->v1->x);
        bbox[BOXRIGHT] = MAX(bbox[BOXRIGHT], S->v2->x);
        bbox[BOXTOP]   = MAX(bbox[BOXTOP], S->v1->y);
        bbox[BOXTOP]   = MAX(bbox[BOXTOP], S->v2->y);
    }
}

void BSP_MergeBounds(fixed_t* out, fixed_t* box1, fixed_t* box2)
{
    out[BOXLEFT]   = MIN(box1[BOXLEFT], box2[BOXLEFT]);
    out[BOXBOTTOM] = MIN(box1[BOXBOTTOM], box2[BOXBOTTOM]);
    out[BOXRIGHT]  = MAX(box1[BOXRIGHT], box2[BOXRIGHT]);
    out[BOXTOP]    = MAX(box1[BOXTOP], box2[BOXTOP]);
}

void BSP_SegForLineSide(int i, int side, seg_t** list_var)
{
    line_t* ld = &lines[i];

    if(ld->sidenum[side] == NO_INDEX) // [FG]
        return;

    seg_t* seg = BSP_NewSeg();

    seg->v1 = side ? ld->v2 : ld->v1;
    seg->v2 = side ? ld->v1 : ld->v2;

    seg->sidedef = &sides[ld->sidenum[side]];
    seg->linedef = ld;

    seg->angle = R_PointToAngle2(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y);

    seg->frontsector = side ? ld->backsector : ld->frontsector;
    seg->backsector  = side ? ld->frontsector : ld->backsector;

    BSP_CalcOffset(seg);

    // link into the list
    seg->next   = (*list_var);
    (*list_var) = seg;
}

seg_t* BSP_CreateSegs(void)
{
    seg_t* list = NULL;

    int i;
    for(i = 0; i < numlines; i++)
    {
        BSP_SegForLineSide(i, 0, &list);
        BSP_SegForLineSide(i, 1, &list);
    }

    return list;
}

nanode_t* BSP_CreateLeaf(seg_t* soup)
{
    nanode_t* node = BSP_NewNode();

    node->segs = soup;

    return node;
}

//----------------------------------------------------------------------------

struct NodeEval
{
    int left, right, split;
};

int BSP_PointOnSide(seg_t* part, fixed_t x, fixed_t y)
{
    x -= part->v1->x;
    y -= part->v1->y;

    fixed_t dx = part->v2->x - part->v1->x;
    fixed_t dy = part->v2->y - part->v1->y;

    if(dx == 0)
    {
        if(x < -DIST_EPSILON)
            return (dy < 0) ? +1 : -1;

        if(x > +DIST_EPSILON)
            return (dy > 0) ? +1 : -1;

        return 0;
    }

    if(dy == 0)
    {
        if(y < -DIST_EPSILON)
            return (dx > 0) ? +1 : -1;

        if(y > +DIST_EPSILON)
            return (dx < 0) ? +1 : -1;

        return 0;
    }

    // note that we compute the distance to the partition along an axis
    // (rather than perpendicular to it), which can give values smaller
    // than the true distance.  for our purposes, that is okay.

    if(abs(dx) >= abs(dy))
    {
        fixed_t slope = FixedDiv(dy, dx);

        y -= FixedMul(x, slope);

        if(y < -DIST_EPSILON)
            return (dx > 0) ? +1 : -1;

        if(y > +DIST_EPSILON)
            return (dx < 0) ? +1 : -1;
    }
    else
    {
        fixed_t slope = FixedDiv(dx, dy);

        x -= FixedMul(y, slope);

        if(x < -DIST_EPSILON)
            return (dy < 0) ? +1 : -1;

        if(x > +DIST_EPSILON)
            return (dy > 0) ? +1 : -1;
    }

    return 0;
}

bool BSP_SameDirection(seg_t* part, seg_t* seg)
{
    fixed_t pdx = part->v2->x - part->v1->x;
    fixed_t pdy = part->v2->y - part->v1->y;

    fixed_t sdx = seg->v2->x - seg->v1->x;
    fixed_t sdy = seg->v2->y - seg->v1->y;

    int64_t n = (int64_t)sdx * (int64_t)pdx + (int64_t)sdy * (int64_t)pdy;

    return (n > 0);
}

int BSP_SegOnSide(seg_t* part, seg_t* seg)
{
    if(seg == part)
        return +1;

    int side1 = BSP_PointOnSide(part, seg->v1->x, seg->v1->y);
    int side2 = BSP_PointOnSide(part, seg->v2->x, seg->v2->y);

    // colinear?
    if(side1 == 0 && side2 == 0)
        return BSP_SameDirection(part, seg) ? +1 : -1;

    // splits the seg?
    if((side1 * side2) < 0)
        return 0;

    return (side1 >= 0 && side2 >= 0) ? +1 : -1;
}

//
// Evaluate a seg as a partition candidate, storing the results in `eval`.
// returns true if the partition is viable, false otherwise.
//
bool BSP_EvalPartition(seg_t* part, seg_t* soup, struct NodeEval* eval)
{
    eval->left  = 0;
    eval->right = 0;
    eval->split = 0;

    // do not create tiny partitions
    if(abs(part->v2->x - part->v1->x) < 4 * DIST_EPSILON &&
    abs(part->v2->y - part->v1->y) < 4 * DIST_EPSILON)
        return false;

    seg_t* S;
    for(S = soup; S != NULL; S = S->next)
    {
        int side = BSP_SegOnSide(part, S);

        switch(side)
        {
        case 0:
            eval->split += 1;
            break;
        case -1:
            eval->left += 1;
            break;
        case +1:
            eval->right += 1;
            break;
        }
    }

    // a viable partition either splits something, or has other segs
    // lying on *both* the left and right sides.

    return (eval->split > 0 || (eval->left > 0 && eval->right > 0));
}

//
// Look for an axis-aligned seg which can divide the other segs
// in a "nice" way.  returns NULL if none found.
//
seg_t* BSP_PickNode_Fast(seg_t* soup)
{
    // use slower method when number of segs is below a threshold
    int count = 0;

    seg_t* S;
    for(S = soup; S != NULL; S = S->next)
        count += 1;

    if(count < FAST_THRESHOLD)
        return NULL;

    // determine bounding box of the segs
    fixed_t bbox[4];

    BSP_BoundingBox(soup, bbox);

    fixed_t mid_x = bbox[BOXLEFT] / 2 + bbox[BOXRIGHT] / 2;
    fixed_t mid_y = bbox[BOXBOTTOM] / 2 + bbox[BOXTOP] / 2;

    seg_t* vert_part  = NULL;
    fixed_t vert_dist = (1 << 30);

    seg_t* horiz_part  = NULL;
    fixed_t horiz_dist = (1 << 30);

    // find the seg closest to the middle of the bbox
    seg_t* part;
    for(part = soup; part != NULL; part = part->next)
    {
        if(part->v1->x == part->v2->x)
        {
            fixed_t dist = abs(part->v1->x - mid_x);

            if(dist < vert_dist)
            {
                vert_part = part;
                vert_dist = dist;
            }
        }
        else if(part->v1->y == part->v2->y)
        {
            fixed_t dist = abs(part->v1->y - mid_y);

            if(dist < horiz_dist)
            {
                horiz_part = part;
                horiz_dist = dist;
            }
        }
    }

    // check that each partition is viable
    struct NodeEval v_eval;
    struct NodeEval h_eval;

    bool vert_ok = (vert_part != NULL) && BSP_EvalPartition(vert_part, soup, &v_eval);
    bool horiz_ok = (horiz_part != NULL) && BSP_EvalPartition(horiz_part, soup, &h_eval);

    if(vert_ok && horiz_ok)
    {
        int vert_cost = abs(v_eval.left - v_eval.right) * 2 + v_eval.split * SPLIT_COST;
        int horiz_cost = abs(h_eval.left - h_eval.right) * 2 + h_eval.split * SPLIT_COST;

        return (horiz_cost < vert_cost) ? horiz_part : vert_part;
    }

    if(vert_ok)
        return vert_part;
    if(horiz_ok)
        return horiz_part;

    return NULL;
}

//
// Evaluate *every* seg in the list as a partition candidate,
// returning the best one, or NULL if none found (which means
// the remaining segs form a subsector).
//
seg_t* BSP_PickNode_Slow(seg_t* soup)
{
    seg_t* part;
    seg_t* best   = NULL;
    int best_cost = (1 << 30);

    for(part = soup; part != NULL; part = part->next)
    {
        struct NodeEval eval;

        if(BSP_EvalPartition(part, soup, &eval))
        {
            int cost = abs(eval.left - eval.right) * 2 + eval.split * SPLIT_COST;

            if(cost < best_cost)
            {
                best      = part;
                best_cost = cost;
            }
        }
    }

    return best;
}

//----------------------------------------------------------------------------

void BSP_ComputeIntersection(seg_t* part, seg_t* seg, fixed_t* x, fixed_t* y)
{
    fixed_t a, b;

    if(part->v1->x == part->v2->x)
    {
        // vertical partition

        if(seg->v1->y == seg->v2->y)
        {
            // horizontal seg
            *x = part->v1->x;
            *y = seg->v1->y;
            return;
        }

        a = abs(seg->v1->x - part->v1->x);
        b = abs(seg->v2->x - part->v1->x);
    }
    else if(part->v1->y == part->v2->y)
    {
        // horizontal partition

        if(seg->v1->x == seg->v2->x)
        {
            // vertical seg
            *x = seg->v1->x;
            *y = part->v1->y;
            return;
        }

        a = abs(seg->v1->y - part->v1->y);
        b = abs(seg->v2->y - part->v1->y);
    }
    else
    {
        fixed_t dx = part->v2->x - part->v1->x;
        fixed_t dy = part->v2->y - part->v1->y;

        // compute seg coords relative to partition start
        fixed_t x1 = seg->v1->x - part->v1->x;
        fixed_t y1 = seg->v1->y - part->v1->y;

        fixed_t x2 = seg->v2->x - part->v1->x;
        fixed_t y2 = seg->v2->y - part->v1->y;

        if(abs(dx) >= abs(dy))
        {
            fixed_t slope = FixedDiv(dy, dx);

            a = abs(y1 - FixedMul(x1, slope));
            b = abs(y2 - FixedMul(x2, slope));
        }
        else
        {
            fixed_t slope = FixedDiv(dx, dy);

            a = abs(x1 - FixedMul(y1, slope));
            b = abs(x2 - FixedMul(y2, slope));
        }
    }

    // this is higher precision: 2.30 instead of 16.16
    fixed_t along = ((int64_t)a << 30) / (int64_t)(a + b);

    if(seg->v1->x == seg->v2->x)
        *x = seg->v1->x;
    else
        *x = seg->v1->x + (((int64_t)(seg->v2->x - seg->v1->x) * (int64_t)along) >> 30);

    if(seg->v1->y == seg->v2->y)
        *y = seg->v1->y;
    else
        *y = seg->v1->y + (((int64_t)(seg->v2->y - seg->v1->y) * (int64_t)along) >> 30);
}

//
// For segs not intersecting the partition, just move them into the
// correct output list (`lefts` or `rights`).  otherwise split the seg
// at the intersection point, one piece goes left, the other right.
//
void BSP_SplitSegs(seg_t* part, seg_t* soup, seg_t** lefts, seg_t** rights)
{
    while(soup != NULL)
    {
        seg_t* S = soup;
        soup     = soup->next;

        int where = BSP_SegOnSide(part, S);

        if(where < 0)
        {
            S->next  = (*lefts);
            (*lefts) = S;
            continue;
        }

        if(where > 0)
        {
            S->next   = (*rights);
            (*rights) = S;
            continue;
        }

        // we must split this seg
        fixed_t ix, iy;

        BSP_ComputeIntersection(part, S, &ix, &iy);

        vertex_t* iv = BSP_NewVertex(ix, iy);

        seg_t* T = BSP_NewSeg();

        T->v2 = S->v2;
        T->v1 = iv;
        S->v2 = iv;

        T->angle   = S->angle;
        T->sidedef = S->sidedef;
        T->linedef = S->linedef;

        T->frontsector = S->frontsector;
        T->backsector  = S->backsector;

        // compute offsets for the split pieces
        BSP_CalcOffset(T);
        BSP_CalcOffset(S);

        if(BSP_PointOnSide(part, S->v1->x, S->v1->y) < 0)
        {
            S->next  = (*lefts);
            (*lefts) = S;

            T->next   = (*rights);
            (*rights) = T;
        }
        else
        {
            S->next   = (*rights);
            (*rights) = S;

            T->next  = (*lefts);
            (*lefts) = T;
        }
    }
}

// Stack frame for iterative BSP subdivision
typedef struct
{
    nanode_t* node;    // the node we're building
    seg_t* soup;       // seg list to process (only valid in state 0)
    int state;         // 0=initial, 1=right done, 2=left done
} SubdivideFrame;

//
// Iterative version of BSP tree construction.
// Uses an explicit stack to avoid call stack overflow on complex maps.
//
nanode_t* BSP_SubdivideSegs_Iterative(seg_t* initial_soup)
{
    SubdivideFrame stack[MAX_BSP_DEPTH];
    int sp = 0;  // stack pointer

    nanode_t* root = NULL;
    nanode_t* result = NULL;  // result from "returning" up the stack

    // Push initial work
    stack[sp].node = NULL;
    stack[sp].soup = initial_soup;
    stack[sp].state = 0;

    while(sp >= 0)
    {
        SubdivideFrame* frame = &stack[sp];

        if(frame->state == 0)
        {
            // Initial state: process this seg soup
            seg_t* soup = frame->soup;
            seg_t* part = BSP_PickNode_Fast(soup);

            if(part == NULL)
                part = BSP_PickNode_Slow(soup);

            if(part == NULL)
            {
                // This is a leaf - "return" it
                result = BSP_CreateLeaf(soup);
                if(sp == 0)
                    root = result;
                sp--;
                continue;
            }

            // Create the node
            nanode_t* N = BSP_NewNode();

            N->x  = part->v1->x;
            N->y  = part->v1->y;
            N->dx = part->v2->x - N->x;
            N->dy = part->v2->y - N->y;

            // ensure partitions are a minimum length
            fixed_t min_size = 64 * FRACUNIT;
            while(abs(N->dx) < min_size && abs(N->dy) < min_size)
            {
                N->dx *= 2;
                N->dy *= 2;
            }

            seg_t* lefts  = NULL;
            seg_t* rights = NULL;
            BSP_SplitSegs(part, soup, &lefts, &rights);

            frame->node = N;
            frame->state = 1;

            // Push right child work
            sp++;
            if(sp >= MAX_BSP_DEPTH)
                I_Error("BSP_SubdivideSegs: tree depth exceeded %d", MAX_BSP_DEPTH);

            stack[sp].node = NULL;
            stack[sp].soup = rights;
            stack[sp].state = 0;

            // Store lefts in the parent frame's soup field for later
            frame->soup = lefts;
        }
        else if(frame->state == 1)
        {
            // Right child is done, result is in 'result'
            frame->node->right = result;
            frame->state = 2;

            // Push left child work
            sp++;
            if(sp >= MAX_BSP_DEPTH)
                I_Error("BSP_SubdivideSegs: tree depth exceeded %d", MAX_BSP_DEPTH);

            stack[sp].node = NULL;
            stack[sp].soup = frame->soup;  // lefts stored here earlier
            stack[sp].state = 0;
        }
        else // state == 2
        {
            // Left child is done
            frame->node->left = result;
            result = frame->node;
            if(sp == 0)
                root = result;
            sp--;
        }
    }

    return root;
}

//----------------------------------------------------------------------------

//
// Iterative post-order traversal to count nodes, subsectors, and segs.
// Assigns indices such that root node gets the highest index.
//
typedef struct
{
    nanode_t* node;
    int state;  // 0=visit, 1=left done, 2=right done
} CountFrame;

void BSP_CountStuff_Iterative(nanode_t* root)
{
    CountFrame stack[MAX_BSP_DEPTH];
    int sp = 0;

    stack[sp].node = root;
    stack[sp].state = 0;

    while(sp >= 0)
    {
        CountFrame* frame = &stack[sp];
        nanode_t* N = frame->node;

        if(N->segs != NULL)
        {
            // Leaf node
            N->index = numsubsectors;
            numsubsectors += 1;

            seg_t* seg;
            for(seg = N->segs; seg != NULL; seg = seg->next)
                numsegs += 1;

            sp--;
        }
        else if(frame->state == 0)
        {
            // First visit - go left
            frame->state = 1;

            sp++;
            if(sp >= MAX_BSP_DEPTH)
                I_Error("BSP_CountStuff: tree depth exceeded %d", MAX_BSP_DEPTH);

            stack[sp].node = N->left;
            stack[sp].state = 0;
        }
        else if(frame->state == 1)
        {
            // Left done - go right
            frame->state = 2;

            sp++;
            if(sp >= MAX_BSP_DEPTH)
                I_Error("BSP_CountStuff: tree depth exceeded %d", MAX_BSP_DEPTH);

            stack[sp].node = N->right;
            stack[sp].state = 0;
        }
        else
        {
            // Both children done - assign index to this node
            N->index = numnodes;
            numnodes += 1;

            sp--;
        }
    }
}

//----------------------------------------------------------------------------

//
// Iterative post-order traversal to write nodes and subsectors.
// Also computes bounding boxes and frees nodes as it goes.
//
typedef struct
{
    nanode_t* node;
    fixed_t* bbox;      // where to store this node's bbox
    int state;          // 0=visit, 1=right done, 2=left done
} WriteFrame;

// Forward declaration
static void BSP_WriteSubsector(nanode_t* N);

unsigned int BSP_WriteNode_Iterative(nanode_t* root, fixed_t* root_bbox)
{
    WriteFrame stack[MAX_BSP_DEPTH];
    int sp = 0;

    unsigned int final_result = 0;

    stack[sp].node = root;
    stack[sp].bbox = root_bbox;
    stack[sp].state = 0;

    while(sp >= 0)
    {
        WriteFrame* frame = &stack[sp];
        nanode_t* N = frame->node;

        if(N->segs != NULL)
        {
            // Leaf node
            unsigned int index = N->index | NF_SUBSECTOR;

            BSP_BoundingBox(N->segs, frame->bbox);
            BSP_WriteSubsector(N);
            Z_Free(N);

            if(sp == 0)
            {
                final_result = index;
            }
            else
            {
                // Store result in parent's children_result
                // state 1 means we just finished right child (index 0)
                // state 2 means we just finished left child (index 1)
                int child_idx = stack[sp - 1].state - 1;
                stack[sp - 1].node->children_result[child_idx] = index;
            }

            sp--;
        }
        else if(frame->state == 0)
        {
            // First visit - set up node and go to right child
            node_t* out = &nodes[N->index];

            out->x  = N->x;
            out->y  = N->y;
            out->dx = N->dx;
            out->dy = N->dy;

            frame->state = 1;

            sp++;
            if(sp >= MAX_BSP_DEPTH)
                I_Error("BSP_WriteNode: tree depth exceeded %d", MAX_BSP_DEPTH);

            stack[sp].node = N->right;
            stack[sp].bbox = out->bbox[0];
            stack[sp].state = 0;
        }
        else if(frame->state == 1)
        {
            // Right child done - go to left child
            node_t* out = &nodes[N->index];

            frame->state = 2;

            sp++;
            if(sp >= MAX_BSP_DEPTH)
                I_Error("BSP_WriteNode: tree depth exceeded %d", MAX_BSP_DEPTH);

            stack[sp].node = N->left;
            stack[sp].bbox = out->bbox[1];
            stack[sp].state = 0;
        }
        else
        {
            // Both children done - finalize this node
            node_t* out = &nodes[N->index];

            // Copy children results to output node
            out->children[0] = N->children_result[0];  // right child
            out->children[1] = N->children_result[1];  // left child

            BSP_MergeBounds(frame->bbox, out->bbox[0], out->bbox[1]);

            unsigned int index = N->index;
            Z_Free(N);

            if(sp == 0)
            {
                final_result = index;
            }
            else
            {
                int child_idx = stack[sp - 1].state - 1;
                stack[sp - 1].node->children_result[child_idx] = index;
            }

            sp--;
        }
    }

    return final_result;
}

static int nano_seg_index;

static void BSP_WriteSubsector(nanode_t* N)
{
    subsector_t* out = &subsectors[N->index];

    out->numlines  = 0;
    out->firstline = nano_seg_index;
    out->sector    = NULL; // determined in P_GroupLines

    seg_t* seg;

    while(N->segs != NULL)
    {
        seg = N->segs;

        // unlink this seg from the list
        N->segs   = seg->next;
        seg->next = NULL;

        // copy and free it
        memcpy(&segs[nano_seg_index], seg, sizeof(seg_t));

        Z_Free(seg);

        nano_seg_index += 1;
        out->numlines += 1;
    }
}

void BSP_BuildNodes(void)
{
    int64_t start_time = I_GetTimeMS();

    seg_t* list = BSP_CreateSegs();

    nanode_t* root = BSP_SubdivideSegs_Iterative(list);

    // determine total number of nodes, subsectors and segs
    numnodes      = 0;
    numsubsectors = 0;
    numsegs       = 0;

    BSP_CountStuff_Iterative(root);

    // allocate the global arrays
    nodes      = Z_Malloc(numnodes * sizeof(node_t), PU_NANOBSP, NULL);
    subsectors = Z_Malloc(numsubsectors * sizeof(subsector_t), PU_NANOBSP, NULL);
    segs       = Z_Malloc(numsegs * sizeof(seg_t), PU_NANOBSP, NULL);

    // clear the initial contents
    if (numnodes > 0)
        memset(nodes, 0, numnodes * sizeof(node_t));
    if (numsubsectors > 0)
        memset(subsectors, 0, numsubsectors * sizeof(subsector_t));

    nano_seg_index = 0;

    fixed_t dummy[4];

    // this also frees stuff as it goes
    BSP_WriteNode_Iterative(root, dummy);

    int64_t elapsed = I_GetTimeMS() - start_time;
    C_Output("NanoBSP: Built %d nodes, %d subsectors, %d segs in %lld ms",
    numnodes, numsubsectors, numsegs, elapsed);
}
