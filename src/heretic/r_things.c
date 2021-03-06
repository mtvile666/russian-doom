//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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
// R_things.c



#include <stdio.h>
#include <stdlib.h>
#include "doomdef.h"
#include "deh_str.h"
#include "i_swap.h"
#include "i_system.h"
#include "r_local.h"
#include "v_trans.h"
#include "crispy.h"
#include "jn.h"

typedef struct
{
    int x1, x2;

    int column;
    int topclip;
    int bottomclip;
} maskdraw_t;

/*

Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn CLOCKWISE
around the axis. This is not the same as the angle, which increases counter
clockwise (protractor).  There was a lot of stuff grabbed wrong, so I changed it...

*/


fixed_t pspritescale, pspriteiscale;

lighttable_t **spritelights;

// [JN] Brightmaps
lighttable_t** fullbrights_greenonly;
lighttable_t** fullbrights_redonly;
lighttable_t** fullbrights_blueonly;
lighttable_t** fullbrights_purpleonly;
lighttable_t** fullbrights_notbronze;
lighttable_t** fullbrights_flame;
lighttable_t** fullbrights_greenonly_dim;
lighttable_t** fullbrights_redonly_dim;
lighttable_t** fullbrights_blueonly_dim;
lighttable_t** fullbrights_yellowonly_dim;
lighttable_t** fullbrights_ethereal;


// constant arrays used for psprite clipping and initializing clipping
int negonearray[WIDESCREENWIDTH];       // [crispy] 32-bit integer math
int screenheightarray[WIDESCREENWIDTH]; // [crispy] 32-bit integer math

/*
===============================================================================

						INITIALIZATION FUNCTIONS

===============================================================================
*/

// variables used to look up and range check thing_t sprites patches
spritedef_t *sprites;
int numsprites;

spriteframe_t sprtemp[26];
int maxframe;
char *spritename;



/*
=================
=
= R_InstallSpriteLump
=
= Local function for R_InitSprites
=================
*/

void R_InstallSpriteLump(int lump, unsigned frame, unsigned rotation,
                         boolean flipped)
{
    int r;

    if (frame >= 26 || rotation > 8)
        I_Error(english_language ?
                "R_InstallSpriteLump: bad frame characters in lump %i" :
                "R_InstallSpriteLump: некорректные символы фрейма в блоке %i",
                lump);

    if ((int) frame > maxframe)
        maxframe = frame;

    if (rotation == 0)
    {
// the lump should be used for all rotations
        if (sprtemp[frame].rotate == false)
            I_Error (english_language ?
                    "R_InitSprites: sprite %s frame %c has multip rot=0 lump" :
                    "R_InitSprites: фрейм %c спрайта %s имеет многократный блок rot=0",
                    spritename, 'A' + frame);
        if (sprtemp[frame].rotate == true)
            I_Error (english_language ?
                "R_InitSprites: sprite %s frame %c has rotations and a rot=0 lump" :
                "R_InitSprites: фрейм %c спрайта %s имеет фреймы поворота и блок rot=0",
                 spritename, 'A' + frame);

        sprtemp[frame].rotate = false;
        for (r = 0; r < 8; r++)
        {
            sprtemp[frame].lump[r] = lump - firstspritelump;
            sprtemp[frame].flip[r] = (byte) flipped;
        }
        return;
    }

// the lump is only used for one rotation
    if (sprtemp[frame].rotate == false)
        I_Error (english_language ?
                 "R_InitSprites: sprite %s frame %c has rotations and a rot=0 lump" :
                 "R_InitSprites: фрейм спрайта %c спрайта %s имеет фреймы поворота и блок rot=0",
                 spritename, 'A' + frame);

    sprtemp[frame].rotate = true;

    rotation--;                 // make 0 based
    if (sprtemp[frame].lump[rotation] != -1)
        I_Error (english_language ?
                 "R_InitSprites: sprite %s : %c : %c has two lumps mapped to it" :
                 "R_InitSprites: спрайу %s : %c : %c назначено несколько одинаковых блоков",            
                 spritename, 'A' + frame, '1' + rotation);

    sprtemp[frame].lump[rotation] = lump - firstspritelump;
    sprtemp[frame].flip[rotation] = (byte) flipped;
}

/*
=================
=
= R_InitSpriteDefs
=
= Pass a null terminated list of sprite names (4 chars exactly) to be used
= Builds the sprite rotation matrixes to account for horizontally flipped
= sprites.  Will report an error if the lumps are inconsistant
=Only called at startup
=
= Sprite lump names are 4 characters for the actor, a letter for the frame,
= and a number for the rotation, A sprite that is flippable will have an
= additional letter/number appended.  The rotation character can be 0 to
= signify no rotations
=================
*/

void R_InitSpriteDefs(char **namelist)
{
    char **check;
    int i, l, frame, rotation;
    int start, end;

// count the number of sprite names
    check = namelist;
    while (*check != NULL)
        check++;
    numsprites = check - namelist;

    if (!numsprites)
        return;

    sprites = Z_Malloc(numsprites * sizeof(*sprites), PU_STATIC, NULL);

    start = firstspritelump - 1;
    end = lastspritelump + 1;

// scan all the lump names for each of the names, noting the highest
// frame letter
// Just compare 4 characters as ints
    for (i = 0; i < numsprites; i++)
    {
        spritename = DEH_String(namelist[i]);
        memset(sprtemp, -1, sizeof(sprtemp));

        maxframe = -1;

        //
        // scan the lumps, filling in the frames for whatever is found
        //
        for (l = start + 1; l < end; l++)
            if (!strncasecmp(lumpinfo[l]->name, spritename, 4))
            {
                frame = lumpinfo[l]->name[4] - 'A';
                rotation = lumpinfo[l]->name[5] - '0';
                R_InstallSpriteLump(l, frame, rotation, false);
                if (lumpinfo[l]->name[6])
                {
                    frame = lumpinfo[l]->name[6] - 'A';
                    rotation = lumpinfo[l]->name[7] - '0';
                    R_InstallSpriteLump(l, frame, rotation, true);
                }
            }

        //
        // check the frames that were found for completeness
        //
        if (maxframe == -1)
        {
            //continue;
            sprites[i].numframes = 0;
            if (gamemode == shareware)
                continue;
            I_Error(english_language ?
                    "R_InitSprites: no lumps found for sprite %s" :
                    "R_InitSprites: не найдены блоки в спрайте %s",
                    spritename);
        }

        maxframe++;
        for (frame = 0; frame < maxframe; frame++)
        {
            switch ((int) sprtemp[frame].rotate)
            {
                case -1:       // no rotations were found for that frame at all
                    I_Error(english_language ?
                            "R_InitSprites: no patches found for %s frame %c" :
                            "R_InitSprites: не найдены патчи для спрайта %s, фрейма %c",
                            spritename, frame + 'A');
                case 0:        // only the first rotation is needed
                    break;

                case 1:        // must have all 8 frames
                    for (rotation = 0; rotation < 8; rotation++)
                        if (sprtemp[frame].lump[rotation] == -1)
                            I_Error(english_language ?
                                    "R_InitSprites: sprite %s frame %c is missing rotations" :
                                    "R_InitSprites: в фрейме %c спрайта %s отсутствует информация о вращении",
                                    spritename, frame + 'A');
            }
        }

        //
        // allocate space for the frames present and copy sprtemp to it
        //
        sprites[i].numframes = maxframe;
        sprites[i].spriteframes =
            Z_Malloc(maxframe * sizeof(spriteframe_t), PU_STATIC, NULL);
        memcpy(sprites[i].spriteframes, sprtemp,
               maxframe * sizeof(spriteframe_t));
    }

}


/*
===============================================================================

							GAME FUNCTIONS

===============================================================================
*/

vissprite_t *vissprites;
vissprite_t *vissprite_p;
int newvissprite;
static int	numvissprites;


/*
===================
=
= R_InitSprites
=
= Called at program start
===================
*/

void R_InitSprites(char **namelist)
{
    int i;

    for (i = 0; i < screenwidth; i++)
    {
        negonearray[i] = -1;
    }

    R_InitSpriteDefs(namelist);
}


/*
===================
=
= R_ClearSprites
=
= Called at frame start
===================
*/

void R_ClearSprites(void)
{
    vissprite_p = vissprites;
}


/*
===================
=
= R_NewVisSprite
=
===================
*/

vissprite_t overflowsprite;

vissprite_t *R_NewVisSprite(void)
{
    // [crispy] remove MAXVISSPRITE limit
    if (vissprite_p == &vissprites[numvissprites])
    {
        static int i;
        int numvissprites_old = numvissprites;

        if (i++ == 1)
        puts("R_NewVisSprite: Hit MAXVISSPRITES limit.");

        numvissprites += i * MAXVISSPRITES;
        vissprites = realloc(vissprites, numvissprites * sizeof(*vissprites));
        vissprite_p = vissprites + numvissprites_old;
    }

    vissprite_p++;
    return vissprite_p - 1;
}


/*
================
=
= R_DrawMaskedColumn
=
= Used for sprites and masked mid textures
================
*/

int* mfloorclip;   // [crispy] 32-bit integer math
int* mceilingclip; // [crispy] 32-bit integer math
fixed_t spryscale;
int64_t sprtopscreen;
int64_t sprbotscreen;

void R_DrawMaskedColumn(column_t * column, signed int baseclip)
{
    int64_t topscreen;
    int64_t bottomscreen;
    fixed_t basetexturemid;

    basetexturemid = dc_texturemid;
    dc_texheight = 0;

    for (; column->topdelta != 0xff;)
    {
// calculate unclipped screen coordinates for post
        topscreen = sprtopscreen + spryscale * column->topdelta;
        bottomscreen = topscreen + spryscale * column->length;

        dc_yl = (int)((topscreen+FRACUNIT-1)>>FRACBITS);
        dc_yh = (int)((bottomscreen-1)>>FRACBITS);

        if (dc_yh >= mfloorclip[dc_x])
            dc_yh = mfloorclip[dc_x] - 1;
        if (dc_yl <= mceilingclip[dc_x])
            dc_yl = mceilingclip[dc_x] + 1;

        if (dc_yh >= baseclip && baseclip != -1)
            dc_yh = baseclip;

        if (dc_yl <= dc_yh)
        {
            dc_source = (byte *) column + 3;
            dc_texturemid = basetexturemid - (column->topdelta << FRACBITS);
//                      dc_source = (byte *)column + 3 - column->topdelta;
            colfunc();          // either R_DrawColumn or R_DrawTLColumn
        }
        column = (column_t *) ((byte *) column + column->length + 4);
    }

    dc_texturemid = basetexturemid;
}


/*
================
=
= R_DrawVisSprite
=
= mfloorclip and mceilingclip should also be set
================
*/

void R_DrawVisSprite(vissprite_t * vis, int x1, int x2)
{
    column_t *column;
    int texturecolumn;
    fixed_t frac;
    patch_t *patch;
    fixed_t baseclip;


    patch = W_CacheLumpNum(vis->patch + firstspritelump, PU_CACHE);

    dc_colormap = vis->colormap;

//      if(!dc_colormap)
//              colfunc = tlcolfunc;  // NULL colormap = shadow draw

    if (vis->mobjflags & MF_SHADOW)
    {
        if (vis->mobjflags & MF_TRANSLATION)
        {
            colfunc = R_DrawTranslatedTLColumn;
            dc_translation = translationtables - 256 +
                ((vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT - 8));
        }
        else
        {                       // Draw using shadow column function
            colfunc = tlcolfunc;
        }
    }
    else if (vis->mobjflags & MF_TRANSLATION)
    {
        // Draw using translated column function
        colfunc = R_DrawTranslatedColumn;
        dc_translation = translationtables - 256 +
            ((vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT - 8));
    }
    else if (vis->translation)
    {
        colfunc = R_DrawTranslatedColumn;
        dc_translation = vis->translation;
    }

    dc_iscale = abs(vis->xiscale) >> detailshift;
    dc_texturemid = vis->texturemid;
    frac = vis->startfrac;
    spryscale = vis->scale;

    sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);

// check to see if weapon is a vissprite
    if (vis->psprite)
    {
        dc_texturemid += FixedMul(((centery - viewheight / 2) << FRACBITS),
                                  pspriteiscale);
        sprtopscreen += (viewheight / 2 - centery) << FRACBITS;
    }

    if (vis->footclip && !vis->psprite)
    {
        sprbotscreen = sprtopscreen + FixedMul(SHORT(patch->height) << FRACBITS,
                                               spryscale);
        baseclip = (sprbotscreen - FixedMul(vis->footclip << FRACBITS,
                                            spryscale)) >> FRACBITS;
    }
    else
    {
        baseclip = -1;
    }

    for (dc_x = vis->x1; dc_x <= vis->x2; dc_x++, frac += vis->xiscale)
    {
        texturecolumn = frac >> FRACBITS;
#ifdef RANGECHECK
        if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
            I_Error(english_language ?
                    "R_DrawSpriteRange: bad texturecolumn" :
                    "R_DrawSpriteRange: некорректныая информация texturecolumn");
#endif
        column = (column_t *) ((byte *) patch +
                               LONG(patch->columnofs[texturecolumn]));
        R_DrawMaskedColumn(column, baseclip);
    }

    colfunc = basecolfunc;
}



/*
===================
=
= R_ProjectSprite
=
= Generates a vissprite for a thing if it might be visible
=
===================
*/

void R_ProjectSprite(mobj_t * thing)
{
    fixed_t trx, try;
    fixed_t gxt, gyt;
    fixed_t tx, tz;
    fixed_t xscale;
    int x1, x2;
    spritedef_t *sprdef;
    spriteframe_t *sprframe;
    int lump;
    unsigned rot;
    boolean flip;
    int index;
    vissprite_t *vis;
    angle_t ang;
    fixed_t iscale;

    fixed_t             interpx;
    fixed_t             interpy;
    fixed_t             interpz;
    fixed_t             interpangle;

    // [AM] Interpolate between current and last position,
    //      if prudent.
    if (uncapped_fps && !vanillaparm &&
        // Don't interpolate if the mobj did something
        // that would necessitate turning it off for a tic.
        thing->interp == true &&
        // Don't interpolate during a paused state.
        !paused && (!menuactive || demoplayback || netgame))
    {
        interpx = thing->oldx + FixedMul(thing->x - thing->oldx, fractionaltic);
        interpy = thing->oldy + FixedMul(thing->y - thing->oldy, fractionaltic);
        interpz = thing->oldz + FixedMul(thing->z - thing->oldz, fractionaltic);
        interpangle = R_InterpolateAngle(thing->oldangle, thing->angle, fractionaltic);
    }
    else
    {
        interpx = thing->x;
        interpy = thing->y;
        interpz = thing->z;
        interpangle = thing->angle;
    }

    if (thing->flags2 & MF2_DONTDRAW)
    {   // Never make a vissprite when MF2_DONTDRAW is flagged.
        return;
    }

    // [JN] Never draw a blood splat for Liches if colored blood is not set
    if ((!colored_blood || vanillaparm) 
    &&  thing->type == MT_BLOODSPLATTER &&  thing->target
    &&  thing->target->type == MT_HEAD)
    {
        return;
    }

//
// transform the origin point
//
    trx = interpx - viewx;
    try = interpy - viewy;

    gxt = FixedMul(trx, viewcos);
    gyt = -FixedMul(try, viewsin);
    tz = gxt - gyt;

    if (tz < MINZ)
        return;                 // thing is behind view plane
    xscale = FixedDiv(projection, tz);

    gxt = -FixedMul(trx, viewsin);
    gyt = FixedMul(try, viewcos);
    tx = -(gyt + gxt);

    if (abs(tx) > (tz << 2))
        return;                 // too far off the side

//
// decide which patch to use for sprite reletive to player
//
#ifdef RANGECHECK
    if ((unsigned) thing->sprite >= numsprites)
        I_Error(english_language ?
                "R_ProjectSprite: invalid sprite number %i " :
                "R_ProjectSprite: некорректный номер спрайта %i ", thing->sprite);
#endif
    sprdef = &sprites[thing->sprite];
#ifdef RANGECHECK
    if ((thing->frame & FF_FRAMEMASK) >= sprdef->numframes)
        I_Error(english_language ?
                "R_ProjectSprite: invalid sprite frame %i : %i " :
                "R_ProjectSprite: некорректный фрейм спрайта %i : %i ",
                thing->sprite, thing->frame);
#endif
    sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];

    if (sprframe->rotate)
    {                           // choose a different rotation based on player view
        ang = R_PointToAngle(interpx, interpy);
        rot = (ang - interpangle + (unsigned) (ANG45 / 2) * 9) >> 29;
        lump = sprframe->lump[rot];
        flip = (boolean) sprframe->flip[rot];
    }
    else
    {                           // use single rotation for all views
        lump = sprframe->lump[0];
        flip = (boolean) sprframe->flip[0];
    }

//
// calculate edges of the shape
//
    tx -= spriteoffset[lump];
    x1 = (centerxfrac + FixedMul(tx, xscale)) >> FRACBITS;
    if (x1 > viewwidth)
        return;                 // off the right side
    tx += spritewidth[lump];
    x2 = ((centerxfrac + FixedMul(tx, xscale)) >> FRACBITS) - 1;
    if (x2 < 0)
        return;                 // off the left side


//
// store information in a vissprite
//
    vis = R_NewVisSprite();
    vis->translation = NULL;
    vis->mobjflags = thing->flags;
    vis->psprite = false;
    vis->scale = xscale << detailshift;
    vis->gx = interpx;
    vis->gy = interpy;
    vis->gz = interpz;
    vis->gzt = interpz + spritetopoffset[lump];

    // foot clipping
    if (thing->flags2 & MF2_FEETARECLIPPED
        && thing->z <= thing->subsector->sector->floorheight)
    {
        vis->footclip = 10;
    }
    else
        vis->footclip = 0;
    vis->texturemid = vis->gzt - viewz - (vis->footclip << FRACBITS);

    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth - 1 : x2;
    iscale = FixedDiv(FRACUNIT, xscale);

    // [crispy] flip death sprites and corpses randomly
    if (randomly_flipcorpses && !vanillaparm)
    {
        if ((thing->flags & MF_CORPSE && thing->type != MT_MINOTAUR
        && thing->type != MT_SORCERER1 && thing->type != MT_SORCERER2)
        || thing->info->spawnstate == S_MOSS1             // Moss 1
        || thing->info->spawnstate == S_MOSS2             // Moss 2
        || thing->info->spawnstate == S_HANGINGCORPSE     // Hanging Corpse (51)
        || thing->info->spawnstate == S_SKULLHANG70_1     // Hanging Skull 1 (17)
        || thing->info->spawnstate == S_SKULLHANG60_1     // Hanging Skull 2 (24)
        || thing->info->spawnstate == S_SKULLHANG45_1     // Hanging Skull 3 (25)
        || thing->info->spawnstate == S_SKULLHANG35_1)    // Hanging Skull 4 (26)
        {
            if (thing->health & 1)
            {
                flip = true;
            }
        }
    }

    if (flip)
    {
        vis->startfrac = spritewidth[lump] - 1;
        vis->xiscale = -iscale;
    }
    else
    {
        vis->startfrac = 0;
        vis->xiscale = iscale;
    }
    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);
    vis->patch = lump;
//
// get light level
//

//      if (thing->flags & MF_SHADOW)
//              vis->colormap = NULL;                   // shadow draw
//      else ...

    if (fixedcolormap)
        vis->colormap = fixedcolormap;  // fixed map
    else if (thing->frame & FF_FULLBRIGHT)
        vis->colormap = colormaps;      // full bright
    else
    {                           // diminished light
        index = xscale >> (LIGHTSCALESHIFT - detailshift + hires);
        if (index >= MAXLIGHTSCALE)
            index = MAXLIGHTSCALE - 1;
        vis->colormap = spritelights[index];

        // [JN] Applying brightmaps to sprites...
        if (brightmaps && !vanillaparm)
        {
            // - Green only -
            if (thing->type == MT_ARTIEGG ||   // Morph Ovum
                thing->type == MT_AMCBOWHEFTY) // Quiver of Etherial Arrows
                vis->colormap = fullbrights_greenonly[index];

            // - Red only -
            if (thing->type == MT_AMSKRDWIMPY  || // Lesser Runes
                thing->type == MT_AMSKRDHEFTY  || // Greater Runes
                thing->type == MT_ARTITELEPORT || // Chaos Device
                thing->type == MT_MUMMYSOUL    || // Golem's freed ghost
                thing->type == MT_HEAD)           // Iron Lich
                vis->colormap = fullbrights_redonly[index];

            // - Blue only -
            if (thing->type == MT_SORCERER1 ||  // D'Sparil on Serpent
                thing->type == MT_SORCERER2 ||  // D'Sparil walking
                thing->type == MT_SOR2TELEFADE) // D'Sparil teleporting
                vis->colormap = fullbrights_blueonly[index];

            // - Not bronze -
            if (thing->type == MT_ARTIINVULNERABILITY) // Ring of Invulnerability
                vis->colormap = fullbrights_notbronze[index];

            // - Purple only -
            if (thing->type == MT_WIZARD) // Disciple of D'Sparil
                vis->colormap = fullbrights_purpleonly[index];

            // - Flame -
            if (thing->type == MT_AMPHRDWIMPY || // Flame Orb
                thing->type == MT_AMPHRDHEFTY || // Inferno Orb
                thing->type == MT_MISC4       || // Torch (Artifact)
                thing->type == MT_CHANDELIER  || // Chandelier
                thing->type == MT_MISC10      || // Torch
                thing->type == MT_SERPTORCH   || // Serpent Torch
                thing->type == MT_MISC6       || // Fire Brazier
                thing->type == MT_MISC12      || // Volcano
                thing->info->deathstate == S_CLINK_DIE1) // Sabreclaw's death sequence
                vis->colormap = fullbrights_flame[index];

            // - Green only (diminished) -
            if (thing->type == MT_MISC15    || // Etherial Crossbow
                thing->type == MT_AMCBOWWIMPY) // Etherial Arrows
                vis->colormap = fullbrights_greenonly_dim[index];

            if (thing->type == MT_KNIGHT    || // Undead Warrior
                thing->type == MT_KNIGHTGHOST) // Undead Warrior Ghost
                vis->colormap = fullbrights_greenonly_dim[index];

            // - Red only (diminished) -
            if (thing->type == MT_WSKULLROD   || // Hellstaff
                thing->type == MT_WPHOENIXROD || // Phoenix Rod
                thing->type == MT_ITEMSHIELD2)   // Enchanted Shield
                vis->colormap = fullbrights_redonly_dim[index];

            // - Blue only -
            if (thing->type == MT_AMBLSRWIMPY || // Claw Orb
                thing->type == MT_AMBLSRHEFTY)   // Energy Orb
                vis->colormap = fullbrights_blueonly_dim[index];

            // - Yellow only -
            if (thing->type == MT_AMGWNDWIMPY || // Wand Crystal
                thing->type == MT_AMGWNDHEFTY)   // Crystal Geode
                vis->colormap = fullbrights_yellowonly_dim[index];

            // - Standard full bright formula -
            if (thing->type == MT_BEASTBALL    || // Weredragon's fireball
                thing->type == MT_BURNBALL     || // Weredragon's fireball
                thing->type == MT_BURNBALLFB   || // Weredragon's fireball
                thing->type == MT_PUFFY        || // Weredragon's fireball
                thing->type == MT_HEADFX3      || // Iron Lich's fire column
                thing->type == MT_VOLCANOBLAST || // Volcano blast
                thing->type == MT_VOLCANOTBLAST)  // Volcano blast (impact)
                vis->colormap = colormaps;
        }
        
        // [JN] Fallback. If we are not using brightmaps, apply full brightness
        // to the objects, that no longer lighten up in info.c:
        // (S_FIREBRAZIER* and S_WALLTORCH*).
        if (!brightmaps || vanillaparm)
        {
            if (thing->type == MT_MISC4  || // Torch (Artifact)
                thing->type == MT_MISC6  || // S_FIREBRAZIER*
                thing->type == MT_MISC10)   // S_WALLTORCH*
                vis->colormap = colormaps;
        }
    }

    // [JN] Colored blood
    if (colored_blood && !vanillaparm
    &&  thing->type == MT_BLOODSPLATTER && thing->target)
    {
        if (thing->target->type == MT_WIZARD)
        vis->translation = cr[CR_RED2MAGENTA_HERETIC];

        else if (thing->target->type == MT_HEAD)
        vis->translation = cr[CR_RED2GRAY_HERETIC];
    }
}


/*
========================
=
= R_AddSprites
=
========================
*/

void R_AddSprites(sector_t * sec)
{
    mobj_t *thing;
    int lightnum;

    if (sec->validcount == validcount)
        return;                 // already added

    sec->validcount = validcount;

    lightnum = ((sec->lightlevel + level_brightness) >> LIGHTSEGSHIFT) + extralight;
    if (lightnum < 0)
    {
        spritelights = scalelight[0];

        // [JN] Brightmaps
        fullbrights_greenonly = fullbright_greenonly[0];
        fullbrights_redonly = fullbright_redonly[0];
        fullbrights_blueonly = fullbright_blueonly[0];
        fullbrights_purpleonly = fullbright_purpleonly[0];
        fullbrights_notbronze = fullbright_notbronze[0];
        fullbrights_flame = fullbright_flame[0];
        fullbrights_greenonly_dim = fullbright_greenonly_dim[0];
        fullbrights_redonly_dim = fullbright_redonly_dim[0];
        fullbrights_blueonly_dim = fullbright_blueonly_dim[0];
        fullbrights_yellowonly_dim = fullbright_yellowonly_dim[0];
        fullbrights_ethereal = fullbright_ethereal[0];
    }
    else if (lightnum >= LIGHTLEVELS)
    {
        spritelights = scalelight[LIGHTLEVELS - 1];

        // [JN] Brightmaps
        fullbrights_greenonly = fullbright_greenonly[LIGHTLEVELS - 1];
        fullbrights_redonly = fullbright_redonly[LIGHTLEVELS - 1];
        fullbrights_blueonly = fullbright_blueonly[LIGHTLEVELS - 1];
        fullbrights_purpleonly = fullbright_purpleonly[LIGHTLEVELS - 1];
        fullbrights_notbronze = fullbright_notbronze[LIGHTLEVELS - 1];
        fullbrights_flame = fullbright_flame[LIGHTLEVELS - 1];
        fullbrights_greenonly_dim = fullbright_greenonly_dim[LIGHTLEVELS - 1];
        fullbrights_redonly_dim = fullbright_redonly_dim[LIGHTLEVELS - 1];
        fullbrights_blueonly_dim = fullbright_blueonly_dim[LIGHTLEVELS - 1];
        fullbrights_yellowonly_dim = fullbright_yellowonly_dim[LIGHTLEVELS - 1];
        fullbrights_ethereal = fullbright_ethereal[LIGHTLEVELS - 1];
    }
    else
    {
        spritelights = scalelight[lightnum];

        // [JN] Brightmaps
        fullbrights_greenonly = fullbright_greenonly[lightnum];
        fullbrights_redonly = fullbright_redonly[lightnum];
        fullbrights_blueonly = fullbright_blueonly[lightnum];
        fullbrights_purpleonly = fullbright_purpleonly[lightnum];
        fullbrights_notbronze = fullbright_notbronze[lightnum];
        fullbrights_flame = fullbright_flame[lightnum];
        fullbrights_greenonly_dim = fullbright_greenonly_dim[lightnum];
        fullbrights_redonly_dim = fullbright_redonly_dim[lightnum];
        fullbrights_blueonly_dim = fullbright_blueonly_dim[lightnum];
        fullbrights_yellowonly_dim = fullbright_yellowonly_dim[lightnum];
        fullbrights_ethereal = fullbright_ethereal[lightnum];
    }

    for (thing = sec->thinglist; thing; thing = thing->snext)
        R_ProjectSprite(thing);
}

// [crispy] apply bobbing (or centering) to the player's weapon sprite
static inline void R_ApplyWeaponBob (fixed_t *sx, boolean bobx, fixed_t *sy, boolean boby)
{
	const angle_t angle = (128 * leveltime) & FINEMASK;

	if (sx)
	{
		*sx = FRACUNIT;

		if (bobx)
		{
			 *sx += FixedMul(viewplayer->bob, finecosine[angle]);
		}
	}

	if (sy)
	{
		*sy = 32 * FRACUNIT; // [crispy] WEAPONTOP

		if (boby)
		{
			*sy += FixedMul(viewplayer->bob, finesine[angle & (FINEANGLES / 2 - 1)]);
		}
	}
}

// [crispy] & [JN] Halfed bobbing amplitude while firing
static inline void R_ApplyWeaponFiringBob (fixed_t *sx, boolean bobx, fixed_t *sy, boolean boby)
{
    const angle_t angle = (128 * leveltime) & FINEMASK;

    if (sx)
    {
        *sx = FRACUNIT;
    
        if (bobx)
        {
            *sx += FixedMul(viewplayer->bob, finecosine[angle] / 2);
        }
    }

    if (sy)
    {
        *sy = 32 * FRACUNIT; // [crispy] WEAPONTOP
    
        if (boby)
        {
            *sy += FixedMul(viewplayer->bob, finesine[angle & (FINEANGLES / 2 - 1)] / 2);
        }
    }
}

// [crispy] & [JN] Chicken's special bobbing
static inline void R_ApplyChickenBob (fixed_t *sx, boolean bobx, fixed_t *sy, boolean boby)
{
    const angle_t angle = (128 * leveltime) & FINEMASK;

    if (sx)
    {
        *sx = FRACUNIT;
    
        if (bobx)
        *sx += FixedMul(viewplayer->bob, finecosine[angle] / 18);
    }

    if (sy)
    {
        *sy = 32 * FRACUNIT; // [crispy] WEAPONTOP
    
        if (boby)
        *sy += FixedMul(viewplayer->bob, finesine[angle & (FINEANGLES / 2 - 1)] / 6);
    }
}

/*
========================
=
= R_DrawPSprite
=
========================
*/

int PSpriteSY[NUMWEAPONS] = {
    0,                          // staff
    5 * FRACUNIT,               // goldwand
    15 * FRACUNIT,              // crossbow
    15 * FRACUNIT,              // blaster
    15 * FRACUNIT,              // skullrod
    15 * FRACUNIT,              // phoenix rod
    15 * FRACUNIT,              // mace
    15 * FRACUNIT,              // gauntlets
    15 * FRACUNIT               // beak
};

void R_DrawPSprite(pspdef_t * psp)
{
    fixed_t tx;
    int x1, x2;
    spritedef_t *sprdef;
    spriteframe_t *sprframe;
    int lump;
    boolean flip;
    vissprite_t *vis, avis;
    fixed_t psp_sx = psp->sx, psp_sy = psp->sy;
    // [JN] We need to define what "state" actually is (from Crispy)
    const int state = viewplayer->psprites[ps_weapon].state - states;

    int tempangle;

//
// decide which patch to use
//
#ifdef RANGECHECK
    if ((unsigned) psp->state->sprite >= numsprites)
        I_Error(english_language ?
                "R_ProjectSprite: invalid sprite number %i " :
                "R_ProjectSprite: некорректный номер спрайта %i ",
                psp->state->sprite);
#endif
    sprdef = &sprites[psp->state->sprite];
#ifdef RANGECHECK
    if ((psp->state->frame & FF_FRAMEMASK) >= sprdef->numframes)
        I_Error(english_language ?
                "R_ProjectSprite: invalid sprite frame %i : %i " :
                "R_ProjectSprite: некорректный фрейм спрайта %i : %i ",
                psp->state->sprite, psp->state->frame);
#endif
    sprframe = &sprdef->spriteframes[psp->state->frame & FF_FRAMEMASK];

    lump = sprframe->lump[0];
    flip = (boolean)sprframe->flip[0] ^ flip_levels;

    // [JN] Applying standard bobbing for animation interpolation (Staff+, Gauntlets+),
    // and for preventing "shaking" between refiring states. "Plus" means activated Tome of Power:
    if (singleplayer && weapon_bobbing && !vanillaparm && (
        /* Staff+       */ state == S_STAFFREADY2_1    || state == S_STAFFREADY2_2    || state == S_STAFFREADY2_3    ||
        /* Gauntlets+   */ state == S_GAUNTLETREADY2_1 || state == S_GAUNTLETREADY2_2 || state == S_GAUNTLETREADY2_3 ||
        /* CrBow        */ state == S_CRBOWATK1_6      || state == S_CRBOWATK1_7      || state == S_CRBOWATK1_8      ||
        /* CrBow+       */ state == S_CRBOWATK2_6      || state == S_CRBOWATK2_7      || state == S_CRBOWATK2_8      ||
        /* HellStaff+   */ state == S_HORNRODATK2_5    || state == S_HORNRODATK2_6    || state == S_HORNRODATK2_7    || state == S_HORNRODATK2_8 || state == S_HORNRODATK2_9 ||
        /* Phoenix Rod  */ state == S_PHOENIXATK1_4    || state == S_PHOENIXATK1_5    ||
        /* Phoenix Rod+ */ state == S_PHOENIXATK2_4    ||
        /* Firemace+    */ state == S_MACEATK2_4))
        {
            R_ApplyWeaponBob(&psp_sx, true, &psp_sy, true);
        }

    // [JN] Applying halfed bobbing while firing:
    if (singleplayer && weapon_bobbing && !vanillaparm && (
        /* Gauntlets    */ state == S_GAUNTLETATK1_1 || state == S_GAUNTLETATK1_2 || state == S_GAUNTLETATK1_3 || state == S_GAUNTLETATK1_4 || state == S_GAUNTLETATK1_5 || state == S_GAUNTLETATK1_6 || state == S_GAUNTLETATK1_7 ||
        /* Gauntlets+   */ state == S_GAUNTLETATK2_1 || state == S_GAUNTLETATK2_2 || state == S_GAUNTLETATK2_3 || state == S_GAUNTLETATK2_4 || state == S_GAUNTLETATK2_5 || state == S_GAUNTLETATK2_6 || state == S_GAUNTLETATK2_7 ||
        /* Staff        */ state == S_STAFFATK1_1    || state == S_STAFFATK1_2    || state == S_STAFFATK1_3    ||
        /* Staff+       */ state == S_STAFFATK2_1    || state == S_STAFFATK2_2    || state == S_STAFFATK2_3    ||
        /* Wand         */ state == S_GOLDWANDATK1_1 || state == S_GOLDWANDATK1_2 || state == S_GOLDWANDATK1_3 || state == S_GOLDWANDATK1_4 ||
        /* Wand+        */ state == S_GOLDWANDATK2_1 || state == S_GOLDWANDATK2_2 || state == S_GOLDWANDATK2_3 || state == S_GOLDWANDATK2_4 ||
        /* CrBow        */ state == S_CRBOWATK1_1    || state == S_CRBOWATK1_2    || state == S_CRBOWATK1_3    || state == S_CRBOWATK1_4    || state == S_CRBOWATK1_5    ||
        /* CrBow+       */ state == S_CRBOWATK2_1    || state == S_CRBOWATK2_2    || state == S_CRBOWATK2_3    || state == S_CRBOWATK2_4    || state == S_CRBOWATK2_5    ||
        /* DrClaw       */ state == S_BLASTERATK1_1  || state == S_BLASTERATK1_2  || state == S_BLASTERATK1_3  || state == S_BLASTERATK1_4  || state == S_BLASTERATK1_5  || state == S_BLASTERATK1_6  ||
        /* DrClaw+      */ state == S_BLASTERATK2_1  || state == S_BLASTERATK2_2  || state == S_BLASTERATK2_3  || state == S_BLASTERATK2_4  || state == S_BLASTERATK2_5  || state == S_BLASTERATK2_6  ||
        /* HlStaff      */ state == S_HORNRODATK1_1  || state == S_HORNRODATK1_2  || state == S_HORNRODATK1_3  ||
        /* HlStaff+     */ state == S_HORNRODATK2_1  || state == S_HORNRODATK2_2  || state == S_HORNRODATK2_3  || state == S_HORNRODATK2_4  ||
        /* Phoenix Rod  */ state == S_PHOENIXATK1_1  || state == S_PHOENIXATK1_2  || state == S_PHOENIXATK1_3  ||
        /* Phoenix Rod+ */ state == S_PHOENIXATK2_1  || state == S_PHOENIXATK2_2  || state == S_PHOENIXATK2_3  ||
        /* Firemace     */ state == S_MACEATK1_1     || state == S_MACEATK1_2     || state == S_MACEATK1_3     || state == S_MACEATK1_4     || state == S_MACEATK1_5     || state == S_MACEATK1_6     || state == S_MACEATK1_7     || state == S_MACEATK1_8 || state == S_MACEATK1_9 || state == S_MACEATK1_10 ||
        /* Firemace+    */ state == S_MACEATK2_1     || state == S_MACEATK2_2     || state == S_MACEATK2_3))
        {
            R_ApplyWeaponFiringBob(&psp_sx, true, &psp_sy, true);
        }

    // [JN] Applying special chicken's bobbing:
    if (singleplayer && weapon_bobbing && !vanillaparm && (state == S_BEAKREADY || state == S_BEAKATK1_1 || state == S_BEAKATK2_1))
        R_ApplyChickenBob(&psp_sx, true, &psp_sy, true);

    // [crispy] squat down weapon sprite a bit after hitting the ground
    if (weapon_bobbing && !vanillaparm)
    psp_sy += abs(viewplayer->psp_dy);
        
//
// calculate edges of the shape
//
    tx = psp_sx - 160 * FRACUNIT;

    // [crispy] fix sprite offsets for mirrored sprites
    tx -= flip ? 2 * tx - spriteoffset[lump] + spritewidth[lump] :
                                               spriteoffset[lump];
    if (viewangleoffset)
    {
        tempangle =
            ((centerxfrac / 1024) * (viewangleoffset >> ANGLETOFINESHIFT));
    }
    else
    {
        tempangle = 0;
    }
    x1 = (centerxfrac + FixedMul(tx, pspritescale) + tempangle) >> FRACBITS;
    if (x1 > viewwidth)
        return;                 // off the right side
    tx += spritewidth[lump];
    x2 = ((centerxfrac + FixedMul(tx, pspritescale) +
           tempangle) >> FRACBITS) - 1;
    if (x2 < 0)
        return;                 // off the left side

//
// store information in a vissprite
//
    vis = &avis;
    vis->translation = NULL;
    vis->mobjflags = 0;
    vis->psprite = true;
    // [crispy] weapons drawn 1 pixel too high when player is idle
    vis->texturemid = (BASEYCENTER<<FRACBITS)
                    + FRACUNIT/4-(psp_sy-spritetopoffset[lump]);
    if (viewheight == SCREENHEIGHT)
    {
        vis->texturemid -= PSpriteSY[players[consoleplayer].readyweapon];
    }
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth - 1 : x2;
    vis->scale = pspritescale << detailshift;
    if (flip)
    {
        vis->xiscale = -pspriteiscale;
        vis->startfrac = spritewidth[lump] - 1;
    }
    else
    {
        vis->xiscale = pspriteiscale;
        vis->startfrac = 0;
    }
    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);
    vis->patch = lump;

    if (viewplayer->powers[pw_invisibility] > 4 * 32 ||
        viewplayer->powers[pw_invisibility] & 8)
    {
        // [JN] Fixed vanilla bug: translucent HUD weapons 
        // should also be affected by yellow invulnerability palette.
        // Invisibility
        vis->colormap = fixedcolormap ? fixedcolormap : 
                                        spritelights[MAXLIGHTSCALE - 1];
        vis->mobjflags |= MF_SHADOW;
    }
    else if (fixedcolormap)
    {
        // Fixed color
        vis->colormap = fixedcolormap;
    }
    else if (psp->state->frame & FF_FULLBRIGHT)
    {
        // Full bright
        vis->colormap = colormaps;
    }
    else
    {
        // local light
        vis->colormap = spritelights[MAXLIGHTSCALE - 1];
    }
    R_DrawVisSprite(vis, vis->x1, vis->x2);
}

/*
========================
=
= R_DrawPlayerSprites
=
========================
*/

void R_DrawPlayerSprites(void)
{
    int i, lightnum;
    pspdef_t *psp;
    // [JN] We need to define what "state" actually is (from Crispy)
    const int state = viewplayer->psprites[ps_weapon].state - states;

//
// get light level
//
    lightnum = ((viewplayer->mo->subsector->sector->lightlevel 
             + level_brightness) >> LIGHTSEGSHIFT) + extralight;
    if (lightnum < 0)
    {
        spritelights = scalelight[0];

        // [JN] Applying brightmaps to HUD weapons...
        if (brightmaps && !vanillaparm)
        {
            // Staff+
            if (state == S_STAFFDOWN2 || state == S_STAFFUP2
            ||  state == S_STAFFREADY2_1 || state == S_STAFFREADY2_2
            ||  state == S_STAFFREADY2_3 || state == S_STAFFATK2_1
            ||  state == S_STAFFATK2_2 || state == S_STAFFATK2_3)
            spritelights = fullbright_blueonly[0];
            // Gauntlets
            else
            if (state == S_GAUNTLETATK1_1 || state == S_GAUNTLETATK1_2
            ||  state == S_GAUNTLETATK1_3 || state == S_GAUNTLETATK1_4
            ||  state == S_GAUNTLETATK1_5 || state == S_GAUNTLETATK1_6
            ||  state == S_GAUNTLETATK1_7)
            spritelights = fullbright_greenonly[0];
            // Gauntlets+
            else
            if (state == S_GAUNTLETDOWN2 || state == S_GAUNTLETUP2
            ||  state == S_GAUNTLETATK2_1 || state == S_GAUNTLETATK2_2
            ||  state == S_GAUNTLETATK2_3 || state == S_GAUNTLETATK2_4
            ||  state == S_GAUNTLETATK2_5 || state == S_GAUNTLETATK2_6
            ||  state == S_GAUNTLETATK2_7)
            spritelights = fullbright_redonly[0];
            // Wand
            else
            if (state == S_GOLDWANDATK1_1 || state == S_GOLDWANDATK1_2
            ||  state == S_GOLDWANDATK1_3 || state == S_GOLDWANDATK1_4)
            spritelights = fullbright_flame[0];
            // Wand+
            else
            if (state == S_GOLDWANDATK2_1 || state == S_GOLDWANDATK2_2
            ||  state == S_GOLDWANDATK2_3 || state == S_GOLDWANDATK2_4)
            spritelights = fullbright_flame[0];
            // Crossbow
            else
            if (state == S_CRBOWDOWN || state == S_CRBOWUP
            ||  state == S_CRBOW1 || state == S_CRBOW2
            ||  state == S_CRBOW3 || state == S_CRBOW4
            ||  state == S_CRBOW5 || state == S_CRBOW6
            ||  state == S_CRBOW7 || state == S_CRBOW8
            ||  state == S_CRBOW9 || state == S_CRBOW10
            ||  state == S_CRBOW11 || state == S_CRBOW12
            ||  state == S_CRBOW13 || state == S_CRBOW14
            ||  state == S_CRBOW15 || state == S_CRBOW16
            ||  state == S_CRBOW17 || state == S_CRBOW18
            ||  state == S_CRBOWATK1_1 || state == S_CRBOWATK1_2
            ||  state == S_CRBOWATK1_3 || state == S_CRBOWATK1_4
            ||  state == S_CRBOWATK1_5 || state == S_CRBOWATK1_6
            ||  state == S_CRBOWATK1_7 || state == S_CRBOWATK1_8)
            spritelights = fullbright_ethereal[0];
            // Crossbow+
            else
            if (state == S_CRBOWATK2_1 || state == S_CRBOWATK2_2
            ||  state == S_CRBOWATK2_3 || state == S_CRBOWATK2_4
            ||  state == S_CRBOWATK2_5 || state == S_CRBOWATK2_6
            ||  state == S_CRBOWATK2_7 || state == S_CRBOWATK2_8)
            spritelights = fullbright_ethereal[0];
            // Dragon Claw
            else
            if (state == S_BLASTERREADY || state == S_BLASTERDOWN
            ||  state == S_BLASTERUP || state == S_BLASTERATK1_1
            ||  state == S_BLASTERATK1_2 || state == S_BLASTERATK1_3
            ||  state == S_BLASTERATK1_4 || state == S_BLASTERATK1_5
            ||  state == S_BLASTERATK1_6)
            spritelights = fullbright_blueonly[0];
            // Dragon Claw+
            else
            if (state == S_BLASTERATK2_1 || state == S_BLASTERATK2_2
            ||  state == S_BLASTERATK2_3 || state == S_BLASTERATK2_4
            ||  state == S_BLASTERATK2_5 || state == S_BLASTERATK2_6)
            spritelights = fullbright_blueonly[0];    
            // Hell Staff 
            else
            if (state == S_HORNRODATK1_1 || state == S_HORNRODATK1_2
            ||  state == S_HORNRODATK1_3)
            spritelights = fullbright_redonly[0];
            // Hell Staff+
            else
            if (state == S_HORNRODATK2_1 || state == S_HORNRODATK2_2
            ||  state == S_HORNRODATK2_3 || state == S_HORNRODATK2_4
            ||  state == S_HORNRODATK2_5 || state == S_HORNRODATK2_6
            ||  state == S_HORNRODATK2_7 || state == S_HORNRODATK2_8
            ||  state == S_HORNRODATK2_9)
            spritelights = fullbright_redonly[0];
            // Phoenix Rod
            else
            if (state == S_PHOENIXATK1_1 || state == S_PHOENIXATK1_2
            ||  state == S_PHOENIXATK1_3 || state == S_PHOENIXATK1_4)
            spritelights = fullbright_flame[0];    
            // Phoenix Rod+
            else
            if (state == S_PHOENIXATK2_1 || state == S_PHOENIXATK2_3
            ||  state == S_PHOENIXATK2_4)
            spritelights = fullbright_flame[0];
            // Phoenix Rod's red gem
            else
            if (state == S_PHOENIXREADY || state == S_PHOENIXDOWN
            ||  state == S_PHOENIXUP)
            spritelights = fullbright_redonly[0];
        }
    }
    else if (lightnum >= LIGHTLEVELS)
    {
        spritelights = scalelight[LIGHTLEVELS - 1];

        // [JN] Applying brightmaps to HUD weapons...
        if (brightmaps && !vanillaparm)
        {
            if (brightmaps && !vanillaparm)
            {
                // Staff+
                if (state == S_STAFFDOWN2 ||  state == S_STAFFUP2
                ||  state == S_STAFFREADY2_1 || state == S_STAFFREADY2_2
                ||  state == S_STAFFREADY2_3 || state == S_STAFFATK2_1
                ||  state == S_STAFFATK2_2 || state == S_STAFFATK2_3)
                spritelights = fullbright_blueonly[LIGHTLEVELS - 1];
                // Gauntlets
                else
                if (state == S_GAUNTLETATK1_1 || state == S_GAUNTLETATK1_2
                ||  state == S_GAUNTLETATK1_3 || state == S_GAUNTLETATK1_4
                ||  state == S_GAUNTLETATK1_5 || state == S_GAUNTLETATK1_6
                ||  state == S_GAUNTLETATK1_7)
                spritelights = fullbright_greenonly[LIGHTLEVELS - 1];
                // Gauntlets+
                else
                if (state == S_GAUNTLETDOWN2 || state == S_GAUNTLETUP2
                ||  state == S_GAUNTLETATK2_1 || state == S_GAUNTLETATK2_2
                ||  state == S_GAUNTLETATK2_3 || state == S_GAUNTLETATK2_4
                ||  state == S_GAUNTLETATK2_5 || state == S_GAUNTLETATK2_6
                ||  state == S_GAUNTLETATK2_7)
                spritelights = fullbright_redonly[LIGHTLEVELS - 1];
                // Wand
                else
                if (state == S_GOLDWANDATK1_1 || state == S_GOLDWANDATK1_2
                ||  state == S_GOLDWANDATK1_3 || state == S_GOLDWANDATK1_4)
                spritelights = fullbright_flame[LIGHTLEVELS - 1];
                // Wand+
                else
                if (state == S_GOLDWANDATK2_1 || state == S_GOLDWANDATK2_2
                ||  state == S_GOLDWANDATK2_3 || state == S_GOLDWANDATK2_4)
                spritelights = fullbright_flame[LIGHTLEVELS - 1];
                // Crossbow
                else
                if (state == S_CRBOWDOWN || state == S_CRBOWUP
                ||  state == S_CRBOW1 || state == S_CRBOW2
                ||  state == S_CRBOW3 || state == S_CRBOW4
                ||  state == S_CRBOW5 || state == S_CRBOW6
                ||  state == S_CRBOW7 || state == S_CRBOW8
                ||  state == S_CRBOW9 || state == S_CRBOW10
                ||  state == S_CRBOW11 || state == S_CRBOW12
                ||  state == S_CRBOW13 || state == S_CRBOW14
                ||  state == S_CRBOW15 || state == S_CRBOW16
                ||  state == S_CRBOW17 || state == S_CRBOW18
                ||  state == S_CRBOWATK1_1 || state == S_CRBOWATK1_2
                ||  state == S_CRBOWATK1_3 || state == S_CRBOWATK1_4
                ||  state == S_CRBOWATK1_5 || state == S_CRBOWATK1_6
                ||  state == S_CRBOWATK1_7 || state == S_CRBOWATK1_8)
                spritelights = fullbright_ethereal[LIGHTLEVELS - 1];
                // Crossbow+
                else
                if (state == S_CRBOWATK2_1 || state == S_CRBOWATK2_2
                ||  state == S_CRBOWATK2_3 || state == S_CRBOWATK2_4
                ||  state == S_CRBOWATK2_5 || state == S_CRBOWATK2_6
                ||  state == S_CRBOWATK2_7 || state == S_CRBOWATK2_8)
                spritelights = fullbright_ethereal[LIGHTLEVELS - 1];
                // Dragon Claw
                else
                if (state == S_BLASTERREADY || state == S_BLASTERDOWN
                ||  state == S_BLASTERUP || state == S_BLASTERATK1_1
                ||  state == S_BLASTERATK1_2 || state == S_BLASTERATK1_3
                ||  state == S_BLASTERATK1_4 || state == S_BLASTERATK1_5
                ||  state == S_BLASTERATK1_6)
                spritelights = fullbright_blueonly[LIGHTLEVELS - 1];
                // Dragon Claw+
                else
                if (state == S_BLASTERATK2_1 || state == S_BLASTERATK2_2
                ||  state == S_BLASTERATK2_3 || state == S_BLASTERATK2_4
                ||  state == S_BLASTERATK2_5 || state == S_BLASTERATK2_6)
                spritelights = fullbright_blueonly[LIGHTLEVELS - 1];
                // Hell Staff 
                else
                if (state == S_HORNRODATK1_1 || state == S_HORNRODATK1_2
                ||  state == S_HORNRODATK1_3)
                spritelights = fullbright_redonly[LIGHTLEVELS - 1];
                // Hell Staff+
                else
                if (state == S_HORNRODATK2_1 || state == S_HORNRODATK2_2
                ||  state == S_HORNRODATK2_3 || state == S_HORNRODATK2_4
                ||  state == S_HORNRODATK2_5 || state == S_HORNRODATK2_6
                ||  state == S_HORNRODATK2_7 || state == S_HORNRODATK2_8
                ||  state == S_HORNRODATK2_9)
                spritelights = fullbright_redonly[LIGHTLEVELS - 1];
                // Phoenix Rod
                else
                if (state == S_PHOENIXATK1_1 || state == S_PHOENIXATK1_2
                ||  state == S_PHOENIXATK1_3 || state == S_PHOENIXATK1_4)
                spritelights = fullbright_flame[LIGHTLEVELS - 1];
                // Phoenix Rod+
                else
                if (state == S_PHOENIXATK2_1 || state == S_PHOENIXATK2_3
                ||  state == S_PHOENIXATK2_4)
                spritelights = fullbright_flame[LIGHTLEVELS - 1];
                // Phoenix Rod's red gem
                else
                if (state == S_PHOENIXREADY || state == S_PHOENIXDOWN
                ||  state == S_PHOENIXUP)
                spritelights = fullbright_redonly[LIGHTLEVELS - 1];
            }
        }
    }
    else
    {
        spritelights = scalelight[lightnum];

        // [JN] Applying brightmaps to HUD weapons...
        if (brightmaps && !vanillaparm)
        {
            // Staff+
            if (state == S_STAFFDOWN2 || state == S_STAFFUP2
            ||  state == S_STAFFREADY2_1 || state == S_STAFFREADY2_2
            ||  state == S_STAFFREADY2_3 || state == S_STAFFATK2_1
            ||  state == S_STAFFATK2_2 || state == S_STAFFATK2_3)
            spritelights = fullbright_blueonly[lightnum];
            // Gauntlets
            else
            if (state == S_GAUNTLETATK1_1 || state == S_GAUNTLETATK1_2
            ||  state == S_GAUNTLETATK1_3 || state == S_GAUNTLETATK1_4
            ||  state == S_GAUNTLETATK1_5 || state == S_GAUNTLETATK1_6
            ||  state == S_GAUNTLETATK1_7)
            spritelights = fullbright_greenonly[lightnum];
            // Gauntlets+
            else
            if (state == S_GAUNTLETDOWN2 || state == S_GAUNTLETUP2
            ||  state == S_GAUNTLETATK2_1 || state == S_GAUNTLETATK2_2
            ||  state == S_GAUNTLETATK2_3 || state == S_GAUNTLETATK2_4
            ||  state == S_GAUNTLETATK2_5 || state == S_GAUNTLETATK2_6
            ||  state == S_GAUNTLETATK2_7)
            spritelights = fullbright_redonly[lightnum];
            // Wand
            else
            if (state == S_GOLDWANDATK1_1 || state == S_GOLDWANDATK1_2
            ||  state == S_GOLDWANDATK1_3 || state == S_GOLDWANDATK1_4)
            spritelights = fullbright_flame[lightnum];
            // Wand+
            else
            if (state == S_GOLDWANDATK2_1 || state == S_GOLDWANDATK2_2
            ||  state == S_GOLDWANDATK2_3 || state == S_GOLDWANDATK2_4)
            spritelights = fullbright_flame[lightnum];
            // Crossbow
            else
            if (state == S_CRBOWDOWN || state == S_CRBOWUP
            ||  state == S_CRBOW1 || state == S_CRBOW2
            ||  state == S_CRBOW3 || state == S_CRBOW4
            ||  state == S_CRBOW5 || state == S_CRBOW6
            ||  state == S_CRBOW7 || state == S_CRBOW8
            ||  state == S_CRBOW9 || state == S_CRBOW10
            ||  state == S_CRBOW11 || state == S_CRBOW12
            ||  state == S_CRBOW13 || state == S_CRBOW14
            ||  state == S_CRBOW15 || state == S_CRBOW16
            ||  state == S_CRBOW17 || state == S_CRBOW18
            ||  state == S_CRBOWATK1_1 || state == S_CRBOWATK1_2
            ||  state == S_CRBOWATK1_3 || state == S_CRBOWATK1_4
            ||  state == S_CRBOWATK1_5 || state == S_CRBOWATK1_6
            ||  state == S_CRBOWATK1_7 || state == S_CRBOWATK1_8)
            spritelights = fullbright_ethereal[lightnum];
            // Crossbow+
            else
            if (state == S_CRBOWATK2_1 || state == S_CRBOWATK2_2
            ||  state == S_CRBOWATK2_3 || state == S_CRBOWATK2_4
            ||  state == S_CRBOWATK2_5 || state == S_CRBOWATK2_6
            ||  state == S_CRBOWATK2_7 || state == S_CRBOWATK2_8)
            spritelights = fullbright_ethereal[lightnum];
            // Dragon Claw
            else
            if (state == S_BLASTERREADY || state == S_BLASTERDOWN
            ||  state == S_BLASTERUP || state == S_BLASTERATK1_1
            ||  state == S_BLASTERATK1_2 || state == S_BLASTERATK1_3
            ||  state == S_BLASTERATK1_4 || state == S_BLASTERATK1_5
            ||  state == S_BLASTERATK1_6)
            spritelights = fullbright_blueonly[lightnum];
            // Dragon Claw+
            else
            if (state == S_BLASTERATK2_1 || state == S_BLASTERATK2_2
            ||  state == S_BLASTERATK2_3 || state == S_BLASTERATK2_4
            ||  state == S_BLASTERATK2_5 || state == S_BLASTERATK2_6)
            spritelights = fullbright_blueonly[lightnum];
            // Hell Staff 
            else
            if (state == S_HORNRODATK1_1 || state == S_HORNRODATK1_2
            ||  state == S_HORNRODATK1_3)
            spritelights = fullbright_redonly[lightnum];
            // Hell Staff+
            else
            if (state == S_HORNRODATK2_1 || state == S_HORNRODATK2_2
            ||  state == S_HORNRODATK2_3 || state == S_HORNRODATK2_4
            ||  state == S_HORNRODATK2_5 || state == S_HORNRODATK2_6
            ||  state == S_HORNRODATK2_7 || state == S_HORNRODATK2_8
            ||  state == S_HORNRODATK2_9)
            spritelights = fullbright_redonly[lightnum];
            // Phoenix Rod
            else
            if (state == S_PHOENIXATK1_1 || state == S_PHOENIXATK1_2
            ||  state == S_PHOENIXATK1_3 || state == S_PHOENIXATK1_4)
            spritelights = fullbright_flame[lightnum];
            // Phoenix Rod+
            else
            if (state == S_PHOENIXATK2_1 || state == S_PHOENIXATK2_3
            ||  state == S_PHOENIXATK2_4)
            spritelights = fullbright_flame[lightnum];
            // Phoenix Rod's red gem
            else
            if (state == S_PHOENIXREADY || state == S_PHOENIXDOWN
            ||  state == S_PHOENIXUP)
            spritelights = fullbright_redonly[lightnum];
        }
    }

    // [JN] Fallback. If we are not using brightmaps, apply full brightness
    // to the objects, that no longer lighten up in info.c:
    // (S_GAUNTLETATK1_3-5 and S_GAUNTLETATK2_3-5).
    if (!brightmaps || vanillaparm)
    {
        if (state == S_GAUNTLETATK1_3 || state == S_GAUNTLETATK1_4
        ||  state == S_GAUNTLETATK1_5 || state == S_GAUNTLETATK2_3
        ||  state == S_GAUNTLETATK2_4 || state == S_GAUNTLETATK2_5)
        spritelights = scalelight[LIGHTLEVELS-1]; 
    }
//
// clip to screen bounds
//
    mfloorclip = screenheightarray;
    mceilingclip = negonearray;

//
// add all active psprites
//
    for (i = 0, psp = viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
        if (psp->state)
            R_DrawPSprite(psp);

}


/*
========================
=
= R_SortVisSprites
=
========================
*/

vissprite_t vsprsortedhead;

void R_SortVisSprites(void)
{
    int i, count;
    vissprite_t *ds, *best;
    vissprite_t unsorted;
    fixed_t bestscale;

    count = vissprite_p - vissprites;

    unsorted.next = unsorted.prev = &unsorted;
    if (!count)
        return;

    for (ds = vissprites; ds < vissprite_p; ds++)
    {
        ds->next = ds + 1;
        ds->prev = ds - 1;
    }
    vissprites[0].prev = &unsorted;
    unsorted.next = &vissprites[0];
    (vissprite_p - 1)->next = &unsorted;
    unsorted.prev = vissprite_p - 1;

//
// pull the vissprites out by scale
//
    best = 0;                   // shut up the compiler warning
    vsprsortedhead.next = vsprsortedhead.prev = &vsprsortedhead;
    for (i = 0; i < count; i++)
    {
        bestscale = INT_MAX;
        for (ds = unsorted.next; ds != &unsorted; ds = ds->next)
        {
            if (ds->scale < bestscale)
            {
                bestscale = ds->scale;
                best = ds;
            }
        }
        best->next->prev = best->prev;
        best->prev->next = best->next;
        best->next = &vsprsortedhead;
        best->prev = vsprsortedhead.prev;
        vsprsortedhead.prev->next = best;
        vsprsortedhead.prev = best;
    }
}



/*
========================
=
= R_DrawSprite
=
========================
*/

void R_DrawSprite(vissprite_t * spr)
{
    drawseg_t *ds;
    int clipbot[WIDESCREENWIDTH]; // [crispy] 32-bit integer math
    int cliptop[WIDESCREENWIDTH]; // [crispy] 32-bit integer math
    int x, r1, r2;
    fixed_t scale, lowscale;
    int silhouette;

    for (x = spr->x1; x <= spr->x2; x++)
        clipbot[x] = cliptop[x] = -2;

//
// scan drawsegs from end to start for obscuring segs
// the first drawseg that has a greater scale is the clip seg
//
    for (ds = ds_p - 1; ds >= drawsegs; ds--)
    {
        //
        // determine if the drawseg obscures the sprite
        //
        if (ds->x1 > spr->x2 || ds->x2 < spr->x1 ||
            (!ds->silhouette && !ds->maskedtexturecol))
            continue;           // doesn't cover sprite

        r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
        r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;
        if (ds->scale1 > ds->scale2)
        {
            lowscale = ds->scale2;
            scale = ds->scale1;
        }
        else
        {
            lowscale = ds->scale1;
            scale = ds->scale2;
        }

        if (scale < spr->scale || (lowscale < spr->scale
                                   && !R_PointOnSegSide(spr->gx, spr->gy,
                                                        ds->curline)))
        {
            if (ds->maskedtexturecol)   // masked mid texture
                R_RenderMaskedSegRange(ds, r1, r2);
            continue;           // seg is behind sprite
        }

//
// clip this piece of the sprite
//
        silhouette = ds->silhouette;
        if (spr->gz >= ds->bsilheight)
            silhouette &= ~SIL_BOTTOM;
        if (spr->gzt <= ds->tsilheight)
            silhouette &= ~SIL_TOP;

        if (silhouette == 1)
        {                       // bottom sil
            for (x = r1; x <= r2; x++)
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];
        }
        else if (silhouette == 2)
        {                       // top sil
            for (x = r1; x <= r2; x++)
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
        }
        else if (silhouette == 3)
        {                       // both
            for (x = r1; x <= r2; x++)
            {
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
            }
        }

    }

//
// all clipping has been performed, so draw the sprite
//

// check for unclipped columns
    for (x = spr->x1; x <= spr->x2; x++)
    {
        if (clipbot[x] == -2)
            clipbot[x] = viewheight;
        if (cliptop[x] == -2)
            cliptop[x] = -1;
    }

    mfloorclip = clipbot;
    mceilingclip = cliptop;
    R_DrawVisSprite(spr, spr->x1, spr->x2);
}


/*
========================
=
= R_DrawMasked
=
========================
*/

void R_DrawMasked(void)
{
    vissprite_t *spr;
    drawseg_t *ds;

    R_SortVisSprites();

    if (vissprite_p > vissprites)
    {
        // draw all vissprites back to front

        for (spr = vsprsortedhead.next; spr != &vsprsortedhead;
             spr = spr->next)
            R_DrawSprite(spr);
    }

//
// render any remaining masked mid textures
//
    for (ds = ds_p - 1; ds >= drawsegs; ds--)
        if (ds->maskedtexturecol)
            R_RenderMaskedSegRange(ds, ds->x1, ds->x2);

//
// draw the psprites on top of everything
//
// Added for the sideviewing with an external device
    if (viewangleoffset <= 1024 << ANGLETOFINESHIFT || viewangleoffset >=
        -1024 << ANGLETOFINESHIFT)
    {                           // don't draw on side views
        R_DrawPlayerSprites();
    }
}
