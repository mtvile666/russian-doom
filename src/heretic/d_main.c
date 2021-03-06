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
// D_main.c



#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "doomfeatures.h"

#include "txt_main.h"
#include "txt_io.h"

#include "net_client.h"

#include "config.h"
#include "ct_chat.h"
#include "doomdef.h"
#include "deh_main.h"
#include "d_iwad.h"
#include "i_endoom.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "m_misc.h"
#include "p_local.h"
#include "r_local.h"
#include "s_sound.h"
#include "w_main.h"
#include "v_video.h"
#include "w_merge.h"
#include "jn.h"


#define CT_KEY_GREEN    'g'
#define CT_KEY_YELLOW   'y'
#define CT_KEY_RED      'r'
#define CT_KEY_BLUE     'b'

#define STARTUP_WINDOW_X 17
#define STARTUP_WINDOW_Y 7

GameMode_t gamemode = indetermined;
char *gamedescription = "unknown";

boolean nomonsters;             // checkparm of -nomonsters
boolean respawnparm;            // checkparm of -respawn
boolean debugmode;              // checkparm of -debug
boolean ravpic;                 // checkparm of -ravpic
boolean cdrom;                  // true if cd-rom mode active
boolean noartiskip;             // whether shift-enter skips an artifact

skill_t startskill;
int startepisode;
int startmap;
int UpdateState;
static int graphical_startup = 0; // [JN] Disabled by default
static boolean using_graphical_startup;
static boolean main_loop_started = false;
boolean autostart;
extern boolean automapactive;

boolean advancedemo;

FILE *debugfile;

// [JN] Support for fallback to the English language.
// Windows OS only: do not set game language on first launch, 
// try to determine it automatically in D_DoomMain.
#ifdef _WIN32
int english_language = -1;
#else
int english_language = 0;
#endif

static int show_endoom = 0;
int level_brightness = 0; // [JN] Level brightness level
int local_time = 0; // [JN] Local time widget

// [JN] Automap specific variables.
int automap_follow = 1;
int automap_overlay = 0;
int automap_rotate = 0;
int automap_grid = 0;

void D_ConnectNetGame(void);
void D_CheckNetGame(void);
void D_PageDrawer(void);
void D_AdvanceDemo(void);
boolean F_Responder(event_t * ev);

int draw_shadowed_text;     // [JN] Элементы меню и тексты отбрасывают тень

//---------------------------------------------------------------------------
//
// PROC D_ProcessEvents
//
// Send all the events of the given timestamp down the responder chain.
//
//---------------------------------------------------------------------------

void D_ProcessEvents(void)
{
    event_t *ev;

    while ((ev = D_PopEvent()) != NULL)
    {
        if (F_Responder(ev))
        {
            continue;
        }
        if (MN_Responder(ev))
        {
            continue;
        }
        G_Responder(ev);
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawMessage
//
//---------------------------------------------------------------------------

void DrawMessage(void)
{
    player_t *player;

    player = &players[consoleplayer];
    if (player->messageTics <= 0 || !player->message)
    {                           // No message
        return;
    }

    if (english_language)
    {
        MN_DrTextA(player->message, 
                   160 - MN_TextAWidth(player->message) /
                   2 + wide_delta, 1);
    }
    else
    {
        MN_DrTextSmallRUS(player->message, 
                          160 - MN_DrTextSmallRUSWidth(player->message) /
                          2 + wide_delta, 1);
    }
}

//---------------------------------------------------------------------------
//
// PROC D_Display
//
// Draw current display, possibly wiping it from the previous.
//
//---------------------------------------------------------------------------

extern boolean finalestage;

void D_Display(void)
{
    extern boolean askforquit;

    // [JN] Set correct palette. Allow finale stages use own palettes.
    if (gamestate != GS_LEVEL && gamestate != GS_FINALE)
    {
        I_SetPalette(W_CacheLumpName(DEH_String(usegamma <= 8 ?
                                                "PALFIX" :
                                                "PLAYPAL"),
                                                PU_CACHE));
    }

    // Change the view size if needed
    if (setsizeneeded)
    {
        R_ExecuteSetViewSize();
    }

//
// do buffered drawing
//
    switch (gamestate)
    {
        case GS_LEVEL:
            if (!gametic)
                break;
            if (automapactive)
            {
                // [crispy] update automap while playing
                R_RenderPlayerView(&players[displayplayer]);
                AM_Drawer();
            }
            else
                R_RenderPlayerView(&players[displayplayer]);
            CT_Drawer();
            UpdateState |= I_FULLVIEW;
            SB_Drawer();
            break;
        case GS_INTERMISSION:
            IN_Drawer();
            break;
        case GS_FINALE:
            F_Drawer();
            break;
        case GS_DEMOSCREEN:
            D_PageDrawer();
            break;
    }

    if (testcontrols)
    {
        V_DrawMouseSpeedBox(testcontrols_mousespeed);
    }

    if (paused && !menuactive && !askforquit)
    {
        if (!netgame)
        {
            V_DrawShadowedPatchRaven(160 + wide_delta,
                                    (viewwindowy >> hires) + 5, W_CacheLumpName
                                    (DEH_String
                                    (english_language ? 
                                     "PAUSED" : "RD_PAUSE"), PU_CACHE));
        }
        else
        {
            V_DrawShadowedPatchRaven(160 + wide_delta,
                                     70, W_CacheLumpName
                                    (DEH_String(english_language ?
                                     "PAUSED" : "RD_PAUSE"), PU_CACHE));
        }
    }
    // Handle player messages
    DrawMessage();

    // Menu drawing
    MN_Drawer();

    // Send out any new accumulation
    NetUpdate();

    // Flush buffered stuff to screen
    I_FinishUpdate();
}

//
// D_GrabMouseCallback
//
// Called to determine whether to grab the mouse pointer
//

boolean D_GrabMouseCallback(void)
{
    // when menu is active or game is paused, release the mouse

    if (menuactive || paused)
        return false;

    // only grab mouse when playing levels (but not demos)

    return (gamestate == GS_LEVEL) && !demoplayback && !advancedemo;
}

//---------------------------------------------------------------------------
//
// PROC D_DoomLoop
//
//---------------------------------------------------------------------------

void D_DoomLoop(void)
{
    if (M_CheckParm("-debugfile"))
    {
        char filename[20];
        M_snprintf(filename, sizeof(filename), "debug%i.txt", consoleplayer);
        debugfile = fopen(filename, "w");
    }
    I_GraphicsCheckCommandLine();
    I_SetGrabMouseCallback(D_GrabMouseCallback);
    I_InitGraphics();

    main_loop_started = true;

    while (1)
    {
        // Process one or more tics
        // Will run at least one tic
        TryRunTics();

        // [JN] Mute and restore sound and music volume.
        if (mute_inactive_window && volume_needs_update)
        {
            if (!window_focused)
            {
                S_MuteSound();
            }
            else
            {
                S_UnMuteSound();
            }
        }

        // Move positional sounds
        S_UpdateSounds(players[consoleplayer].mo);

        // Update display, next frame, with current state.
        if (screenvisible)
        D_Display();
    }
}

/*
===============================================================================

						DEMO LOOP

===============================================================================
*/

int demosequence;
int pagetic;
char *pagename;


/*
================
=
= D_PageTicker
=
= Handles timing for warped projection
=
================
*/

void D_PageTicker(void)
{
    if (--pagetic < 0)
        D_AdvanceDemo();
}


/*
================
=
= D_PageDrawer
=
================
*/

void D_PageDrawer(void)
{
    if (widescreen)
    {
        // [JN] Clean up remainings of the wide screen before
        // drawing any new RAW screen.
        V_DrawFilledBox(0, 0, WIDESCREENWIDTH, SCREENHEIGHT, 0);
    }

    V_DrawRawScreen(W_CacheLumpName(pagename, PU_CACHE));
    if (demosequence == 1)
    {
        V_DrawShadowedPatchRaven(4, 160, W_CacheLumpName
                                (DEH_String
                                (english_language ? "ADVISOR" : "ADVIS_RU"),
                                                                 PU_CACHE));
    }
    UpdateState |= I_FULLSCRN;
}

/*
=================
=
= D_AdvanceDemo
=
= Called after each demo or intro demosequence finishes
=================
*/

void D_AdvanceDemo(void)
{
    advancedemo = true;
}

void D_DoAdvanceDemo(void)
{
    S_ResumeSound();    // [JN] Fix vanilla Heretic bug: resume music playing
    players[consoleplayer].playerstate = PST_LIVE;      // don't reborn
    advancedemo = false;
    usergame = false;           // can't save / end game here
    paused = false;
    gameaction = ga_nothing;
    demosequence = (demosequence + 1) % 7;
    switch (demosequence)
    {
        case 0:
            pagetic = 210;
            gamestate = GS_DEMOSCREEN;

            if (english_language)
            {
                pagename = DEH_String("TITLE");
            }
            else
            {
                pagename = DEH_String(gamemode == retail ? "TITLE_RT" : "TITLE");
            }
            S_StartSong(mus_titl, false);
            break;
        case 1:
            pagetic = 140;
            gamestate = GS_DEMOSCREEN;

            if (english_language)
            {
                pagename = DEH_String("TITLE");
            }
            else
            {
                pagename = DEH_String(gamemode == retail ? "TITLE_RT" : "TITLE");
            }

            break;
        case 2:
            BorderNeedRefresh = true;
            UpdateState |= I_FULLSCRN;
            if (!no_internal_demos)
            G_DeferedPlayDemo(DEH_String("demo1"));
            break;
        case 3:
            pagetic = 200;
            gamestate = GS_DEMOSCREEN;

            if (english_language)
            {
                pagename = DEH_String("CREDIT");
            }
            else
            {
                pagename = DEH_String(gamemode == retail ? "CRED_RT" : "CRED_RG");
            }

            break;
        case 4:
            BorderNeedRefresh = true;
            UpdateState |= I_FULLSCRN;
            if (!no_internal_demos)
            G_DeferedPlayDemo(DEH_String("demo2"));
            break;
        case 5:
            pagetic = 200;
            gamestate = GS_DEMOSCREEN;
            if (gamemode == shareware)
            {
                if (english_language)
                pagename = DEH_String("ORDER");
                else
                pagename = DEH_String("ORDER_R");
            }
            else
            {
                if (english_language)
                {
                    pagename = DEH_String("CREDIT");
                }
                else
                {
                    if (gamemode == retail)
                    {
                        pagename = DEH_String("CRED_RT");
                    }
                    else
                    {
                        pagename = DEH_String("CRED_RG");
                    }
                }
            }
            break;
        case 6:
            BorderNeedRefresh = true;
            UpdateState |= I_FULLSCRN;
            if (!no_internal_demos)
            G_DeferedPlayDemo(DEH_String("demo3"));
            break;
    }
}


/*
=================
=
= D_StartTitle
=
=================
*/

void D_StartTitle(void)
{
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo();
}


/*
==============
=
= D_CheckRecordFrom
=
= -recordfrom <savegame num> <demoname>
==============
*/

void D_CheckRecordFrom(void)
{
    int p;
    char *filename;

    //!
    // @vanilla
    // @category demo
    // @arg <savenum> <demofile>
    //
    // Record a demo, loading from the given filename. Equivalent
    // to -loadgame <savenum> -record <demofile>.

    p = M_CheckParmWithArgs("-recordfrom", 2);
    if (!p)
    {
        return;
    }

    filename = SV_Filename(myargv[p + 1][0] - '0');
    G_LoadGame(filename);
    G_DoLoadGame();             // load the gameskill etc info from savegame

    G_RecordDemo(gameskill, 1, gameepisode, gamemap, myargv[p + 2]);
    D_DoomLoop();               // never returns
    free(filename);
}

/*
===============
=
= D_AddFile
=
===============
*/

// MAPDIR should be defined as the directory that holds development maps
// for the -wart # # command

#define MAPDIR "\\data\\"

#define SHAREWAREWADNAME "heretic1.wad"

char *iwadfile;

char *basedefault = "heretic.ini";

void wadprintf(void)
{
    if (debugmode)
    {
        return;
    }
    // haleyjd FIXME: convert to textscreen code?
#ifdef __WATCOMC__
    _settextposition(23, 2);
    _setbkcolor(1);
    _settextcolor(0);
    _outtext(exrnwads);
    _settextposition(24, 2);
    _outtext(exrnwads2);
#endif
}

boolean D_AddFile(char *file)
{
    wad_file_t *handle;

    printf(english_language ?
           " adding: %s\n" :
           " добавление: %s\n",
           file);

    handle = W_AddFile(file);

    return handle != NULL;
}

//==========================================================
//
//  Startup Thermo code
//
//==========================================================
#define MSG_Y       9
#define THERM_X     14
#define THERM_Y     14

int thermMax;
int thermCurrent;
char smsg[80];                  // status bar line

//
//  Heretic startup screen shit
//

static int startup_line = STARTUP_WINDOW_Y;

void hprintf(char *string)
{
    if (using_graphical_startup)
    {
        TXT_BGColor(TXT_COLOR_CYAN, 0);
        TXT_FGColor(TXT_COLOR_BRIGHT_WHITE);

        TXT_GotoXY(STARTUP_WINDOW_X, startup_line);
        ++startup_line;
        TXT_Puts(string);

        TXT_UpdateScreen();
    }

    // haleyjd: shouldn't be WATCOMC-only
    if (debugmode)
        puts(string);
}

void drawstatus(void)
{
    int i;

    TXT_GotoXY(1, 24);
    TXT_BGColor(TXT_COLOR_BLUE, 0);
    TXT_FGColor(TXT_COLOR_BRIGHT_WHITE);

    for (i=0; smsg[i] != '\0'; ++i) 
    {
        TXT_PutChar(smsg[i]);
    }
}

void status(char *string)
{
    if (using_graphical_startup)
    {
        M_StringConcat(smsg, string, sizeof(smsg));
        drawstatus();
    }
}

void DrawThermo(void)
{
    static int last_progress = -1;
    int progress;
    int i;

    if (!using_graphical_startup)
    {
        return;
    }

#if 0
    progress = (98 * thermCurrent) / thermMax;
    screen = (char *) 0xb8000 + (THERM_Y * 160 + THERM_X * 2);
    for (i = 0; i < progress / 2; i++)
    {
        switch (i)
        {
            case 4:
            case 9:
            case 14:
            case 19:
            case 29:
            case 34:
            case 39:
            case 44:
                *screen++ = 0xb3;
                *screen++ = (THERMCOLOR << 4) + 15;
                break;
            case 24:
                *screen++ = 0xba;
                *screen++ = (THERMCOLOR << 4) + 15;
                break;
            default:
                *screen++ = 0xdb;
                *screen++ = 0x40 + THERMCOLOR;
                break;
        }
    }
    if (progress & 1)
    {
        *screen++ = 0xdd;
        *screen++ = 0x40 + THERMCOLOR;
    }
#else

    // No progress? Don't update the screen.

    progress = (50 * thermCurrent) / thermMax + 2;

    if (last_progress == progress)
    {
        return;
    }

    last_progress = progress;

    TXT_GotoXY(THERM_X, THERM_Y);

    TXT_FGColor(TXT_COLOR_BRIGHT_GREEN);
    TXT_BGColor(TXT_COLOR_GREEN, 0);

    for (i = 0; i < progress; i++)
    {
        TXT_PutChar(0xdb);
    }

    TXT_UpdateScreen();
#endif
}

void initStartup(void)
{
    byte *textScreen;
    byte *loading;

    if (!graphical_startup || debugmode || testcontrols)
    {
        using_graphical_startup = false;
        return;
    }

    if (!TXT_Init()) 
    {
        using_graphical_startup = false;
        return;
    }

    I_InitWindowTitle();
    I_InitWindowIcon();

    // Blit main screen
    textScreen = TXT_GetScreenData();
    loading = W_CacheLumpName(DEH_String("LOADING"), PU_CACHE);
    memcpy(textScreen, loading, 4000);

    // Print version string

    TXT_BGColor(TXT_COLOR_RED, 0);
    TXT_FGColor(TXT_COLOR_YELLOW);
    TXT_GotoXY(46, 2);
    TXT_Puts(HERETIC_VERSION_TEXT);

    TXT_UpdateScreen();

    using_graphical_startup = true;
}

static void finishStartup(void)
{
    if (using_graphical_startup)
    {
        TXT_Shutdown();
    }
}

char tmsg[300];
void tprintf(char *msg, int initflag)
{
    // haleyjd FIXME: convert to textscreen code?
#ifdef __WATCOMC__
    char temp[80];
    int start;
    int add;
    int i;

    if (initflag)
        tmsg[0] = 0;
    M_StringConcat(tmsg, msg, sizeof(tmsg));
    blitStartup();
    DrawThermo();
    _setbkcolor(4);
    _settextcolor(15);
    for (add = start = i = 0; i <= strlen(tmsg); i++)
        if ((tmsg[i] == '\n') || (!tmsg[i]))
        {
            memset(temp, 0, 80);
            M_StringCopy(temp, tmsg + start, sizeof(temp));
            if (i - start < sizeof(temp))
            {
                temp[i - start] = '\0';
            }
            _settextposition(MSG_Y + add, 40 - strlen(temp) / 2);
            _outtext(temp);
            start = i + 1;
            add++;
        }
    _settextposition(25, 1);
    drawstatus();
#else
    printf("%s", msg);
#endif
}

// haleyjd: moved up, removed WATCOMC code
void CleanExit(void)
{
    DEH_printf(english_language ?
    "Exited from HERETIC.\n" :
    "Выполнен выход из HERETIC.\n");
    exit(1);
}

void CheckAbortStartup(void)
{
    // haleyjd: removed WATCOMC
    // haleyjd FIXME: this should actually work in text mode too, but how to
    // get input before SDL video init?
    if(using_graphical_startup)
    {
        if(TXT_GetChar() == 27)
            CleanExit();
    }
}

void IncThermo(void)
{
    thermCurrent++;
    DrawThermo();
    CheckAbortStartup();
}

void InitThermo(int max)
{
    thermMax = max;
    thermCurrent = 0;
}

//
// Add configuration file variable bindings.
//

void D_BindVariables(void)
{
    extern int snd_Channels;
    int i;

    M_ApplyPlatformDefaults();

    I_BindInputVariables();
    I_BindVideoVariables();
    I_BindJoystickVariables();
    I_BindSoundVariables();

    M_BindBaseControls();
    M_BindHereticControls();
    M_BindWeaponControls();
    M_BindChatControls(MAXPLAYERS);

    key_multi_msgplayer[0] = CT_KEY_GREEN;
    key_multi_msgplayer[1] = CT_KEY_YELLOW;
    key_multi_msgplayer[2] = CT_KEY_RED;
    key_multi_msgplayer[3] = CT_KEY_BLUE;

    M_BindMenuControls();
    M_BindMapControls();

#ifdef FEATURE_MULTIPLAYER
    NET_BindVariables();
#endif

    // [JN] Support for fallback to the English language.
    M_BindIntVariable("english_language",       &english_language);

    // Rendering
    M_BindIntVariable("uncapped_fps",           &uncapped_fps);
    M_BindIntVariable("show_endoom",            &show_endoom);
    M_BindIntVariable("graphical_startup",      &graphical_startup);

    // Display
    M_BindIntVariable("screenblocks",           &screenblocks);
    M_BindIntVariable("level_brightness",       &level_brightness);
    M_BindIntVariable("local_time",             &local_time);

    // Automap
    M_BindIntVariable("automap_follow",         &automap_follow);
    M_BindIntVariable("automap_overlay",        &automap_overlay);
    M_BindIntVariable("automap_rotate",         &automap_rotate);
    M_BindIntVariable("automap_grid",           &automap_grid);

    // Sound
    M_BindIntVariable("sfx_volume",             &snd_MaxVolume);
    M_BindIntVariable("music_volume",           &snd_MusicVolume);
    M_BindIntVariable("snd_monomode",           &snd_monomode);
    M_BindIntVariable("snd_channels",           &snd_Channels);

    // Controls
    M_BindIntVariable("mlook",                  &mlook);
    M_BindIntVariable("mouse_sensitivity",      &mouseSensitivity);

    // Gameplay: Graphical
    M_BindIntVariable("brightmaps",             &brightmaps);
    M_BindIntVariable("fake_contrast",          &fake_contrast);
    M_BindIntVariable("colored_hud",            &colored_hud);
    M_BindIntVariable("colored_blood",          &colored_blood);
    M_BindIntVariable("invul_sky",              &invul_sky);
    M_BindIntVariable("draw_shadowed_text",     &draw_shadowed_text);

    // Gameplay: Tactical
    M_BindIntVariable("automap_stats",          &automap_stats);
    M_BindIntVariable("secret_notification",    &secret_notification);
    M_BindIntVariable("negative_health",        &negative_health);

    // Gameplay: Physical
    M_BindIntVariable("torque",                 &torque);
    M_BindIntVariable("weapon_bobbing",         &weapon_bobbing);
    M_BindIntVariable("randomly_flipcorpses",   &randomly_flipcorpses);

    // Gameplay: Crosshair
    M_BindIntVariable("crosshair_draw",         &crosshair_draw);
    M_BindIntVariable("crosshair_health",       &crosshair_health);
    M_BindIntVariable("crosshair_scale",        &crosshair_scale);    

    // Геймплей
    M_BindIntVariable("no_internal_demos",      &no_internal_demos);
    M_BindIntVariable("flip_levels",            &flip_levels);

    for (i=0; i<10; ++i)
    {
        char buf[12];

        M_snprintf(buf, sizeof(buf), "chatmacro%i", i);
        M_BindStringVariable(buf, &chat_macros[i]);
    }
}

// 
// Called at exit to display the ENDOOM screen (ENDTEXT in Heretic)
//

static void D_Endoom(void)
{
    byte *endoom_data;

    // Disable ENDOOM?

    if (!show_endoom || testcontrols || !main_loop_started)
    {
        return;
    }

    endoom_data = W_CacheLumpName(DEH_String("ENDTEXT"), PU_STATIC);

    I_Endoom(endoom_data);
}

//---------------------------------------------------------------------------
//
// PROC D_DoomMain
//
//---------------------------------------------------------------------------

void D_DoomMain(void)
{
    GameMission_t gamemission;
    int p;
    char file[256];
    char demolumpname[9];
    int newpwadfile;

#ifdef _WIN32
    // [JN] Get system preffed language...
    DWORD rd_lang_id = PRIMARYLANGID(LANGIDFROMLCID(GetSystemDefaultLCID()));
    // ..if game language is not set yet (-1), and OS preffered language
    // is appropriate for using Russian language in the game, use it.
    if (english_language == -1)
    {
        if (rd_lang_id != LANG_RUSSIAN
        &&  rd_lang_id != LANG_UKRAINIAN
        &&  rd_lang_id != LANG_BELARUSIAN)
        english_language = 1;
        else
        english_language = 0;
    }

    // [JN] Print colorized title
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_GREEN
                                                           | FOREGROUND_RED
                                                           | FOREGROUND_GREEN
                                                           | FOREGROUND_BLUE
                                                           | FOREGROUND_INTENSITY);
    DEH_printf("                              Russian Heretic " PACKAGE_VERSION
               "                              ");
    DEH_printf("\n");

    // [JN] Fallback to standard console colos
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED
                                                           | FOREGROUND_GREEN
                                                           | FOREGROUND_BLUE);
#else
    // [JN] Just print an uncolored banner
    I_PrintBanner(PACKAGE_STRING);    
#endif 

    I_AtExit(D_Endoom, false);

    //!
    // @vanilla
    //
    // Disable monsters.
    //

    nomonsters = M_ParmExists("-nomonsters");

    //!
    // @vanilla
    //
    // Monsters respawn after being killed.
    //

    respawnparm = M_ParmExists("-respawn");

    //!
    // @vanilla
    //
    // Take screenshots when F1 is pressed.
    //

    ravpic = M_ParmExists("-ravpic");

    //!
    // @vanilla
    //
    // Allow artifacts to be used when the run key is held down.
    //

    noartiskip = M_ParmExists("-noartiskip");

    debugmode = M_ParmExists("-debug");
    startskill = sk_medium;
    startepisode = 1;
    startmap = 1;
    autostart = false;

    //!
    // @vanilla
    //
    // [JN] Activate vanilla gameplay mode.
    // All optional enhancements will be disabled without 
    // modifying configuration file (russian-heretic.ini)
    //

    vanillaparm = M_ParmExists("-vanilla");

//
// get skill / episode / map from parms
//

    //!
    // @vanilla
    // @category net
    //
    // Start a deathmatch game.
    //

    if (M_ParmExists("-deathmatch"))
    {
        deathmatch = true;
    }

    //!
    // @arg <skill>
    // @vanilla
    //
    // Set the game skill, 1-5 (1: easiest, 5: hardest).  A skill of
    // 0 disables all monsters.
    //

    p = M_CheckParmWithArgs("-skill", 1);
    if (p)
    {
        startskill = myargv[p + 1][0] - '1';
        autostart = true;
    }

    //!
    // @arg <n>
    // @vanilla
    //
    // Start playing on episode n (1-4)
    //

    p = M_CheckParmWithArgs("-episode", 1);
    if (p)
    {
        startepisode = myargv[p + 1][0] - '0';
        startmap = 1;
        autostart = true;
    }

    //!
    // @arg <x> <y>
    // @vanilla
    //
    // Start a game immediately, warping to level ExMy.
    //

    p = M_CheckParmWithArgs("-warp", 2);
    if (p && p < myargc - 2)
    {
        startepisode = myargv[p + 1][0] - '0';
        startmap = myargv[p + 2][0] - '0';
        autostart = true;
    }

//
// init subsystems
//
    DEH_printf(english_language ?
               "V_Init: allocate screens.\n" :
               "V_Init: Инициализация видео.\n");
    V_Init();

    // Check for -CDROM

    cdrom = false;

#ifdef _WIN32

    //!
    // @platform windows
    // @vanilla
    //
    // Save configuration data and savegames in c:\heretic.cd,
    // allowing play from CD.
    //

    if (M_CheckParm("-cdrom"))
    {
        cdrom = true;
    }
#endif

    if (cdrom)
    {
        M_SetConfigDir(DEH_String("c:\\heretic.cd"));
    }
    else
    {
        M_SetConfigDir(NULL);
    }

    // Load defaults before initing other systems
    DEH_printf(english_language ?
               "M_LoadDefaults: Load system defaults.\n" :
               "M_LoadDefaults: Загрузка системных стандартов.\n");
    D_BindVariables();
    M_SetConfigFilenames(PROGRAM_PREFIX "heretic.ini");
    M_LoadDefaults();

    I_AtExit(M_SaveDefaults, false);

    DEH_printf(english_language ?
               "Z_Init: Init zone memory allocation daemon.\n" :
               "Z_Init: Инициализация распределения памяти.\n");
    Z_Init();

    DEH_printf(english_language ?
               "W_Init: Init WADfiles.\n" :
               "W_Init: Инициализация WAD-файлов.\n");

    iwadfile = D_FindIWAD(IWAD_MASK_HERETIC, &gamemission);

    if (iwadfile == NULL)
    {
        if (english_language)
        {
            I_Error("Game mode indeterminate. No IWAD was found. Try specifying\n"
                    "one with the '-iwad' command line parameter.");
        }
        else
        {
            I_Error("Невозможно определить игру из за отсутствующего IWAD-файла.\n"
                    "Попробуйте указать IWAD-файл командой '-iwad'.\n");
        }
    }

    D_AddFile(iwadfile);
    W_CheckCorrectIWAD(heretic);

#ifdef FEATURE_DEHACKED
    // Load dehacked patches specified on the command line.
    DEH_ParseCommandLine();
#endif

    // Load PWAD files.
    W_ParseCommandLine();

    //!
    // @arg <demo>
    // @category demo
    // @vanilla
    //
    // Play back the demo named demo.lmp.
    //

    p = M_CheckParmWithArgs("-playdemo", 1);
    if (!p)
    {
        //!
        // @arg <demo>
        // @category demo
        // @vanilla
        //
        // Play back the demo named demo.lmp, determining the framerate
        // of the screen.
        //

        p = M_CheckParmWithArgs("-timedemo", 1);
    }

    if (p)
    {
        char *uc_filename = strdup(myargv[p + 1]);
        M_ForceUppercase(uc_filename);

        // In Vanilla, the filename must be specified without .lmp,
        // but make that optional.
        if (M_StringEndsWith(uc_filename, ".LMP"))
        {
            M_StringCopy(file, myargv[p + 1], sizeof(file));
        }
        else
        {
            DEH_snprintf(file, sizeof(file), "%s.lmp", myargv[p + 1]);
        }

        free(uc_filename);

        if (D_AddFile(file))
        {
            M_StringCopy(demolumpname, lumpinfo[numlumps - 1]->name,
                         sizeof(demolumpname));
        }
        else
        {
            // The file failed to load, but copy the original arg as a
            // demo name to make tricks like -playdemo demo1 possible.
            M_StringCopy(demolumpname, myargv[p + 1], sizeof(demolumpname));
        }

        printf(english_language ?
               "Playing demo %s.\n" :
               "Проигрывание демозаписи %s.\n",
               file);
    }

    // [JN] Addition: also generate the WAD hash table.  Speed things up a bit.
    W_GenerateHashTable();

    //!
    // @category demo
    //
    // Record or playback a demo without automatically quitting
    // after either level exit or player respawn.
    //

    demoextend = M_ParmExists("-demoextend");

    W_MergeFile("base/heretic-common.wad");

    if (W_CheckNumForName(DEH_String("E2M1")) == -1)
    {
        gamemode = shareware;
        gamedescription = english_language ?
                          "Heretic (shareware)" :
                          "Heretic (демоверсия)";
    }
    else if (W_CheckNumForName("EXTENDED") != -1)
    {
        // Presence of the EXTENDED lump indicates the retail version
        gamemode = retail;
        gamedescription = english_language ? 
                          "Heretic: Shadow of the Serpent Riders" :
                          "Heretic: Тень Змеиных Всадников";
    }
    else
    {
        gamemode = registered;
        gamedescription = "Heretic";
    }

    // [JN] Параметр "-file" перенесен из w_main.c
    // Необходимо для того, чтобы любые ресурсы из pwad-файлов
    // загружались после руссифицированных pwad-файлов.

    newpwadfile = M_CheckParmWithArgs ("-file", 1);
    if (newpwadfile)
    {
        while (++newpwadfile != myargc && myargv[newpwadfile][0] != '-')
        {
        char *filename;
        filename = D_TryFindWADByName(myargv[newpwadfile]);
        printf(english_language ?
               " adding: %s\n" :
               " добавление: %s\n",
               filename);
        W_MergeFile(filename);
        }
    }

    I_SetWindowTitle(gamedescription);

    savegamedir = M_GetSaveGameDir("heretic.wad");

    if (M_ParmExists("-testcontrols"))
    {
        startepisode = 1;
        startmap = 1;
        autostart = true;
        testcontrols = true;
    }

    I_InitTimer();
    I_InitSound(false);
    I_InitMusic();

#ifdef FEATURE_MULTIPLAYER
    tprintf(english_language ?
            "NET_Init: Init network subsystem.\n" :
            "NET_Init: Инициализация сетевой подсистемы.\n", 1);
    NET_Init ();
#endif

    D_ConnectNetGame();

    // haleyjd: removed WATCOMC
    initStartup();

    //
    //  Build status bar line!
    //
    smsg[0] = 0;
    if (deathmatch)
        status(DEH_String(english_language ?
                          "DeathMatch..." :
                          "Дефматч..."));
    if (nomonsters)
        status(DEH_String(english_language ?
                          "No Monsters..." :
                          "Без монстров..."));
    if (respawnparm)
        status(DEH_String(english_language ? 
                          "Respawning..." :
                          "Монстры воскрешаются..."));
    if (autostart)
    {
        char temp[64];
        DEH_snprintf(temp, sizeof(temp), english_language ?
                     "Warp to Episode %d, Map %d, Skill %d " :
                     "Перемещение в эпизод %d, уровень %d, сложность %d ",
                     startepisode, startmap, startskill + 1);
        status(temp);
    }
    wadprintf();                // print the added wadfiles

    tprintf(DEH_String(english_language ?
                       "MN_Init: Init menu system.\n" :
                       "MN_Init: Инициализация игрового меню.\n"), 1);
    MN_Init();

    CT_Init();

    tprintf(DEH_String(english_language ?
                       "R_Init: Init Heretic refresh daemon." :
                       "R_Init: Инициализация процесса запуска Heretic."), 1);
    hprintf(DEH_String("Loading graphics"));
    R_Init();
    tprintf("\n", 0);

    tprintf(DEH_String(english_language ?
                       "P_Init: Init Playloop state.\n" :
                       "P_Init: Инициализация игрового окружения.\n"), 1);
    hprintf(DEH_String("Init game engine."));
    P_Init();
    IncThermo();

    tprintf(DEH_String(english_language ? 
                       "I_Init: Setting up machine state.\n" :
                       "I_Init: Инициализация состояния компьютера.\n"), 1);
    I_CheckIsScreensaver();
    I_InitJoystick();
    IncThermo();

    tprintf(DEH_String(english_language ?
                       "S_Init: Setting up sound.\n" :
                       "S_Init: Активация звуковой системы.\n"), 1);
    S_Init();
    //IO_StartupTimer();
    S_Start();

    tprintf(DEH_String(english_language ?
                       "D_CheckNetGame: Checking network game status.\n" :
                       "D_CheckNetGame: Проверка статуса сетевой игры.\n"), 1);
    hprintf(DEH_String("Checking network game status."));
    D_CheckNetGame();
    IncThermo();

    // haleyjd: removed WATCOMC

    tprintf(DEH_String(english_language ?
                       "SB_Init: Loading patches.\n" :
                       "SB_Init: Загрузка патчей.\n"), 1);
    SB_Init();
    IncThermo();

//
// start the apropriate game based on parms
//

    D_CheckRecordFrom();

    //!
    // @arg <x>
    // @category demo
    // @vanilla
    //
    // Record a demo named x.lmp.
    //

    p = M_CheckParmWithArgs("-record", 1);
    if (p)
    {
        G_RecordDemo(startskill, 1, startepisode, startmap, myargv[p + 1]);
        D_DoomLoop();           // Never returns
    }

    p = M_CheckParmWithArgs("-playdemo", 1);
    if (p)
    {
        singledemo = true;      // Quit after one demo
        G_DeferedPlayDemo(demolumpname);
        D_DoomLoop();           // Never returns
    }

    p = M_CheckParmWithArgs("-timedemo", 1);
    if (p)
    {
        G_TimeDemo(demolumpname);
        D_DoomLoop();           // Never returns
    }

    //!
    // @arg <s>
    // @vanilla
    //
    // Load the game in savegame slot s.
    //

    p = M_CheckParmWithArgs("-loadgame", 1);
    if (p && p < myargc - 1)
    {
        char *filename;

	filename = SV_Filename(myargv[p + 1][0] - '0');
        G_LoadGame(filename);
	free(filename);
    }

    // Check valid episode and map
    if (autostart || netgame)
    {
        if (!D_ValidEpisodeMap(heretic, gamemode, startepisode, startmap))
        {
            startepisode = 1;
            startmap = 1;
        }
    }

    if (gameaction != ga_loadgame)
    {
        UpdateState |= I_FULLSCRN;
        BorderNeedRefresh = true;
        if (autostart || netgame)
        {
            G_InitNew(startskill, startepisode, startmap);
        }
        else
        {
            D_StartTitle();
        }
    }

    finishStartup();

    // [JN] Show the game we are playing
    DEH_printf(english_language ? "Starting game: " : "Запуск игры: ");
    DEH_printf("\"");
    DEH_printf(gamedescription);
    DEH_printf("\".");
    DEH_printf("\n");

    // [JN] Define and load translated strings
    RD_DefineLanguageStrings();

    D_DoomLoop();               // Never returns
}
