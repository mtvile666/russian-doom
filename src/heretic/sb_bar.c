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
// SB_bar.c



#include <time.h>
#include "doomdef.h"
#include "deh_str.h"
#include "i_video.h"
#include "i_swap.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "v_video.h"
#include "v_trans.h"
#include "rd_lang.h"
#include "jn.h"

// Types

typedef struct Cheat_s
{
    void (*func) (player_t * player, struct Cheat_s * cheat);
    cheatseq_t *seq;
} Cheat_t;

// Private Functions

static void DrawSoundInfo(void);
static void ShadeLine(int x, int y, int height, int shade);
static void ShadeChain(void);
static void DrINumber(signed int val, int x, int y);
static void DrBNumber(signed int val, int x, int y);
static void DrawCommonBar(void);
static void DrawMainBar(void);
static void DrawInventoryBar(void);
static void DrawFullScreenStuff(void);
static boolean HandleCheats(byte key);
static void CheatGodFunc(player_t * player, Cheat_t * cheat);
static void CheatNoClipFunc(player_t * player, Cheat_t * cheat);
static void CheatWeaponsFunc(player_t * player, Cheat_t * cheat);
static void CheatPowerFunc(player_t * player, Cheat_t * cheat);
static void CheatHealthFunc(player_t * player, Cheat_t * cheat);
static void CheatKeysFunc(player_t * player, Cheat_t * cheat);
static void CheatSoundFunc(player_t * player, Cheat_t * cheat);
static void CheatTickerFunc(player_t * player, Cheat_t * cheat);
static void CheatArtifact1Func(player_t * player, Cheat_t * cheat);
static void CheatArtifact2Func(player_t * player, Cheat_t * cheat);
static void CheatArtifact3Func(player_t * player, Cheat_t * cheat);
static void CheatWarpFunc(player_t * player, Cheat_t * cheat);
static void CheatChickenFunc(player_t * player, Cheat_t * cheat);
static void CheatMassacreFunc(player_t * player, Cheat_t * cheat);
static void CheatIDKFAFunc(player_t * player, Cheat_t * cheat);
static void CheatIDDQDFunc(player_t * player, Cheat_t * cheat);
static void CheatVERSIONFunc(player_t * player, Cheat_t * cheat);

// Public Data

boolean DebugSound;             // debug flag for displaying sound info

boolean inventory;
int curpos;
int inv_ptr;
int ArtifactFlash;

static int DisplayTicker = 0;

// Private Data

static int HealthMarker;
static int ChainWiggle;
static player_t *CPlayer;
// [JN] lumps for PALFIX (1) and PLAYPAL (2)
int playpalette1, playpalette2;

patch_t *PatchLTFACE;
patch_t *PatchRTFACE;
patch_t *PatchBARBACK;
patch_t *PatchCHAIN;
patch_t *PatchSTATBAR;
patch_t *PatchSTATBAR_RUS;
patch_t *PatchLIFEGEM;
patch_t *PatchLTFCTOP;
patch_t *PatchRTFCTOP;
patch_t *PatchSELECTBOX;
patch_t *PatchINVLFGEM1;
patch_t *PatchINVLFGEM2;
patch_t *PatchINVRTGEM1;
patch_t *PatchINVRTGEM2;
patch_t *PatchINumbers[10];
patch_t *PatchNEGATIVE;
patch_t *PatchSmNumbers[10];
patch_t *PatchBLACKSQ;
patch_t *PatchINVBAR;
patch_t *PatchARMCLEAR;
patch_t *PatchCHAINBACK;
int FontBNumBase;
int spinbooklump;
int spinflylump;

// Toggle god mode
cheatseq_t CheatGodSeq = CHEAT("quicken", 0);

// Toggle no clipping mode
cheatseq_t CheatNoClipSeq = CHEAT("kitty", 0);

// Get all weapons and ammo
cheatseq_t CheatWeaponsSeq = CHEAT("rambo", 0);

// Toggle tome of power
cheatseq_t CheatPowerSeq = CHEAT("shazam", 0);

// Get full health
cheatseq_t CheatHealthSeq = CHEAT("ponce", 0);

// Get all keys
cheatseq_t CheatKeysSeq = CHEAT("skel", 0);

// Toggle sound debug info
cheatseq_t CheatSoundSeq = CHEAT("noise", 0);

// Toggle ticker
cheatseq_t CheatTickerSeq = CHEAT("ticker", 0);

// Get an artifact 1st stage (ask for type)
cheatseq_t CheatArtifact1Seq = CHEAT("gimme", 0);

// Get an artifact 2nd stage (ask for count)
cheatseq_t CheatArtifact2Seq = CHEAT("gimme", 1);

// Get an artifact final stage
cheatseq_t CheatArtifact3Seq = CHEAT("gimme", 2);

// Warp to new level
cheatseq_t CheatWarpSeq = CHEAT("engage", 2);

// Save a screenshot
cheatseq_t CheatChickenSeq = CHEAT("cockadoodledoo", 0);

// Kill all monsters
cheatseq_t CheatMassacreSeq = CHEAT("massacre", 0);

cheatseq_t CheatIDKFASeq = CHEAT("idkfa", 0);
cheatseq_t CheatIDDQDSeq = CHEAT("iddqd", 0);

// [JN] Russian Doom "VERSION" cheat code
cheatseq_t CheatVERSIONSeq = CHEAT("version", 0);

static Cheat_t Cheats[] = {
    {CheatGodFunc,       &CheatGodSeq},
    {CheatNoClipFunc,    &CheatNoClipSeq},
    {CheatWeaponsFunc,   &CheatWeaponsSeq},
    {CheatPowerFunc,     &CheatPowerSeq},
    {CheatHealthFunc,    &CheatHealthSeq},
    {CheatKeysFunc,      &CheatKeysSeq},
    {CheatSoundFunc,     &CheatSoundSeq},
    {CheatTickerFunc,    &CheatTickerSeq},
    {CheatArtifact1Func, &CheatArtifact1Seq},
    {CheatArtifact2Func, &CheatArtifact2Seq},
    {CheatArtifact3Func, &CheatArtifact3Seq},
    {CheatWarpFunc,      &CheatWarpSeq},
    {CheatChickenFunc,   &CheatChickenSeq},
    {CheatMassacreFunc,  &CheatMassacreSeq},
    {CheatIDKFAFunc,     &CheatIDKFASeq},
    {CheatIDDQDFunc,     &CheatIDDQDSeq},
    {CheatVERSIONFunc,   &CheatVERSIONSeq},
    {NULL,               NULL} 
};

//---------------------------------------------------------------------------
//
// PROC SB_Init
//
//---------------------------------------------------------------------------

void SB_Init(void)
{
    int i;
    int startLump;

    PatchLTFACE = W_CacheLumpName(DEH_String("LTFACE"), PU_STATIC);
    PatchRTFACE = W_CacheLumpName(DEH_String("RTFACE"), PU_STATIC);
    PatchBARBACK = W_CacheLumpName(DEH_String("BARBACK"), PU_STATIC);
    PatchINVBAR = W_CacheLumpName(DEH_String("INVBAR"), PU_STATIC);
    PatchCHAIN = W_CacheLumpName(DEH_String("CHAIN"), PU_STATIC);
    if (deathmatch)
    {
        PatchSTATBAR = W_CacheLumpName(DEH_String("STATBAR"), PU_STATIC);
        PatchSTATBAR_RUS = W_CacheLumpName(DEH_String("RD_STBAR"), PU_STATIC);
    }
    else
    {
        PatchSTATBAR = W_CacheLumpName(DEH_String("LIFEBAR"), PU_STATIC);
        PatchSTATBAR_RUS = W_CacheLumpName(DEH_String("RD_LFBAR"), PU_STATIC);
    }
    if (!netgame)
    {                           // single player game uses red life gem
        PatchLIFEGEM = W_CacheLumpName(DEH_String("LIFEGEM2"), PU_STATIC);
    }
    else
    {
        PatchLIFEGEM = W_CacheLumpNum(W_GetNumForName(DEH_String("LIFEGEM0"))
                                      + consoleplayer, PU_STATIC);
    }
    PatchLTFCTOP = W_CacheLumpName(DEH_String("LTFCTOP"), PU_STATIC);
    PatchRTFCTOP = W_CacheLumpName(DEH_String("RTFCTOP"), PU_STATIC);
    PatchSELECTBOX = W_CacheLumpName(DEH_String("SELECTBOX"), PU_STATIC);
    PatchINVLFGEM1 = W_CacheLumpName(DEH_String("INVGEML1"), PU_STATIC);
    PatchINVLFGEM2 = W_CacheLumpName(DEH_String("INVGEML2"), PU_STATIC);
    PatchINVRTGEM1 = W_CacheLumpName(DEH_String("INVGEMR1"), PU_STATIC);
    PatchINVRTGEM2 = W_CacheLumpName(DEH_String("INVGEMR2"), PU_STATIC);
    PatchBLACKSQ = W_CacheLumpName(DEH_String("BLACKSQ"), PU_STATIC);
    PatchARMCLEAR = W_CacheLumpName(DEH_String("ARMCLEAR"), PU_STATIC);
    PatchCHAINBACK = W_CacheLumpName(DEH_String("CHAINBACK"), PU_STATIC);
    startLump = W_GetNumForName(DEH_String("IN0"));
    for (i = 0; i < 10; i++)
    {
        PatchINumbers[i] = W_CacheLumpNum(startLump + i, PU_STATIC);
    }
    PatchNEGATIVE = W_CacheLumpName(DEH_String("NEGNUM"), PU_STATIC);
    FontBNumBase = W_GetNumForName(DEH_String("FONTB16"));
    startLump = W_GetNumForName(DEH_String("SMALLIN0"));
    for (i = 0; i < 10; i++)
    {
        PatchSmNumbers[i] = W_CacheLumpNum(startLump + i, PU_STATIC);
    }
    playpalette1 = W_GetNumForName(DEH_String("PALFIX"));
    playpalette2 = W_GetNumForName(DEH_String("PLAYPAL"));
    spinbooklump = W_GetNumForName(DEH_String("SPINBK0"));
    spinflylump = W_GetNumForName(DEH_String("SPFLY0"));
}

//---------------------------------------------------------------------------
//
// PROC SB_Ticker
//
//---------------------------------------------------------------------------

void SB_Ticker(void)
{
    int delta;
    int curHealth;

    curHealth = players[consoleplayer].mo->health;

    if (leveltime & 1 && curHealth > 0)
    {
        ChainWiggle = P_Random() & 1;
    }
    if (curHealth < 0)
    {
        curHealth = 0;
    }
    if (curHealth < HealthMarker)
    {
        delta = (HealthMarker - curHealth) >> 2;
        if (delta < 1)
        {
            delta = 1;
        }
        else if (delta > 8)
        {
            delta = 8;
        }
        HealthMarker -= delta;
    }
    else if (curHealth > HealthMarker)
    {
        delta = (curHealth - HealthMarker) >> 2;
        if (delta < 1)
        {
            delta = 1;
        }
        else if (delta > 8)
        {
            delta = 8;
        }
        HealthMarker += delta;
    }
}

//---------------------------------------------------------------------------
//
// PROC DrINumber
//
// Draws a three digit number.
//
//---------------------------------------------------------------------------

static void DrINumber(signed int val, int x, int y)
{
    patch_t *patch;
    int oldval;

    oldval = val;
    if (val < 0)
    {
        if (val < -9)
        {
            // [JN] Negative health: Leave "LAME" sign for Deathmatch
            if (deathmatch)
            {
                V_DrawPatch(x + 1, y + 1, W_CacheLumpName(DEH_String("LAME"), PU_CACHE));
            }
            // [JN] Negative health: -10 and below routine
            else if (negative_health && !vanillaparm)
            {
                // [JN] Can't draw -100 and below
                if (val <= -99)
                val = -99;

                val = -val % 100;
                if (val < 9 || oldval < 99)
                {
                    patch = PatchINumbers[val / 10];
                    V_DrawPatch(x + 9, y, patch);
                }
                val = val % 10;
                patch = PatchINumbers[val];
                V_DrawPatch(x + 18, y, patch);

                V_DrawPatch(x + 1, y, PatchNEGATIVE);
            }
        }
        else
        {
            val = -val;
            V_DrawPatch(x + 18, y, PatchINumbers[val]);
            V_DrawPatch(x + 9, y, PatchNEGATIVE);
        }
        return;
    }
    if (val > 99)
    {
        patch = PatchINumbers[val / 100];
        V_DrawPatch(x, y, patch);
    }
    val = val % 100;
    if (val > 9 || oldval > 99)
    {
        patch = PatchINumbers[val / 10];
        V_DrawPatch(x + 9, y, patch);
    }
    val = val % 10;
    patch = PatchINumbers[val];
    V_DrawPatch(x + 18, y, patch);

    // [JN] Reset color translation if colored hud activated
    if (colored_hud)
    dp_translation = NULL;
}

//---------------------------------------------------------------------------
//
// PROC DrBNumber
//
// Draws a three digit number using FontB
//
//---------------------------------------------------------------------------

static void DrBNumber(signed int val, int x, int y)
{
    patch_t *patch;
    patch_t *patch_n;
    int xpos;
    int oldval;

    // [JN] Declare a "minus" symbol in the big green font
    patch_n = W_CacheLumpName(DEH_String("FONTB13"), PU_CACHE);

    oldval = val;
    xpos = x;
    if (val < 0)
    {
        val = -val; // [JN] Support for negative values
        
        if (-val <= -99) // [JN] Do not drop below -99. Looks confusing, eh?
        val = 99;

        // [JN] Draw minus symbol with respection of digits placement.
        // However, values below -10 requires some correction in "x" placement.
        V_DrawShadowedPatch(xpos + (val <= 9 ? 16 : 8) - SHORT(patch_n->width)
                                                       / 2, y-1, patch_n);
    }
    if (val > 99)
    {
        patch = W_CacheLumpNum(FontBNumBase + val / 100, PU_CACHE);
        V_DrawShadowedPatch(xpos + 6 - SHORT(patch->width) / 2, y, patch);
    }
    val = val % 100;
    xpos += 12;
    if (val > 9 || oldval > 99)
    {
        patch = W_CacheLumpNum(FontBNumBase + val / 10, PU_CACHE);
        V_DrawShadowedPatch(xpos + 6 - SHORT(patch->width) / 2, y, patch);
    }
    val = val % 10;
    xpos += 12;
    patch = W_CacheLumpNum(FontBNumBase + val, PU_CACHE);
    V_DrawShadowedPatch(xpos + 6 - SHORT(patch->width) / 2, y, patch);

    // [JN] Reset color translation if colored hud activated
    if (colored_hud)
    dp_translation = NULL;
}

//---------------------------------------------------------------------------
//
// PROC DrSmallNumber
//
// Draws a small two digit number.
//
//---------------------------------------------------------------------------

static void DrSmallNumber(int val, int x, int y)
{
    patch_t *patch;

    if (val == 1)
    {
        return;
    }
    if (val > 9)
    {
        patch = PatchSmNumbers[val / 10];
        V_DrawPatch(x, y, patch);
    }
    val = val % 10;
    patch = PatchSmNumbers[val];
    V_DrawPatch(x + 4, y, patch);
}

//---------------------------------------------------------------------------
//
// PROC ShadeLine
//
//---------------------------------------------------------------------------

static void ShadeLine(int x, int y, int height, int shade)
{
    byte *dest;
    byte *shades;

    x <<= hires;
    y <<= hires;
    height <<= hires;

    shades = colormaps + 9 * 256 + shade * 2 * 256;
    dest = I_VideoBuffer + y * screenwidth + x;
    while (height--)
    {
        if (hires)
           *(dest + 1) = *(shades + *dest);
        *(dest) = *(shades + *dest);
        dest += screenwidth;
    }
}

//---------------------------------------------------------------------------
//
// PROC ShadeChain
//
//---------------------------------------------------------------------------

static void ShadeChain(void)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        ShadeLine(277 + i + wide_delta, 190, 10, i / 2);
        ShadeLine(19 + i + wide_delta, 190, 10, 7 - (i / 2));
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawSoundInfo
//
// Displays sound debugging information.
//
//---------------------------------------------------------------------------

static void DrawSoundInfo(void)
{
    int i;
    SoundInfo_t s;
    ChanInfo_t *c;
    char text[32];
    int x;
    int y;
    int xPos[7] = { 1, 75, 112, 156, 200, 230, 260 };
    extern int snd_Channels;

    if (leveltime & 16)
    {
        if (english_language)
        {
            MN_DrTextSmallENG(DEH_String("*** SOUND DEBUG INFO ***"), xPos[0]
                                                                    + wide_delta, 20);
        }
        else
        {
            // *** ОТЛАДОЧНАЯ ИНФОРМАЦИЯ О ЗВУКЕ ***
            MN_DrTextSmallRUS(DEH_String("*** JNKFLJXYFZ BYAJHVFWBZ J PDERT ***"), xPos[0]
                                                                                 + wide_delta, 20);
        }
    }
    S_GetChannelInfo(&s);
    if (s.channelCount == 0)
    {
        return;
    }
    x = 0;
    MN_DrTextA(DEH_String("NAME"), xPos[x++] + wide_delta, 30);
    MN_DrTextA(DEH_String("MO.T"), xPos[x++] + wide_delta, 30);
    MN_DrTextA(DEH_String("MO.X"), xPos[x++] + wide_delta, 30);
    MN_DrTextA(DEH_String("MO.Y"), xPos[x++] + wide_delta, 30);
    MN_DrTextA(DEH_String("ID"), xPos[x++] + wide_delta, 30);
    MN_DrTextA(DEH_String("PRI"), xPos[x++] + wide_delta, 30);
    MN_DrTextA(DEH_String("DIST"), xPos[x++] + wide_delta, 30);
    for (i = 0; i < s.channelCount; i++)
    {
        c = &s.chan[i];
        x = 0;
        y = 40 + i * 10;
        if (c->mo == NULL)
        {                       // Channel is unused
            // [JN] Draw only for 8 channes max, otherwise lines will be
            // drawing beneath status bar and game will crash.
            if (snd_Channels <= 8)
            {
                MN_DrTextA(DEH_String("------"), xPos[0] + wide_delta, y);
            }
            continue;
        }
        M_snprintf(text, sizeof(text), "%s", c->name);
        M_ForceUppercase(text);
        MN_DrTextA(text, xPos[x++] + wide_delta, y);
        M_snprintf(text, sizeof(text), "%d", c->mo->type);
        MN_DrTextA(text, xPos[x++] + wide_delta, y);
        M_snprintf(text, sizeof(text), "%d", c->mo->x >> FRACBITS);
        MN_DrTextA(text, xPos[x++] + wide_delta, y);
        M_snprintf(text, sizeof(text), "%d", c->mo->y >> FRACBITS);
        MN_DrTextA(text, xPos[x++] + wide_delta, y);
        M_snprintf(text, sizeof(text), "%d", c->id);
        MN_DrTextA(text, xPos[x++] + wide_delta, y);
        M_snprintf(text, sizeof(text), "%d", c->priority);
        MN_DrTextA(text, xPos[x++] + wide_delta, y);
        M_snprintf(text, sizeof(text), "%d", c->distance);
        MN_DrTextA(text, xPos[x++] + wide_delta, y);
    }
    UpdateState |= I_FULLSCRN;
    BorderNeedRefresh = true;
}

//---------------------------------------------------------------------------
//
// PROC SB_Drawer
//
//---------------------------------------------------------------------------

char patcharti[][10] = {
    {"ARTIBOX"},                // none
    {"ARTIINVU"},               // invulnerability
    {"ARTIINVS"},               // invisibility
    {"ARTIPTN2"},               // health
    {"ARTISPHL"},               // superhealth
    {"ARTIPWBK"},               // tomeofpower
    {"ARTITRCH"},               // torch
    {"ARTIFBMB"},               // firebomb
    {"ARTIEGGC"},               // egg
    {"ARTISOAR"},               // fly
    {"ARTIATLP"}                // teleport
};

char ammopic[][10] = {
    {"INAMGLD"},
    {"INAMBOW"},
    {"INAMBST"},
    {"INAMRAM"},
    {"INAMPNX"},
    {"INAMLOB"}
};

int SB_state = -1;
static int oldarti = 0;
static int oldartiCount = 0;
static int oldfrags = -9999;
static int oldammo = -1;
static int oldarmor = -1;
static int oldweapon = -1;
static int oldhealth = -1;
static int oldlife = -1;
static int oldkeys = -1;

int playerkeys = 0;

extern boolean automapactive;


void SB_Drawer(void)
{
    int frame;
    static boolean hitCenterFrame;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    static char s[64];
    int f = real_fps;
    char fps [999];
    boolean wide_4_3 = widescreen && screenblocks == 9;

    // [JN] Draw extended skulls and stone border
    if ((widescreen && screenblocks <= 10) 
    ||  (widescreen && automapactive && !automap_overlay))
    {
        V_DrawPatch(0, 147, W_CacheLumpName(DEH_String("WDBARLF"), PU_CACHE));
        V_DrawPatch(344, 147, W_CacheLumpName(DEH_String("WDBARRT"), PU_CACHE));
    }

    // [JN] Draw horns separatelly in non wide screen mode
    if (!widescreen && screenblocks <= 10 && automapactive && automap_overlay)
    {
        V_DrawPatch(0 + wide_delta, 148, PatchLTFCTOP);
        V_DrawPatch(290 + wide_delta, 148, PatchRTFCTOP);
    }

    // [JN] Draw local time widget
    if (local_time)
    {

        strftime(s, sizeof(s), 
                 local_time == 1 ? "%I:%M %p" :    // 12-hour (HH:MM designation)
                 local_time == 2 ? "%I:%M:%S %p" : // 12-hour (HH:MM:SS designation)
                 local_time == 3 ? "%H:%M" :       // 24-hour (HH:MM)
                 local_time == 4 ? "%H:%M:%S" :    // 24-hour (HH:MM:SS)
                                   "", tm);        // No time

        MN_DrTextC(s, (local_time == 1 ? 281 :
                       local_time == 2 ? 269 :
                       local_time == 3 ? 293 :
                       local_time == 4 ? 281 : 0) 
                       + (wide_4_3 ? wide_delta : wide_delta*2), 13);
    }

    // [JN] Draw FPS widget
    if (show_fps)
    {
        sprintf (fps, "%d", f);
        MN_DrTextC("FPS:", 279 + (wide_4_3 ? wide_delta : wide_delta*2), 23);
        MN_DrTextC(fps, 297 + (wide_4_3 ? wide_delta : wide_delta*2), 23);   // [JN] fps digits
    }

    // Sound info debug stuff
    if (DebugSound == true)
    {
        DrawSoundInfo();
    }

    CPlayer = &players[consoleplayer];

    // [JN] Draw crosshair
    if (crosshair_draw && !automapactive && !vanillaparm)
    {
        if (crosshair_health)
        {
            dp_translation = CPlayer->health >= 67 ? NULL :
                             CPlayer->health >= 34 ? cr[CR_GREEN2GOLD_HERETIC] :
                                                     cr[CR_GREEN2RED_HERETIC];
        }

        if (crosshair_scale)
        {
            V_DrawPatch(origwidth/2,
                ((screenblocks <= 10) ? (ORIGHEIGHT-38)/2 : (ORIGHEIGHT+4)/2),
                W_CacheLumpName(DEH_String("XHAIR_1S"), PU_CACHE));
        }
        else
        {
            V_DrawPatchUnscaled(screenwidth/2,
                ((screenblocks <= 10) ? (SCREENHEIGHT-76)/2 : (SCREENHEIGHT+8)/2),
                W_CacheLumpName(DEH_String("XHAIR_1U"), PU_CACHE));
        }

        dp_translation = NULL;
    }

    if ((viewheight == SCREENHEIGHT && !automapactive) 
    ||  (viewheight == SCREENHEIGHT && automapactive && automap_overlay))
    {
        if (screenblocks == 11) // [JN] Draw only in 11 screen size, 12 is clean full screen
        DrawFullScreenStuff();
        SB_state = -1;
    }
    else
    {
        if (SB_state == -1)
        {
            V_DrawPatch(0 + wide_delta, 158, PatchBARBACK);
            oldhealth = -1;
        }
        DrawCommonBar();
        if (!inventory)
        {
            if (SB_state != 0)
            {
                // Main interface
                V_DrawPatch(34 + wide_delta, 160, 
                            english_language ? PatchSTATBAR : PatchSTATBAR_RUS);
                oldarti = 0;
                oldammo = -1;
                oldarmor = -1;
                oldweapon = -1;
                oldfrags = -9999;       //can't use -1, 'cuz of negative frags
                oldlife = -1;
                oldkeys = -1;
            }
            DrawMainBar();
            SB_state = 0;
        }
        else
        {
            if (SB_state != 1)
            {
                V_DrawPatch(34 + wide_delta, 160, PatchINVBAR);
            }
            DrawInventoryBar();
            SB_state = 1;
        }
    }
    SB_PaletteFlash();

    // [JN] Apply golden eyes to HUD gargoyles while Ring of Invincibility
    if ((screenblocks <= 10 || automapactive)
    && (players[consoleplayer].cheats & CF_GODMODE
    || (CPlayer->powers[pw_invulnerability] && !vanillaparm)))
    {
        V_DrawPatch(16 + wide_delta, 167,
                    W_CacheLumpName(DEH_String("GOD1"), PU_CACHE));
        V_DrawPatch(287 + wide_delta, 167,
                    W_CacheLumpName(DEH_String("GOD2"), PU_CACHE));

        SB_state = -1;
    }

    // Flight icons
    if (CPlayer->powers[pw_flight])
    {
        if (CPlayer->powers[pw_flight] > BLINKTHRESHOLD
            || !(CPlayer->powers[pw_flight] & 16))
        {
            frame = (leveltime / 3) & 15;
            if (CPlayer->mo->flags2 & MF2_FLY)
            {
                if (hitCenterFrame && (frame != 15 && frame != 0))
                {
                    V_DrawPatch(20 + (wide_4_3 ? wide_delta : 0), 17,
                                W_CacheLumpNum(spinflylump + 15, PU_CACHE));
                }
                else
                {
                    V_DrawPatch(20 + (wide_4_3 ? wide_delta : 0), 17,
                                W_CacheLumpNum(spinflylump + frame, PU_CACHE));
                    hitCenterFrame = false;
                }
            }
            else
            {
                if (!hitCenterFrame && (frame != 15 && frame != 0))
                {
                    V_DrawPatch(20 + (wide_4_3 ? wide_delta : 0), 17,
                                W_CacheLumpNum(spinflylump + frame, PU_CACHE));
                    hitCenterFrame = false;
                }
                else
                {
                    V_DrawPatch(20 + (wide_4_3 ? wide_delta : 0), 17,
                                W_CacheLumpNum(spinflylump + 15, PU_CACHE));
                    hitCenterFrame = true;
                }
            }
            BorderTopRefresh = true;
            UpdateState |= I_MESSAGES;
        }
        else
        {
            BorderTopRefresh = true;
            UpdateState |= I_MESSAGES;
        }
    }

    if (CPlayer->powers[pw_weaponlevel2] && !CPlayer->chickenTics)
    {
        if (CPlayer->powers[pw_weaponlevel2] > BLINKTHRESHOLD
            || !(CPlayer->powers[pw_weaponlevel2] & 16))
        {
            frame = (leveltime / 3) & 15;
            // [JN] Do not obstruct clock widget
            V_DrawPatch(300 + (wide_4_3 ? wide_delta : wide_delta*2), 17,
                        W_CacheLumpNum(spinbooklump + frame, PU_CACHE));
            BorderTopRefresh = true;
            UpdateState |= I_MESSAGES;
        }
        else
        {
            BorderTopRefresh = true;
            UpdateState |= I_MESSAGES;
        }
    }

    // [JN] Wide screen: draw black borders in emulated 4:3 mode.
    if ((widescreen && screenblocks == 9)
    ||  (widescreen && screenblocks == 9 && automapactive))
    {
        V_DrawBlackBorders();
    }
}

// sets the new palette based upon current values of player->damagecount
// and player->bonuscount
void SB_PaletteFlash(void)
{
    static int sb_palette = 0;
    int palette;
    byte *pal;

    CPlayer = &players[consoleplayer];

    if (CPlayer->damagecount)
    {
        palette = (CPlayer->damagecount + 7) >> 3;
        if (palette >= NUMREDPALS)
        {
            palette = NUMREDPALS - 1;
        }
        palette += STARTREDPALS;
    }
    // [crispy] never show the yellow bonus palette for a dead player
    else if (CPlayer->bonuscount && CPlayer->health > 0)
    {
        // [JN] One extra palette for pickup flashing
        // https://doomwiki.org/wiki/PLAYPAL
        palette = (CPlayer->bonuscount + 7) >> 3;
        if (palette >= NUMBONUSPALS)
        {
            palette = NUMBONUSPALS;
        }
        palette += STARTBONUSPALS-1;
    }
    else
    {
        palette = 0;
    }
    if (palette != sb_palette)
    {
        sb_palette = palette;
        pal = (byte *) W_CacheLumpNum(usegamma <= 8 ?
                                      playpalette1 :
                                      playpalette2,
                                      PU_CACHE) + palette * 768;
        I_SetPalette(pal);
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawCommonBar
//
//---------------------------------------------------------------------------

void DrawCommonBar(void)
{
    int chainY;
    int healthPos;

    V_DrawPatch(0 + wide_delta, 148, PatchLTFCTOP);
    V_DrawPatch(290 + wide_delta, 148, PatchRTFCTOP);

    if (oldhealth != HealthMarker)
    {
        oldhealth = HealthMarker;
        healthPos = HealthMarker;
        if (healthPos < 0)
        {
            healthPos = 0;
        }
        if (healthPos > 100)
        {
            healthPos = 100;
        }
        healthPos = (healthPos * 256) / 100;
        chainY =
            (HealthMarker == CPlayer->mo->health) ? 191 : 191 + ChainWiggle;
        V_DrawPatch(0 + wide_delta, 190, PatchCHAINBACK);
        V_DrawPatch(2 + (healthPos % 17) + wide_delta, chainY, PatchCHAIN);
        V_DrawPatch(17 + healthPos + wide_delta, chainY, PatchLIFEGEM);
        V_DrawPatch(0 + wide_delta, 190, PatchLTFACE);
        V_DrawPatch(276 + wide_delta, 190, PatchRTFACE);
        ShadeChain();
        UpdateState |= I_STATBAR;
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawMainBar
//
//---------------------------------------------------------------------------

void DrawMainBar(void)
{
    int i;
    int temp;

    // Ready artifact
    if (ArtifactFlash)
    {
        V_DrawPatch(180 + wide_delta, 161, PatchBLACKSQ);

        temp = W_GetNumForName(DEH_String("useartia")) + ArtifactFlash - 1;

        V_DrawPatch(182 + wide_delta, 161, W_CacheLumpNum(temp, PU_CACHE));
        ArtifactFlash--;
        oldarti = -1;   // so that the correct artifact fills in after the flash
        UpdateState |= I_STATBAR;
    }
    else if (oldarti != CPlayer->readyArtifact
             || oldartiCount != CPlayer->inventory[inv_ptr].count)
    {
        V_DrawPatch(180 + wide_delta, 161, PatchBLACKSQ);
        if (CPlayer->readyArtifact > 0)
        {
            V_DrawPatch(179 + wide_delta, 160,
                        W_CacheLumpName(DEH_String(patcharti[CPlayer->readyArtifact]),
                                        PU_CACHE));
            DrSmallNumber(CPlayer->inventory[inv_ptr].count, 201 + wide_delta, 182);
        }
        oldarti = CPlayer->readyArtifact;
        oldartiCount = CPlayer->inventory[inv_ptr].count;
        UpdateState |= I_STATBAR;
    }

    // Frags
    if (deathmatch)
    {
        temp = 0;
        for (i = 0; i < MAXPLAYERS; i++)
        {
            temp += CPlayer->frags[i];
        }

        // [JN] Always update frags value, needed for colored HUD
        // if (temp != oldfrags)
        {
            V_DrawPatch(57, 171, PatchARMCLEAR);

            // [JN] Colored HUD: Frags
            if (colored_hud && !vanillaparm)
            {
                if (temp < 0)
                dp_translation = cr[CR_GOLD2RED_HERETIC];
                else if (temp == 0)
                dp_translation = NULL;
                else
                dp_translation = cr[CR_GOLD2GREEN_HERETIC];
            }
            DrINumber(temp, 61 + wide_delta, 170);
            oldfrags = temp;
            UpdateState |= I_STATBAR;
        }
    }
    else
    {
        temp = HealthMarker;
        if (temp < 0)
        {
            temp = 0;
        }
        // [JN] Negative health: use actual value
        else if (negative_health && !vanillaparm)
        {
            temp = CPlayer->mo->health;
        }
        else if (temp > 100)
        {
            temp = 100;
        }

        // [JN] Always update health value, needed for colored HUD
        // if (oldlife != temp)
        {
            oldlife = temp;
            V_DrawPatch(57 + wide_delta, 171, PatchARMCLEAR);

            // [JN] Colored HUD: Health
            if (colored_hud && !vanillaparm)
            {
                if ((CPlayer->cheats & CF_GODMODE) || CPlayer->powers[pw_invulnerability])
                dp_translation = cr[CR_GOLD2GRAY_HERETIC];
                else if (CPlayer->mo->health >= 67)
                dp_translation = cr[CR_GOLD2GREEN_HERETIC];
                else if (CPlayer->mo->health >= 34)
                dp_translation = NULL;
                else
                dp_translation = cr[CR_GOLD2RED_HERETIC];
            }

            DrINumber(temp, 61 + wide_delta, 170);
            UpdateState |= I_STATBAR;
        }
    }

    // Keys
    if (oldkeys != playerkeys)
    {
        // [JN] Reset color translation if colored hud activated
        if (colored_hud)
        dp_translation = NULL;
        
        if (CPlayer->keys[key_yellow])
        {
            V_DrawPatch(153 + wide_delta, 164,
                        W_CacheLumpName(DEH_String("ykeyicon"), PU_CACHE));
        }
        if (CPlayer->keys[key_green])
        {
            V_DrawPatch(153 + wide_delta, 172,
                        W_CacheLumpName(DEH_String("gkeyicon"), PU_CACHE));
        }
        if (CPlayer->keys[key_blue])
        {
            V_DrawPatch(153 + wide_delta, 180,
                        W_CacheLumpName(DEH_String("bkeyicon"), PU_CACHE));
        }
        oldkeys = playerkeys;
        UpdateState |= I_STATBAR;
    }
    // Ammo
    temp = CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];
    if (oldammo != temp || oldweapon != CPlayer->readyweapon)
    {
        V_DrawPatch(108 + wide_delta, 161, PatchBLACKSQ);
        if (temp && CPlayer->readyweapon > 0 && CPlayer->readyweapon < 7)
        {
            // [JN] Colored HUD: Ammo
            if (colored_hud && !vanillaparm)
            {
                int ammo =  CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];
                int fullammo = maxammo[wpnlev1info[CPlayer->readyweapon].ammo];
                
                if (ammo < fullammo/4)
                dp_translation = cr[CR_GOLD2RED_HERETIC];
                else if (ammo < fullammo/2)
                dp_translation = NULL;
                else
                dp_translation = cr[CR_GOLD2GREEN_HERETIC]; 
            }
            
            DrINumber(temp, 109 + wide_delta, 162);
            V_DrawPatch(111 + wide_delta, 172,
                        W_CacheLumpName(DEH_String(ammopic[CPlayer->readyweapon - 1]),
                                        PU_CACHE));
        }
        oldammo = temp;
        oldweapon = CPlayer->readyweapon;
        UpdateState |= I_STATBAR;
    }

    // Armor
    // [JN] Always update armor value, needed for colored HUD
    // if (oldarmor != CPlayer->armorpoints)
    {
        V_DrawPatch(224 + wide_delta, 171, PatchARMCLEAR);

        // [JN] Colored HUD: Armor
        if (colored_hud && !vanillaparm)
        {
            if (CPlayer->cheats & CF_GODMODE || CPlayer->powers[pw_invulnerability])
            dp_translation = cr[CR_GOLD2GRAY_HERETIC];
            else if (CPlayer->armortype >= 2)
            dp_translation = cr[CR_GOLD2GREEN_HERETIC];
            else if (CPlayer->armortype == 1)
            dp_translation = NULL;
            else
            dp_translation = cr[CR_GOLD2RED_HERETIC];
        }

        DrINumber(CPlayer->armorpoints, 228 + wide_delta, 170);
        oldarmor = CPlayer->armorpoints;
        UpdateState |= I_STATBAR;
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawInventoryBar
//
//---------------------------------------------------------------------------

void DrawInventoryBar(void)
{
    char *patch;
    int i;
    int x;

    x = inv_ptr - curpos;
    UpdateState |= I_STATBAR;
    V_DrawPatch(34 + wide_delta, 160, PatchINVBAR);
    for (i = 0; i < 7; i++)
    {
        //V_DrawPatch(50+i*31, 160, W_CacheLumpName("ARTIBOX", PU_CACHE));
        if (CPlayer->inventorySlotNum > x + i
            && CPlayer->inventory[x + i].type != arti_none)
        {
            patch = DEH_String(patcharti[CPlayer->inventory[x + i].type]);

            V_DrawPatch(50 + i * 31 + wide_delta, 160,
                        W_CacheLumpName(patch, PU_CACHE));
            DrSmallNumber(CPlayer->inventory[x + i].count, 69 + i * 31
                                                              + wide_delta, 182);
        }
    }
    V_DrawPatch(50 + curpos * 31 + wide_delta, 189, PatchSELECTBOX);
    if (x != 0)
    {
        V_DrawPatch(38 + wide_delta, 159, !(leveltime & 4) ? PatchINVLFGEM1 :
                    PatchINVLFGEM2);
    }
    if (CPlayer->inventorySlotNum - x > 7)
    {
        V_DrawPatch(269 + wide_delta, 159, !(leveltime & 4) ?
                    PatchINVRTGEM1 : PatchINVRTGEM2);
    }
}

void DrawFullScreenStuff(void)
{
    char *patch;
    int i;
    int x;
    int temp;
    // [JN] Definition of full screen ammo
    int fs_ammo = CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];

    UpdateState |= I_FULLSCRN;

    if (CPlayer->mo->health > 0)
    {
        // [JN] Colored HUD: Health
        if (colored_hud && !vanillaparm)
        {
            if (CPlayer->cheats & CF_GODMODE || CPlayer->powers[pw_invulnerability])
            dp_translation = cr[CR_GREEN2GRAY_HERETIC];
            else if (CPlayer->mo->health >= 67)
            dp_translation = NULL;
            else if (CPlayer->mo->health >= 34)
            dp_translation = cr[CR_GREEN2GOLD_HERETIC];
            else
            dp_translation = cr[CR_GREEN2RED_HERETIC];
        }

        DrBNumber(CPlayer->mo->health, 5, 176);
    }
    // [JN] Negative and zero health: can't drop below -99, drawing, colorizing
    else if (CPlayer->mo->health <= 0)
    {
        if (colored_hud && !vanillaparm)
        dp_translation = cr[CR_GREEN2RED_HERETIC];

        if (CPlayer->mo->health <= -99)
        CPlayer->mo->health = -99;

        if (negative_health && !vanillaparm)
        DrBNumber(CPlayer->mo->health, 5, 176);
        else
        DrBNumber(0, 5, 176);
    }

    // [JN] Always draw ammo in full screen HUD
    if (fs_ammo && CPlayer->readyweapon > 0 && CPlayer->readyweapon < 7)
    {
        // [JN] Colored HUD: Health
        if (colored_hud && !vanillaparm)
        {
            int ammo =  CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];
            int fullammo = maxammo[wpnlev1info[CPlayer->readyweapon].ammo];

            if (ammo < fullammo/4)
            dp_translation = cr[CR_GREEN2RED_HERETIC];
            else if (ammo < fullammo/2)
            dp_translation = cr[CR_GREEN2GOLD_HERETIC];
            else
            dp_translation = NULL;
        }

        DrBNumber(fs_ammo, 274 + (wide_delta * 2), 176);
    }

    if (deathmatch)
    {
        temp = 0;

        // [JN] Do not draw frag counter below opened inventory, 
        // because it looks aesthetically bad.
        if (!inventory)
        {
            for (i = 0; i < MAXPLAYERS; i++)
            {
                if (playeringame[i])
                {
                    temp += CPlayer->frags[i];
                }
            }

            // [JN] Colored HUD: Frags
            if (colored_hud && !vanillaparm)
            {
                if (temp < 0)
                dp_translation = cr[CR_GREEN2RED_HERETIC];
                else if (temp == 0)
                dp_translation = cr[CR_GREEN2GOLD_HERETIC];
                else
                dp_translation = NULL;
            }

            DrBNumber(temp, 173 + (wide_delta * 2), 176);
        }
        
        // [JN] Always draw keys in Deathmatch, but only if player alive,
        // and not while opened inventory. Just as visual reminder.
        if (CPlayer->mo->health > 0 && !inventory)
        {
            V_DrawShadowedPatch(219 + (wide_delta * 2), 174,
                                W_CacheLumpName(DEH_String("ykeyicon"), PU_CACHE));
            V_DrawShadowedPatch(219 + (wide_delta * 2), 182,
                                W_CacheLumpName(DEH_String("gkeyicon"), PU_CACHE));
            V_DrawShadowedPatch(219 + (wide_delta * 2), 190,
                                W_CacheLumpName(DEH_String("bkeyicon"), PU_CACHE));
        }
    }
    if (!inventory)
    {
        // [JN] Draw health vial, representing player's health
        if (CPlayer->mo->health == 100)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT00", PU_CACHE));
        else if (CPlayer->mo->health >= 90)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT01", PU_CACHE));
        else if (CPlayer->mo->health >= 80)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT02", PU_CACHE));
        else if (CPlayer->mo->health >= 70)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT03", PU_CACHE));
        else if (CPlayer->mo->health >= 60)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT04", PU_CACHE));
        else if (CPlayer->mo->health >= 50)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT05", PU_CACHE));
        else if (CPlayer->mo->health >= 40)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT06", PU_CACHE));
        else if (CPlayer->mo->health >= 30)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT07", PU_CACHE));
        else if (CPlayer->mo->health >= 20)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT08", PU_CACHE));
        else if (CPlayer->mo->health >= 10)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT09", PU_CACHE));
        else if (CPlayer->mo->health >= 1)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT10", PU_CACHE));
        else if (CPlayer->mo->health <= 0)
        V_DrawShadowedPatch(46, 177, W_CacheLumpName("HUDPNT11", PU_CACHE));
        
        // [JN] Draw keys
        if (!deathmatch)
        {
            if (CPlayer->keys[key_yellow])
            V_DrawShadowedPatch(219 + (wide_delta * 2), 174,
                                W_CacheLumpName(DEH_String("ykeyicon"), PU_CACHE));
            if (CPlayer->keys[key_green])
            V_DrawShadowedPatch(219 + (wide_delta * 2), 182,
                                W_CacheLumpName(DEH_String("gkeyicon"), PU_CACHE));
            if (CPlayer->keys[key_blue])
            V_DrawShadowedPatch(219 + (wide_delta * 2), 190,
                                W_CacheLumpName(DEH_String("bkeyicon"), PU_CACHE));
        }

        if (CPlayer->readyArtifact > 0)
        {
            patch = DEH_String(patcharti[CPlayer->readyArtifact]);
            
            // [JN] Draw Artifacts
            V_DrawShadowedPatch(238 + (wide_delta * 2), 170,
                                W_CacheLumpName(patch, PU_CACHE));
            DrSmallNumber(CPlayer->inventory[inv_ptr].count, 259 + (wide_delta * 2), 191);
        }

        if (CPlayer->armorpoints > 0)
        {
            // [JN] Player have Silver Shield
            if (CPlayer->armortype == 1)
                V_DrawShadowedPatch(110, 214, W_CacheLumpName("SHLDA0", PU_CACHE));
            // [JN] Player have Enchanted Shield
            else if (CPlayer->armortype == 2)
                V_DrawShadowedPatch(108, 216, W_CacheLumpName("SHD2A0", PU_CACHE));

            // [JN] Colored HUD: Armor
            if (colored_hud && !vanillaparm)
            {
                if (CPlayer->cheats & CF_GODMODE || CPlayer->powers[pw_invulnerability])
                dp_translation = cr[CR_GREEN2GRAY_HERETIC];
                else if (CPlayer->armortype >= 2)
                dp_translation = NULL;    
                else if (CPlayer->armortype == 1)
                dp_translation = cr[CR_GREEN2GOLD_HERETIC];
                else
                dp_translation = cr[CR_GREEN2RED_HERETIC];
            }

            // [JN] Draw ammount of armor
            DrBNumber(CPlayer->armorpoints, 57, 176);
        }
    }
    else
    {
        x = inv_ptr - curpos;
        for (i = 0; i < 7; i++)
        {
            V_DrawTLPatch(50 + i * 31 + wide_delta, 168,
                          W_CacheLumpName(DEH_String("ARTIBOX"), PU_CACHE));
            if (CPlayer->inventorySlotNum > x + i
                && CPlayer->inventory[x + i].type != arti_none)
            {
                patch = DEH_String(patcharti[CPlayer->inventory[x + i].type]);
                V_DrawPatch(50 + i * 31 + wide_delta, 168,
                            W_CacheLumpName(patch, PU_CACHE));
                DrSmallNumber(CPlayer->inventory[x + i].count, 69 + i * 31 
                                                                  + wide_delta, 190);
            }
        }
        V_DrawPatch(50 + curpos * 31 + wide_delta, 197, PatchSELECTBOX);
        if (x != 0)
        {
            V_DrawPatch(38 + wide_delta, 167, !(leveltime & 4) ? PatchINVLFGEM1 :
                        PatchINVLFGEM2);
        }
        if (CPlayer->inventorySlotNum - x > 7)
        {
            V_DrawPatch(269 + wide_delta, 167, !(leveltime & 4) ?
                        PatchINVRTGEM1 : PatchINVRTGEM2);
        }
    }
}

//--------------------------------------------------------------------------
//
// FUNC SB_Responder
//
//--------------------------------------------------------------------------

boolean SB_Responder(event_t * event)
{
    if (event->type == ev_keydown)
    {
        if (HandleCheats(event->data1))
        {                       // Need to eat the key
            return (true);
        }
    }
    return (false);
}

//--------------------------------------------------------------------------
//
// FUNC HandleCheats
//
// Returns true if the caller should eat the key.
//
//--------------------------------------------------------------------------

static boolean HandleCheats(byte key)
{
    int i;
    boolean eat;

    if (netgame || gameskill == sk_nightmare)
    {                         // Can't cheat in a net-game, or in nightmare mode
        return (false);
    }
    if (players[consoleplayer].health <= 0)
    {                         // Dead players can't cheat
        return (false);
    }
    eat = false;
    for (i = 0; Cheats[i].func != NULL; i++)
    {
        if (cht_CheckCheat(Cheats[i].seq, key))
        {
            Cheats[i].func(&players[consoleplayer], &Cheats[i]);
            S_StartSound(NULL, sfx_dorcls);
        }
    }
    return (eat);
}

//--------------------------------------------------------------------------
//
// CHEAT FUNCTIONS
//
//--------------------------------------------------------------------------

static void CheatGodFunc(player_t * player, Cheat_t * cheat)
{
    player->cheats ^= CF_GODMODE;
    if (player->cheats & CF_GODMODE)
    {
        P_SetMessage(player, DEH_String(txt_cheatgodon), false);
    }
    else
    {
        P_SetMessage(player, DEH_String(txt_cheatgodoff), false);
    }
    SB_state = -1;
}

static void CheatNoClipFunc(player_t * player, Cheat_t * cheat)
{
    player->cheats ^= CF_NOCLIP;
    if (player->cheats & CF_NOCLIP)
    {
        P_SetMessage(player, DEH_String(txt_cheatnoclipon), false);
    }
    else
    {
        P_SetMessage(player, DEH_String(txt_cheatnoclipoff), false);
    }
}

static void CheatWeaponsFunc(player_t * player, Cheat_t * cheat)
{
    int i;
    //extern boolean *WeaponInShareware;

    player->armorpoints = 200;
    player->armortype = 2;
    if (!player->backpack)
    {
        for (i = 0; i < NUMAMMO; i++)
        {
            player->maxammo[i] *= 2;
        }
        player->backpack = true;
    }
    for (i = 0; i < NUMWEAPONS - 1; i++)
    {
        player->weaponowned[i] = true;
    }
    if (gamemode == shareware)
    {
        player->weaponowned[wp_skullrod] = false;
        player->weaponowned[wp_phoenixrod] = false;
        player->weaponowned[wp_mace] = false;
    }
    for (i = 0; i < NUMAMMO; i++)
    {
        player->ammo[i] = player->maxammo[i];
    }
    P_SetMessage(player, DEH_String(txt_cheatweapons), false);
}

static void CheatPowerFunc(player_t * player, Cheat_t * cheat)
{
    if (player->powers[pw_weaponlevel2])
    {
        player->powers[pw_weaponlevel2] = 0;
        P_SetMessage(player, DEH_String(txt_cheatpoweroff), false);
    }
    else
    {
        P_UseArtifact(player, arti_tomeofpower);
        P_SetMessage(player, DEH_String(txt_cheatpoweron), false);
    }
}

static void CheatHealthFunc(player_t * player, Cheat_t * cheat)
{
    if (player->chickenTics)
    {
        player->health = player->mo->health = MAXCHICKENHEALTH;
    }
    else
    {
        player->health = player->mo->health = MAXHEALTH;
    }
    P_SetMessage(player, DEH_String(txt_cheathealth), false);
}

static void CheatKeysFunc(player_t * player, Cheat_t * cheat)
{
    player->keys[key_yellow] = true;
    player->keys[key_green] = true;
    player->keys[key_blue] = true;
    playerkeys = 7;             // Key refresh flags
    P_SetMessage(player, DEH_String(txt_cheatkeys), false);
}

static void CheatSoundFunc(player_t * player, Cheat_t * cheat)
{
    DebugSound = !DebugSound;
    if (DebugSound)
    {
        P_SetMessage(player, DEH_String(txt_cheatsoundon), false);
    }
    else
    {
        P_SetMessage(player, DEH_String(txt_cheatsoundoff), false);
    }
}

static void CheatTickerFunc(player_t * player, Cheat_t * cheat)
{
    DisplayTicker = !DisplayTicker;
    if (DisplayTicker)
    {
        P_SetMessage(player, DEH_String(txt_cheattickeron), false);
    }
    else
    {
        P_SetMessage(player, DEH_String(txt_cheattickeroff), false);
    }

    I_DisplayFPSDots(DisplayTicker);
}

static void CheatArtifact1Func(player_t * player, Cheat_t * cheat)
{
    P_SetMessage(player, DEH_String(txt_cheatartifacts1), false);
}

static void CheatArtifact2Func(player_t * player, Cheat_t * cheat)
{
    P_SetMessage(player, DEH_String(txt_cheatartifacts2), false);
}

static void CheatArtifact3Func(player_t * player, Cheat_t * cheat)
{
    char args[2];
    int i;
    int j;
    int type;
    int count;

    cht_GetParam(cheat->seq, args);
    type = args[0] - 'a' + 1;
    count = args[1] - '0';
    if (type == 26 && count == 0)
    {                           // All artifacts
        for (i = arti_none + 1; i < NUMARTIFACTS; i++)
        {
            if (gamemode == shareware 
             && (i == arti_superhealth || i == arti_teleport))
            {
                continue;
            }
            for (j = 0; j < 16; j++)
            {
                P_GiveArtifact(player, i, NULL);
            }
        }
        P_SetMessage(player, DEH_String(txt_cheatartifacts3), false);
    }
    else if (type > arti_none && type < NUMARTIFACTS
             && count > 0 && count < 10)
    {
        if (gamemode == shareware
         && (type == arti_superhealth || type == arti_teleport))
        {
            P_SetMessage(player, DEH_String(txt_cheatartifactsfail), false);
            return;
        }
        for (i = 0; i < count; i++)
        {
            P_GiveArtifact(player, type, NULL);
        }
        P_SetMessage(player, DEH_String(txt_cheatartifacts3), false);
    }
    else
    {                           // Bad input
        P_SetMessage(player, DEH_String(txt_cheatartifactsfail), false);
    }
}

static void CheatWarpFunc(player_t * player, Cheat_t * cheat)
{
    char args[2];
    int episode;
    int map;

    cht_GetParam(cheat->seq, args);

    episode = args[0] - '0';
    map = args[1] - '0';
    if (D_ValidEpisodeMap(heretic, gamemode, episode, map))
    {
        G_DeferedInitNew(gameskill, episode, map);
        P_SetMessage(player, DEH_String(txt_cheatwarp), false);
    }
}

static void CheatChickenFunc(player_t * player, Cheat_t * cheat)
{
    extern boolean P_UndoPlayerChicken(player_t * player);

    if (player->chickenTics)
    {
        if (P_UndoPlayerChicken(player))
        {
            P_SetMessage(player, DEH_String(txt_cheatchickenoff), false);
        }
    }
    else if (P_ChickenMorphPlayer(player))
    {
        P_SetMessage(player, DEH_String(txt_cheatchickenon), false);
    }
}

static void CheatMassacreFunc(player_t * player, Cheat_t * cheat)
{
    P_Massacre();
    P_SetMessage(player, DEH_String(txt_cheatmassacre), false);
}

static void CheatIDKFAFunc(player_t * player, Cheat_t * cheat)
{
    int i;
    if (player->chickenTics)
    {
        return;
    }
    for (i = 1; i < 8; i++)
    {
        player->weaponowned[i] = false;
    }
    player->pendingweapon = wp_staff;
    P_SetMessage(player, DEH_String(txt_cheatidkfa), true);
}

static void CheatIDDQDFunc(player_t * player, Cheat_t * cheat)
{
    P_DamageMobj(player->mo, NULL, player->mo, 10000);
    P_SetMessage(player, DEH_String(txt_cheatiddqd), true);
}

static void CheatVERSIONFunc(player_t * player, Cheat_t * cheat)
{
    static char msg[38];

    if (english_language)
    {
        M_snprintf(msg, sizeof(msg), "%s%s - %s",
        TXT_VERSION,
        TXT_ARCH,
        TXT_DATE);
    }
    else
    {
        M_snprintf(msg, sizeof(msg), "%s%s - %s",
        TXT_VERSION_RUS,
        TXT_ARCH_RUS,
        TXT_DATE);
    }
    
    P_SetMessage(player, msg, true);
}
