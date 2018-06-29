/** FunnyMu: portable Funvision emulator *********************/
/**                                                         **/
/**                         funny.h                        **/
/**                                                         **/
/** This file contains declarations relevant to the drivers **/
/** and funny emulation itself.                             **/
/**                                                         **/
/** Copyright (C) Paul Hayter 2001                          **/
/**     based on ColEm by Marat Fayzullin                   **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#include "SDL.h"
#include <gccore.h>
#include <ogcsys.h>
#include "M6502.h"            /* 6502 CPU emulation             */

#define NORAM     0xFF      /* Byte to be returned from      */
                            /* non-existing pages and ports  */
#define MAXSCREEN 3         /* Highest screen mode supported */

#define SCREEN_DELAY 20  /* Technically the number of milliseconds
                               per frame (is 20ms for a 50Hz machine)
                               This used to be in microseconds on earlier
                               versions of funnymu */

//Plugin
extern char *PluginDevice;
extern char *PluginPathGame;
extern bool GameLoop;


/******** Variables used to control emulator behavior ********/
extern byte Verbose;        /* Debug msgs ON/OFF             */
extern byte AutoA,AutoB;    /* 1: Autofire ON on buttons A,B */
extern byte LogSnd;         /* 1: Log soundtrack into a file */
extern byte Adam;           /* 1: Emulate Coleco Adam        */
extern int  VPeriod;        /* CPU cycles/VBlank             */
extern int  HPeriod;        /* CPU cycles/HBlank             */
extern byte UPeriod;   /* Number of interrupts/screen update */
/*************************************************************/

/***** Following are macros to be used in screen drivers *****/
#define BigSprites    (VDP[1]&0x01)   /* Zoomed sprites      */
#define Sprites16x16  (VDP[1]&0x02)   /* 16x16/8x8 sprites   */
#define ScreenON      (VDP[1]&0x40)   /* Show screen         */
/*************************************************************/

extern M6502 CPU;                       /* CPU registers+state */
extern byte *VRAM,*RAM;               /* Main and Video RAMs */

extern char *SndName;                 /* Soundtrack log file */
extern char *PrnName;                 /* Printer redir. file */

extern byte *ChrGen,*ChrTab,*ColTab;  /* VDP tables [screens]*/
extern byte *SprGen,*SprTab;          /* VDP tables [sprites]*/
extern byte FGColor,BGColor;          /* Colors              */
extern byte ScrMode;                  /* Current screen mode */
extern byte VDP[8],VDPStatus;         /* VDP registers       */

extern byte KEYTBL[16];
extern word JoyState[2];              /* Joystick states     */
extern byte ExitNow;                  /* 1: Exit emulator    */

typedef unsigned short uint16;
typedef signed short sint16;
typedef unsigned char uint8;
#define SOUND_MAXRATE 44100
#define SOUND_SAMPLERATE 22050


/** StartFunny() ********************************************/
/** Allocate memory, load ROM image, initialize hardware,   **/
/** CPU and start the emulation. This function returns 0 in **/
/** the case of failure.                                    **/
/*************************************************************/
//int StartFunny(char *Cartridge);
int StartFunny(char *Cartridge, char *Device);

/** TrashFunny() ********************************************/
/** Free memory allocated by StartFunny().                 **/
/*************************************************************/
void TrashFunny(void);

/** InitMachine() ********************************************/
/** Allocate resources needed by the machine-dependent code.**/
/************************************ TO BE WRITTEN BY USER **/
int InitMachine(void);

/** TrashMachine() *******************************************/
/** Deallocate all resources taken by InitMachine().        **/
/************************************ TO BE WRITTEN BY USER **/
void TrashMachine(void);

/** RefreshLine#() *******************************************/
/** Refresh line Y (0..191), on an appropriate SCREEN#,     **/
/** including sprites in this line.                         **/
/************************************ TO BE WRITTEN BY USER **/
void RefreshLine0(byte Y);
void RefreshLine1(byte Y);
void RefreshLine2(byte Y);
void RefreshLine3(byte Y);

/** RefreshScreen() ******************************************/
/** Refresh screen. This function is called in the end of   **/
/** refresh cycle to show the entire screen.                **/
/************************************ TO BE WRITTEN BY USER **/
void RefreshScreen(void);

/** SetColor() ***********************************************/
/** Set color N (0..15) to (R,G,B).                         **/
/************************************ TO BE WRITTEN BY USER **/
void SetColor(byte N,byte R,byte G,byte B);

/** Joysticks() **********************************************/
/** This function is called to poll joysticks. It should    **/
/** set JoyState[0]/JoyState[1] in a following way:         **/
/**                                                         **/
/**      x.FIRE-B.x.x.L.D.R.U.x.FIRE-A.x.x.N3.N2.N1.N0      **/
/**                                                         **/
/** Least significant bit on the right. Active value is 0.  **/
/************************************ TO BE WRITTEN BY USER **/
void Joysticks(void);

/** Sound() **************************************************/
/** Set sound volume (0..255) and frequency (Hz) for a      **/
/** given channel (0..3). This function is only needed with **/
/** #define SOUND. The 3rd channel is noise.                **/
/************************************ TO BE WRITTEN BY USER **/
#ifdef SOUND
void Sound(int Channel,int Freq,int Volume);
#endif

