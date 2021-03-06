//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2019 Julian Nechaevsky
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Rendering main loop and setup functions,
//	 utility functions (BSP, geometry, trigonometry).
//	See tables.c, too.
//



#include <stdlib.h>
#include <math.h>


#include "doomdef.h"
#include "doomstat.h" // [AM] leveltime, paused, menuactive
#include "d_loop.h"
#include "m_bbox.h"
#include "m_menu.h"
#include "p_local.h"
#include "r_local.h"
#include "r_sky.h"
#include "g_game.h"
#include "crispy.h"
#include "jn.h"


// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW     2048	

int viewangleoffset;

// increment every time a check is made
int validcount = 1;		

lighttable_t*           fixedcolormap;
extern lighttable_t**   walllights;

int centerx;
int centery;

fixed_t centerxfrac;
fixed_t centeryfrac;
fixed_t projection;

// just for profiling purposes
int framecount;	

int sscount;
int linecount;
int loopcount;

fixed_t viewx;
fixed_t viewy;
fixed_t viewz;

angle_t viewangle;

fixed_t viewcos;
fixed_t viewsin;

player_t* viewplayer;

// 0 = high, 1 = low
int detailshift;	

//
// precalculated math tables
//
angle_t clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X. 
int viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t xtoviewangle[WIDESCREENWIDTH+1];

lighttable_t* scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* scalelightfixed[MAXLIGHTSCALE];
lighttable_t* zlight[LIGHTLEVELS][MAXLIGHTZ];

// [JN] Floor brightmaps
lighttable_t* fullbright_notgrayorbrown_floor[LIGHTLEVELS][MAXLIGHTZ];
lighttable_t* fullbright_orangeyellow_floor[LIGHTLEVELS][MAXLIGHTZ];

// [JN] Wall and sprite brightmaps
lighttable_t* fullbright_redonly[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_notgray[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_notgrayorbrown[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_greenonly1[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_greenonly2[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_greenonly3[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_orangeyellow[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_dimmeditems[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_brighttan[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_redonly1[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_explosivebarrel[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_alllights[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_candles[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_pileofskulls[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t* fullbright_redonly2[LIGHTLEVELS][MAXLIGHTSCALE];

// bumped light from gun blasts
int extralight;			


void (*colfunc) (void);
void (*basecolfunc) (void);
void (*fuzzcolfunc) (void);
void (*transcolfunc) (void);
void (*tlcolfunc) (void);
void (*spanfunc) (void);


//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
void R_AddPointToBox (int x, int y, fixed_t* box)
{
    if (x< box[BOXLEFT])
    box[BOXLEFT] = x;
    if (x> box[BOXRIGHT])
    box[BOXRIGHT] = x;
    if (y< box[BOXBOTTOM])
    box[BOXBOTTOM] = y;
    if (y> box[BOXTOP])
    box[BOXTOP] = y;
}


//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int R_PointOnSide (fixed_t x, fixed_t y, node_t* node)
{
    fixed_t dx;
    fixed_t dy;
    fixed_t left;
    fixed_t right;

    if (!node->dx)
    {
        if (x <= node->x)
        return node->dy > 0;

        return node->dy < 0;
    }
    if (!node->dy)
    {
        if (y <= node->y)
        return node->dx < 0;

        return node->dx > 0;
    }

    dx = (x - node->x);
    dy = (y - node->y);

    // Try to quickly decide by looking at sign bits.
    if ((node->dy ^ node->dx ^ dx ^ dy)&0x80000000)
    {
        if ((node->dy ^ dx) & 0x80000000)
        {
            // (left is negative)
            return 1;
        }
        return 0;
    }

    left = FixedMul ( node->dy>>FRACBITS , dx );
    right = FixedMul ( dy , node->dx>>FRACBITS );

    if (right < left)
    {
        // front side
        return 0;
    }
    // back side
    return 1;			
}


int R_PointOnSegSide (fixed_t x, fixed_t y, seg_t* line)
{
    fixed_t lx;
    fixed_t ly;
    fixed_t ldx;
    fixed_t ldy;
    fixed_t dx;
    fixed_t dy;
    fixed_t left;
    fixed_t right;

    lx = line->v1->x;
    ly = line->v1->y;

    ldx = line->v2->x - lx;
    ldy = line->v2->y - ly;

    if (!ldx)
    {
        if (x <= lx)
        return ldy > 0;

        return ldy < 0;
    }
    if (!ldy)
    {
        if (y <= ly)
        return ldx < 0;

        return ldx > 0;
    }

    dx = (x - lx);
    dy = (y - ly);

    // Try to quickly decide by looking at sign bits.
    if ( (ldy ^ ldx ^ dx ^ dy)&0x80000000 )
    {
        if  ( (ldy ^ dx) & 0x80000000 )
        {
            // (left is negative)
            return 1;
        }
        return 0;
    }

    left = FixedMul ( ldy>>FRACBITS , dx );
    right = FixedMul ( dy , ldx>>FRACBITS );

    if (right < left)
    {
        // front side
        return 0;
    }
    // back side
    return 1;			
}


//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table.

angle_t
R_PointToAngle (fixed_t x, fixed_t y)
{
    x -= viewx;
    y -= viewy;

    if ((!x) && (!y))
    return 0;

    if (x>= 0)
    {
        // x >=0
        if (y>= 0)
        {
            // y>= 0

            if (x>y)
            {
                // octant 0
                return tantoangle[ SlopeDiv(y,x)];
            }
            else
            {
                // octant 1
                return ANG90-1-tantoangle[ SlopeDiv(x,y)];
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x>y)
            {
                // octant 8
                return -tantoangle[SlopeDiv(y,x)];
            }
            else
            {
                // octant 7
                return ANG270+tantoangle[ SlopeDiv(x,y)];
            }
        }
    }
    else
    {
        // x<0
        x = -x;

        if (y>= 0)
        {
            // y>= 0
            if (x>y)
            {
                // octant 3
                return ANG180-1-tantoangle[ SlopeDiv(y,x)];
            }
            else
            {
                // octant 2
                return ANG90+ tantoangle[ SlopeDiv(x,y)];
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x>y)
            {
                // octant 4
                return ANG180+tantoangle[ SlopeDiv(y,x)];
            }
            else
            {
                // octant 5
                return ANG270-1-tantoangle[ SlopeDiv(x,y)];
            }
        }
    }
    return 0;
}


angle_t
R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{	
    viewx = x1;
    viewy = y1;

    return R_PointToAngle (x2, y2);
}


fixed_t
R_PointToDist (fixed_t x, fixed_t y)
{
    int     angle;
    fixed_t dx;
    fixed_t dy;
    fixed_t temp;
    fixed_t dist;
    fixed_t frac;

    dx = abs(x - viewx);
    dy = abs(y - viewy);

    if (dy>dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    // Fix crashes in udm1.wad

    if (dx != 0)
    {
        frac = FixedDiv(dy, dx);
    }
    else
    {
        frac = 0;
    }

    angle = (tantoangle[frac>>DBITS]+ANG90) >> ANGLETOFINESHIFT;

    // use as cosine
    dist = FixedDiv (dx, finesine[angle] );	

    return dist;
}


// [AM] Interpolate between two angles.
angle_t R_InterpolateAngle(angle_t oangle, angle_t nangle, fixed_t scale)
{
    if (nangle == oangle)
        return nangle;
    else if (nangle > oangle)
    {
        if (nangle - oangle < ANG270)
            return oangle + (angle_t)((nangle - oangle) * FIXED2DOUBLE(scale));
        else // Wrapped around
            return oangle - (angle_t)((oangle - nangle) * FIXED2DOUBLE(scale));
    }
    else // nangle < oangle
    {
        if (oangle - nangle < ANG270)
            return oangle - (angle_t)((oangle - nangle) * FIXED2DOUBLE(scale));
        else // Wrapped around
            return oangle + (angle_t)((nangle - oangle) * FIXED2DOUBLE(scale));
    }
}

//
// R_InitTextureMapping
//
void R_InitTextureMapping (void)
{
    int     i;
    int     x;
    int     t;
    fixed_t focallength;

    // Use tangent table to generate viewangletox:
    //  viewangletox will give the next greatest x
    //  after the view angle.
    //
    // Calc focallength
    //  so FIELDOFVIEW angles covers SCREENWIDTH.

    // [crispy] in widescreen mode, make sure the same number of horizontal
    // pixels shows the same part of the game scene as in regular rendering mode
    fixed_t focalwidth;
    focalwidth = (((ORIGWIDTH << hires)>>detailshift)/2)<<FRACBITS;

    focallength = FixedDiv (widescreen > 0 ? focalwidth : centerxfrac, finetangent[FINEANGLES/4+FIELDOFVIEW/2] );

    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
        if (finetangent[i] > FRACUNIT*2)
        t = -1;
        else if (finetangent[i] < -FRACUNIT*2)
        t = viewwidth+1;
        else
        {
            t = FixedMul (finetangent[i], focallength);
            t = (centerxfrac - t+FRACUNIT-1)>>FRACBITS;

            if (t < -1)
            t = -1;
            else if (t>viewwidth+1)
            t = viewwidth+1;
        }
        viewangletox[i] = t;
    }

    // Scan viewangletox[] to generate xtoviewangle[]:
    //  xtoviewangle will give the smallest view angle
    //  that maps to x.	
    for (x=0;x<=viewwidth;x++)
    {
        i = 0;

        while (viewangletox[i]>x)
        i++;

        xtoviewangle[x] = (i<<ANGLETOFINESHIFT)-ANG90;
    }

    // Take out the fencepost cases from viewangletox.
    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
        t = FixedMul (finetangent[i], focallength);
        t = centerx - t;
	
        if (viewangletox[i] == -1)
            viewangletox[i] = 0;
        else if (viewangletox[i] == viewwidth+1)
            viewangletox[i]  = viewwidth;
    }

    clipangle = xtoviewangle[0];
}


//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//
#define DISTMAP     2

void R_InitLightTables (void)
{
    int i;
    int j;
    int level;
    int startmap; 	
    int scale;

    // Calculate the light levels to use
    //  for each level / distance combination.
    for (i=0 ; i< LIGHTLEVELS ; i++)
    {
        startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;

        // [JN] No smoother diminished lighting in -vanilla mode
        for (j=0 ; vanillaparm ? j<MAXLIGHTZ_VANILLA : j<MAXLIGHTZ ; j++)
        {
            scale = FixedDiv ((320 / 2*FRACUNIT), vanillaparm ? 
                                                      ((j+1)<<LIGHTZSHIFT_VANILLA) : 
                                                      ((j+1)<<LIGHTZSHIFT));

            scale >>= LIGHTSCALESHIFT;
            level = startmap - scale/DISTMAP;

            if (level < 0)
            level = 0;

            if (level >= NUMCOLORMAPS)
            level = NUMCOLORMAPS-1;

            zlight[i][j] = colormaps + level*256;
            
            // [JN] Floor brightmaps
            fullbright_notgrayorbrown_floor[i][j] = brightmaps_notgrayorbrown + level * 256;
            fullbright_orangeyellow_floor[i][j] = brightmaps_orangeyellow + level * 256;
        }
    }
}


//
// R_SetViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
boolean setsizeneeded;
int     setblocks;
int     setdetail;

// [crispy] lookup table for horizontal screen coordinates
int		flipwidth[WIDEMAXWIDTH];


void R_SetViewSize (int blocks, int detail)
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}


//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize (void)
{
    fixed_t cosadj;
    fixed_t dy;
    int     i;
    int     j;
    int     level;
    int     startmap; 	

    setsizeneeded = false;

    if (widescreen == -1 || widescreen == 0)
    {   // [JN] 4:3
        if (setblocks >= 11)
        {
            scaledviewwidth = SCREENWIDTH;
            scaledviewheight = SCREENHEIGHT;
        }
        else
        {
            scaledviewwidth = (setblocks*32)<<hires;
            // [JN] Jaguar: status bar is 40 px tall, instead of standard 32
            scaledviewheight = ((setblocks * (gamemission == jaguar ?
                                            163: 168) / 10) & ~7) << hires;
        }
    }
    else if (widescreen == 1)
    {   // [JN] 16:9
        if (setblocks == 9)
        {
            scaledviewwidth = WIDESCREENWIDTH;
            scaledviewheight = SCREENHEIGHT - (gamemission == jaguar ? 
                                               80 : 32 << hires);
        }
        else if (setblocks == 10)
        {
            scaledviewwidth = WIDESCREENWIDTH;
            // [JN] Jaguar: status bar is 40 px tall, instead of standard 32
            scaledviewheight = ((setblocks * (gamemission == jaguar ?
                                            163: 168) / 10) & ~7) << hires;

        }
        else if (setblocks >= 11)
        {
            scaledviewwidth = WIDESCREENWIDTH;
            scaledviewheight = SCREENHEIGHT;
        }
    }
    else if (widescreen == 2)
    {   // [JN] 16:10
        if (setblocks == 9)
        {
            scaledviewwidth = WIDESCREENWIDTH - (42 << hires);
            scaledviewheight = SCREENHEIGHT - (gamemission == jaguar ? 
                                               80 : 32 << hires);
        }
        else if (setblocks == 10)
        {
            scaledviewwidth = WIDESCREENWIDTH - (42 << hires);
            // [JN] Jaguar: status bar is 40 px tall, instead of standard 32
            scaledviewheight = ((setblocks * (gamemission == jaguar ?
                                            163: 168) / 10) & ~7) << hires;

        }
        else if (setblocks >= 11)
        {
            scaledviewwidth = WIDESCREENWIDTH - (42 << hires);
            scaledviewheight = SCREENHEIGHT;
        }
    }

    detailshift = setdetail;
    viewwidth = scaledviewwidth>>detailshift;
    viewheight = scaledviewheight>>(detailshift && hires);

    centery = viewheight/2;
    centerx = viewwidth/2;
    centerxfrac = centerx<<FRACBITS;
    centeryfrac = centery<<FRACBITS;

    if (widescreen > 0)
    {
        projection = MIN(centerxfrac, (((320 << hires)>>detailshift)/2)<<FRACBITS);
    }
    else
    {
        projection = centerxfrac;
    }

    if (!detailshift)
    {
        colfunc = basecolfunc = R_DrawColumn;
        fuzzcolfunc = (vanillaparm || improved_fuzz == 0) ? R_DrawFuzzColumn :
                                      improved_fuzz == 1  ? R_DrawFuzzColumnBW :
                                      improved_fuzz == 2  ? R_DrawFuzzColumnImproved :
                                                            R_DrawFuzzColumnImprovedBW;
        transcolfunc = R_DrawTranslatedColumn;
        tlcolfunc = R_DrawTLColumn;
        spanfunc = R_DrawSpan;
    }
    else
    {
        colfunc = basecolfunc = R_DrawColumnLow;
        fuzzcolfunc = (vanillaparm || improved_fuzz == 0) ? R_DrawFuzzColumnLow :
                                      improved_fuzz == 1  ? R_DrawFuzzColumnLowBW :
                                      improved_fuzz == 2  ? R_DrawFuzzColumnLowImproved :
                                                            R_DrawFuzzColumnLowImprovedBW;
        transcolfunc = R_DrawTranslatedColumnLow;
        tlcolfunc = R_DrawTLColumnLow;
        spanfunc = R_DrawSpanLow;
    }

    R_InitBuffer (scaledviewwidth, scaledviewheight);

    R_InitTextureMapping ();

    // psprite scales
    pspritescale = FRACUNIT*viewwidth/origwidth;
    pspriteiscale = FRACUNIT*origwidth/viewwidth;

    // thing clipping
    for (i=0 ; i<viewwidth ; i++)
    screenheightarray[i] = viewheight;
    
    // planes
    for (i=0 ; i<viewheight ; i++)
    {
        const fixed_t num = (viewwidth<<(detailshift && !hires))/2*FRACUNIT;
        const fixed_t num_wide = MIN(viewwidth<<detailshift, 320 << !detailshift)/2*FRACUNIT;

        for (j = 0; j < LOOKDIRS; j++)
        {
            if (widescreen > 0)
            {
                dy = ((i-(viewheight/2 + ((j-LOOKDIRMIN) << (hires && !detailshift)) * (screenblocks < 9 ? screenblocks : 9) / 10))<<FRACBITS)+FRACUNIT/2;
            }
            else
            {
                dy = ((i-(viewheight/2 + ((j-LOOKDIRMIN) << (hires && !detailshift)) * (screenblocks < 11 ? screenblocks : 11) / 10))<<FRACBITS)+FRACUNIT/2;
            }

        dy = abs(dy);
        yslopes[j][i] = FixedDiv (widescreen > 0 ? num_wide : num, dy);
        }
    }
    yslope = yslopes[LOOKDIRMIN];

    for (i=0 ; i<viewwidth ; i++)
    {
        cosadj = abs(finecosine[xtoviewangle[i]>>ANGLETOFINESHIFT]);
        distscale[i] = FixedDiv (FRACUNIT,cosadj);
    }

    // Calculate the light levels to use
    //  for each level / scale combination.
    for (i=0 ; i< LIGHTLEVELS ; i++)
    {
        startmap = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
        for (j=0 ; j<MAXLIGHTSCALE ; j++)
        {
            level = startmap - j*screenwidth/(viewwidth<<detailshift)/DISTMAP;

            if (level < 0)
            level = 0;

            if (level >= NUMCOLORMAPS)
            level = NUMCOLORMAPS-1;

            scalelight[i][j] = colormaps + level*256;

            // [JN] Wall and sprite brightmaps
            fullbright_redonly[i][j] = brightmaps_redonly + level*256;
            fullbright_notgray[i][j] = brightmaps_notgray + level*256;
            fullbright_notgrayorbrown[i][j] = brightmaps_notgrayorbrown + level*256;
            fullbright_greenonly1[i][j] = brightmaps_greenonly1 + level*256;
            fullbright_greenonly2[i][j] = brightmaps_greenonly2 + level*256;
            fullbright_greenonly3[i][j] = brightmaps_greenonly3 + level*256;
            fullbright_orangeyellow[i][j] = brightmaps_orangeyellow + level*256;
            fullbright_dimmeditems[i][j] = brightmaps_dimmeditems + level*256;
            fullbright_brighttan[i][j] = brightmaps_brighttan + level*256;
            fullbright_redonly1[i][j] = brightmaps_redonly1 + level*256;
            fullbright_explosivebarrel[i][j] = brightmaps_explosivebarrel + level*256;
            fullbright_alllights[i][j] = brightmaps_alllights + level*256;
            fullbright_candles[i][j] = brightmaps_candles + level*256;
            fullbright_pileofskulls[i][j] = brightmaps_pileofskulls + level*256;
            fullbright_redonly2[i][j] = brightmaps_redonly2 + level*256;
        }
    }

    // [crispy] lookup table for horizontal screen coordinates
    for (i = 0, j = scaledviewwidth - 1; i < scaledviewwidth; i++, j--)
    {
        flipwidth[i] = flip_levels ? j : i;
    }
}


//
// R_Init
//
void R_Init (void)
{
    if (widescreen > 0)
    {
        // [JN] Wide screen: don't allow unsupported view modes at startup
        if (screenblocks < 9)
            screenblocks = 9;
        if (screenblocks > 14)
            screenblocks = 14;
    }

    R_InitData ();
    printf (".");
    R_SetViewSize (screenblocks, detailLevel);  // viewwidth / viewheight / detailLevel are set by the defaults
    printf (".");
    R_InitLightTables ();
    printf (".");
    R_InitSkyMap ();
    printf (".");
    R_InitTranslationTables ();
    printf (".");

    framecount = 0;
}


//
// R_PointInSubsector
//
subsector_t*
R_PointInSubsector (fixed_t x, fixed_t y)
{
    node_t* node;
    int     side;
    int     nodenum;

    // single subsector is a special case
    if (!numnodes)				
    return subsectors;

    nodenum = numnodes-1;

    while (! (nodenum & NF_SUBSECTOR) )
    {
        node = &nodes[nodenum];
        side = R_PointOnSide (x, y, node);
        nodenum = node->children[side];
    }

    return &subsectors[nodenum & ~NF_SUBSECTOR];
}


//
// R_SetupFrame
//
void R_SetupFrame (player_t* player)
{		
    int i;
    int tempCentery;
    int pitch;

    viewplayer = player;

    // [AM] Interpolate the player camera if the feature is enabled.

    if (uncapped_fps && !vanillaparm &&
        // Don't interpolate on the first tic of a level,
        // otherwise oldviewz might be garbage.
        leveltime > 1 &&
        // Don't interpolate if the player did something
        // that would necessitate turning it off for a tic.
        player->mo->interp == true &&
        // Don't interpolate during a paused state
        !paused && (!menuactive || demoplayback || netgame))
    {
        // Interpolate player camera from their old position to their current one.
        viewx = player->mo->oldx + FixedMul(player->mo->x - player->mo->oldx, fractionaltic);
        viewy = player->mo->oldy + FixedMul(player->mo->y - player->mo->oldy, fractionaltic);
        viewz = player->oldviewz + FixedMul(player->viewz - player->oldviewz, fractionaltic);
        viewangle = R_InterpolateAngle(player->mo->oldangle, player->mo->angle, fractionaltic) + viewangleoffset;

        pitch = (player->oldlookdir + (player->lookdir - player->oldlookdir) * FIXED2DOUBLE(fractionaltic)) / MLOOKUNIT;
    }
    else
    {
        viewx = player->mo->x;
        viewy = player->mo->y;
        viewz = player->viewz;
        viewangle = player->mo->angle + viewangleoffset;

        // [crispy] pitch is actual lookdir /*and weapon pitch*/
        pitch = player->lookdir / MLOOKUNIT;
    }

    extralight = player->extralight;

    if (pitch > LOOKDIRMAX)
    pitch = LOOKDIRMAX;
    else
    if (pitch < -LOOKDIRMIN)
    pitch = -LOOKDIRMIN;

    // apply new yslope[] whenever "lookdir", "detailshift" or "screenblocks" change

    if (widescreen > 0)
    {
        tempCentery = viewheight/2 + (pitch << (hires && !detailshift)) * (screenblocks < 9 ? screenblocks : 9) / 10;
    }
    else
    {
        tempCentery = viewheight/2 + (pitch << (hires && !detailshift)) * (screenblocks < 11 ? screenblocks : 11) / 10;
    }

    if (centery != tempCentery)
    {
        centery = tempCentery;
        centeryfrac = centery << FRACBITS;
        yslope = yslopes[LOOKDIRMIN + pitch];
    }

    viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
    viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];

    sscount = 0;

    if (player->fixedcolormap)
    {
        // [JN] Fix aftermath of "Invulnerability colormap bug" fix,
        // when sky texture was slightly affected by changing to
        // fixed (non-inversed) colormap.
        // https://doomwiki.org/wiki/Invulnerability_colormap_bug
        // 
        // Infra green visor is using colormap №33 from Beta's COLORMAB lump.
        // Needed for compatibility and for preventing "black screen" while 
        // using Visor with possible non-standard COLORMAPS in PWADs.

        if (player->powers[pw_invulnerability])
        {
            fixedcolormap = colormaps + player->fixedcolormap * 256;
        }
        // [JN] Support for Press Beta and Infragreen Visor
        else if ((gamemode == pressbeta && player->powers[pw_invisibility])
        || ((infragreen_visor || gamemode == pressbeta) && player->powers[pw_infrared]))
        {
            fixedcolormap = colormaps_beta + player->fixedcolormap * 256;
        }
        else
        {
            fixedcolormap = colormaps;
        }

        walllights = scalelightfixed;

        for (i=0 ; i<MAXLIGHTSCALE ; i++)
	    scalelightfixed[i] = fixedcolormap;
    }
    else
    fixedcolormap = 0;

    framecount++;
    validcount++;
}


//
// R_RenderView
//
void R_RenderPlayerView (player_t* player)
{	
    extern void V_DrawFilledBox (int x, int y, int w, int h, int c);
    extern void R_InterpolateTextureOffsets (void);
    extern boolean beneath_door;

    R_SetupFrame (player);

    // Clear buffers.
    R_ClearClipSegs ();
    R_ClearDrawSegs ();
    if (automapactive && !automap_overlay)
    {
        R_RenderBSPNode (numnodes-1);
        return;
    }

    if (beneath_door == true)
    {
        // [JN] fill whole screen with black color and don't go any farther
        V_DrawFilledBox(viewwindowx, viewwindowy, scaledviewwidth, scaledviewheight, 0);
        return;
    }    

    // [JN] Draw map's "out of bounds" as a black color
    V_DrawFilledBox(viewwindowx, viewwindowy, scaledviewwidth, scaledviewheight, 0);

    R_ClearPlanes ();
    R_ClearSprites ();

    // check for new console commands.
    NetUpdate ();

    // [crispy] smooth texture scrolling
    R_InterpolateTextureOffsets();

    // The head node is the last node output.
    R_RenderBSPNode (numnodes-1);

    // Check for new console commands.
    NetUpdate ();

    R_DrawPlanes ();

    // Check for new console commands.
    NetUpdate ();

    // [crispy] draw fuzz effect independent of rendering frame rate
    // [JN] Continue fuzz animation in paused states in -vanilla mode
    if (!vanillaparm)
    R_SetFuzzPosDraw();

    R_DrawMasked ();

    // Check for new console commands.
    NetUpdate ();				
}

