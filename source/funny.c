/** FunnyMu: portable creatiVision emulator ******************/
/**                                                         **/
/**                        funny.c                          **/
/**                                                         **/
/** This file contains implementation for the Funvision     **/
/** specific hardware: VDP, etc. Initialization code and    **/
/** definitions needed for the machine-dependent drivers    **/
/** are also here.                                          **/
/**                                                         **/
/** Unofficial version by Luca "MADrigal" Antignano, 2007   **/
/**                                                         **/
/** Copyright (C) Paul Hayter 2001                          **/
/**     based on ColEm by Marat Fayzullin                   **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
//#include "INIFile.h"

#include "funny.h"

#ifdef USESDL
  #include "funnysdl.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef ZLIB
  #include <zlib.h>
#endif

byte Verbose     = 0;       /* Debug msgs ON/OFF             */
byte UPeriod     = 2;       /* Interrupts/scr. update        */
int  VPeriod     = 40000;   /* Number of cycles per VBlank   */
int  HPeriod     = 128;     /* Number of cycles per HBlank   */
byte AutoA       = 0,
     AutoB       = 0;       /* 1: Autofire for A,B buttons   */
byte LogSnd      = 0;       /* 1: Log soundtrack into a file */
byte PIAAIO      = 0;
byte PIAADDR     = 0;
byte PIAACTL     = 0;
byte PIABIO      = 0;
byte PIABDDR     = 0;
byte PIABCTL     = 0;

byte KEYTBL[16] =  { 255, 255, 255, 255, 255, 255, 255, 255,   /* KEYTBL[7]  is piaa3=lo */
                     255, 255, 255, 255, 255, 255, 255, 255 }; /* KEYTBL[11] is piaa2=lo */
                                                               /* KEYTBL[13] is piaa1=lo */
                                                               /* KEYTBL[14] is piaa0=lo */

byte *VRAM, *RAM;              /* Main and Video RAMs           */
byte *CART;                    /* ROM cartridge file            */
M6502 CPU;                     /* 6502 CPU registers and state  */

byte ExitNow;                  /* 1: Exit the emulator          */

byte JoyMode;                  /* Joystick controller mode      */
word JoyState[2];              /* Joystick states               */

char *PrnName = NULL;          /* Printer redirection file      */
FILE *PrnStream;

byte *ChrGen, *ChrTab, *ColTab;/* VDP tables (screens)          */
byte *SprGen, *SprTab;         /* VDP tables (sprites)          */
pair WVAddr, RVAddr;           /* Storage for VRAM addresses    */
byte VKey;                     /* VDP address latch key         */
byte FGColor, BGColor;         /* Colors                        */
byte ScrMode;                  /* Current screen mode           */
byte CurLine;                  /* Current scanline              */
byte VDP[8], VDPStatus;        /* VDP registers                 */

char *SndName   = "LOG.SND";   /* Soundtrack log file           */
FILE *SndStream = NULL;

long CartSize   = 0;
Uint32 OldTimer = 0;
Uint32 NewTimer = 0;

unsigned int vdp_totlines = 312;

/*** TMS9918/9928 Palette *******************************************/
struct { byte R,G,B; } Palette[16] =
{
  {0x00,0x00,0x00},{0x00,0x00,0x00},{0x20,0xC0,0x20},{0x60,0xE0,0x60},
  {0x20,0x20,0xE0},{0x40,0x60,0xE0},{0xA0,0x20,0x20},{0x40,0xC0,0xE0},
  {0xE0,0x20,0x20},{0xE0,0x60,0x60},{0xC0,0xC0,0x20},{0xC0,0xC0,0x80},
  {0x20,0x80,0x20},{0xC0,0x40,0xA0},{0xA0,0xA0,0xA0},{0xE0,0xE0,0xE0}
};


struct {
  void (*Refresh)(byte Y); byte R2, R3, R4, R5;
}
SCR[MAXSCREEN+1] = {
  { RefreshLine0, 0x7F, 0x00, 0x3F, 0x00 }, /* SCREEN 0:TEXT 40x24    */
  { RefreshLine1, 0x0F, 0xFF, 0x07, 0x7F }, /* SCREEN 1:TEXT 32x24    */
  { RefreshLine2, 0x7F, 0x80, 0x3C, 0xFF }, /* SCREEN 2:BLOCK 256x192 */
  { RefreshLine3, 0x7F, 0x00, 0x3F, 0xFF }  /* SCREEN 3:GFX 64x48x16  */
};

static void VDPOut(byte Reg, byte Value);   /* Write value into VDP   */
static void CheckSprites(void);             /* Collisions/5th spr.    */
/* Log and play sound     */
//static void Play(int C, int F, int V);     

int SN76496Init(int chip, int clock, int gain, int sample_rate);
void SN76496Update(int chip, uint16 *buffer, int length);
void SN76496Write(int chip, int data);


/** StartFunny() *********************************************/
/** Allocate memory, load ROM image, initialize hardware,   **/
/** CPU and start the emulation. This function returns 0 in **/
/** the case of failure.                                    **/
/*************************************************************/
int StartFunny(char *Cartridge, char *Device)
{
  FILE *F;
  int *T, J;
  char *P;

  /*** VDP control register states: ***/
  static byte VDPInit[8] = {
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  /*** STARTUP CODE starts here: ***/
  T=(int *)"\01\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

#ifdef LSB_FIRST
  if(*T!=1) {
    /*printf("*********** This machine is high-endian. ***********\n");
    printf("Take #define LSB_FIRST out and compile FunnyMu again\n");*/
    return(0);
  }
#else
  if(*T==1) {
    /*printf("*********** This machine is low-endian. ************\n");
    printf("Insert #define LSB_FIRST and compile FunnyMu again\n");*/
    return(0);
  }
#endif

   /* Calculate IPeriod from VPeriod */
  if(UPeriod<1) UPeriod=1;
  if(VPeriod/HPeriod<256) VPeriod=256*HPeriod;
  CPU.IPeriod=HPeriod;
  CPU.TrapBadOps=Verbose&0x04;
  /* CPU.IAutoReset=0; */

  /* Zero everything */
  VRAM=RAM=NULL;SndStream=NULL;ExitNow=0;

  if(Verbose) printf("Allocating 64kB for CPU address space...");  
  if(!(RAM=malloc(0x10000))) { if(Verbose) puts("FAILED");return(0); }
  memset(RAM,NORAM,0x10000);
  if(Verbose) printf("OK\nAllocating 16kB for VDP address space...");
  if(!(VRAM=malloc(0x4000))) { if(Verbose) puts("FAILED");return(0); }
  memset(VRAM,NORAM,0x4000);

  if(Verbose) printf("OK\nLoading ROMs:\n  Opening FUNBOOT.ROM...");
  P=NULL;

  char BiosPath[1024];

  if(Device == NULL)
  {
    sprintf(BiosPath, "/apps/funny/bios/FUNBOOT.ROM" );
  }
  else
  {
    sprintf(BiosPath, "%s/roms/funny/FUNBOOT.ROM", Device);
  }

  if( (F = fopen(BiosPath,"r")) != NULL)
  {
    if(fread(RAM+0xF800,1,0x0800,F)!=0x0800) P="SHORT FILE";
    fclose(F);
  }
  else P="NOT FOUND";
  if(P) { if(Verbose) puts(P);return(0); }

  if(Verbose) printf("OK\n  Opening %s...",Cartridge);
  P=NULL;

#ifdef ZLIB
  #define fopen          gzopen
  #define fclose         gzclose
  #define fread(B,N,L,F) gzread(F,B,(L)*(N))
#endif

  if(F=(FILE *)fopen(Cartridge,"rb"))
  {
    /* Read a max of 16K at 0x8000 */
    CartSize=fread(RAM+0x8000,1,0x4000,(gzFile)F);
    if (CartSize==0x1000) {
       //printf("   4K cartridge\n");
       memcpy(RAM+0x9000,RAM+0x8000,0x1000);
       memcpy(RAM+0xA000,RAM+0x8000,0x2000);
    } 
    if (CartSize==0x2000) {
       //printf("   8K cartridge\n");
       memcpy(RAM+0xA000,RAM+0x8000,0x2000);
    } 
    if (CartSize==0x4000) {
       //printf("   16K (or more) cartridge\n");
       CartSize=fread(RAM+0x4000,1,0x4000,(gzFile)F);
       if (CartSize==0x0800) {
          //printf("   extra 2K\n");
          memcpy(RAM+0x4800,RAM+0x4000,0x0800);
          memcpy(RAM+0x5000,RAM+0x4000,0x1000);
          memcpy(RAM+0x6000,RAM+0x4000,0x2000);
       }
       if (CartSize==0x1000) {
          //printf("   extra 4K\n");
          memcpy(RAM+0x5000,RAM+0x4000,0x1000);
          memcpy(RAM+0x6000,RAM+0x4000,0x2000);
       }
       if (CartSize==0x2000) {
          //printf("   extra 8K\n");
          memcpy(RAM+0x6000,RAM+0x4000,0x2000);
       }
    } 
    fclose((gzFile)F);
  }
  else P="NOT FOUND";

  
  
#ifdef ZLIB
  #undef fopen
  #undef fclose
  #undef fread
#endif

  if(P) {
    if(Verbose)
      puts(P);
    return(0);
  }

  if(!PrnName)
    PrnStream=stdout;
  else {
    if(Verbose)
      printf("Redirecting printer output to %s...",PrnName);

    if(!(PrnStream=fopen(PrnName, "wb")))
      PrnStream=stdout;

    if(Verbose)
      puts(PrnStream==stdout? "FAILED": "OK");
  }

  if(Verbose) {
    printf("Initializing CPU and System Hardware:\n");
    printf("  VBlank = %d cycles\n  HBlank = %d cycles\n", VPeriod, HPeriod);
  }

#ifndef USESDL
  /* Set up the palette */
  for(J=0; J<16; J++)
    SetColor(J, Palette[J].R, Palette[J].G, Palette[J].B);
#endif

  /* Initialize VDP registers */
  memcpy(VDP, VDPInit, sizeof(VDP));

  /* Initialize internal variables */
  VKey=1;                              /* VDP address latch key */
  VDPStatus=0x9F;                      /* VDP status register   */
  FGColor=BGColor=0;                   /* Fore/Background color */
  ScrMode=0;                           /* Current screenmode    */
  CurLine=0;                           /* Current scanline      */
  ChrTab=ColTab=ChrGen=VRAM;           /* VDP tables (screen)   */
  SprTab=SprGen=VRAM;                  /* VDP tables (sprites)  */
  JoyMode=0;                           /* Joystick mode key     */
  JoyState[0]=JoyState[1]=0xFFFF;      /* Joystick states       */
#ifdef SOUND
  Reset76489(&PSG, Play);              /* Reset SN76489 PSG     */
  Sync76489(&PSG, PSG_SYNC);           /* Make it synchronous   */
#endif
  Reset6502(&CPU);                     /* Reset 6502 registers  */
  OldTimer=sdl_getticks();

  if(Verbose)
    printf("Running ROM code...\n");

  J=Run6502(&CPU);

  if(Verbose)
    printf("Exited at PC = %04Xh.\n",J);

  return(1);
}





/** TrashFunny() *********************************************/
/** Free memory allocated                                   **/
/*************************************************************/
void TrashFunny(void) {
  if(RAM)  free(RAM);
  if(VRAM) free(VRAM);
  if(CART) free(CART);
  if(SndStream) fclose(SndStream);
}





/** Wr6502() **************************************************/
/** 6502 emulation calls this function to write byte V to    **/
/** address A of 6502 address space.                         **/
/*************************************************************/
void Wr6502(register word A,register byte V)
{
  static byte VR;

  switch(A&0xf000)
  {
     case 0x3000:
       if(A&0x01)
         if(VKey) { VR=V;VKey--; }
         else
         {
            VKey++;
            switch(V&0xC0)
            {
                case 0x80: VDPOut(V&0x07,VR);break;
                case 0x40: WVAddr.B.l=VR;WVAddr.B.h=V&0x3F;break;
                case 0x00: RVAddr.B.l=VR;RVAddr.B.h=V;
            }
         }
      else
         if(VKey)
         { VRAM[WVAddr.W]=V;
           WVAddr.W=(WVAddr.W+1)&0x3FFF;
            }
      break;
    case 0x1000:
      switch(A&0x03)
      {
         case 0:
            if ((PIAACTL&0x04) == 0) {
               PIAADDR = V; return;
            } else {
               PIAAIO = V;
               return;
            }
         case 1:
            PIAACTL=V; return;
         case 2:
            if ((PIABCTL&0x04) == 0) {
               PIABDDR = V; return;
            } else {
		SN76496Write(0,V);return;
            }
         case 3:
            PIABCTL=V; return;

      }

   }

  if((A<0x1000))
  {

    A&=0x03FF;
    RAM[A]=RAM[0x0400+A]=RAM[0x0800+A]=RAM[0x0c00+A]=V;
    
  }
}

/** Rd6502() **************************************************/
/** 6502 emulation calls this function to read a byte from   **/
/** address A of 6502 address space.                         **/
/*************************************************************/
byte Rd6502(register word A) { 

   byte x,y;

   switch(A&0xf000)
   {
      case 0x2000: /* VDP Status/Data */
         if(A&0x01) { A=VDPStatus;VDPStatus&=0x5F;VKey=1; }
         else { A=VRAM[RVAddr.W];RVAddr.W=(RVAddr.W+1)&0x3FFF; }
         return(A);
      case 0x1000: /* PIA */
         if ((A&0x03)==2) {
            /* read from PIAB */
            if ((PIAAIO&0x0f)==0) {
               x = KEYTBL[7]^KEYTBL[11]^0xff;
               y = KEYTBL[13]^KEYTBL[14]^0xff;
               return (x^y^0xff);
            } else {
               return(KEYTBL[PIAAIO&0x0f]);
            }
         }
   }
   return(RAM[A]);
 }


/** Loop6502() ************************************************/
/** 6502 emulation calls this function periodically to check **/
/** if the system hardware requires any interrupts.         **/
/*************************************************************/
byte Loop6502(M6502 *R)
{
  static byte UCount=0;
  static byte ACount=0;

  //SDL_Event myevent;

  /* Next scanline */

  CurLine=(CurLine+1)%193;

  /* Refresh scanline if needed */
  if(CurLine<192)
  {
    if(!UCount) {
      SDL_LockSurface(sdl_screen); 
      (SCR[ScrMode].Refresh)(CurLine);
      SDL_UnlockSurface(sdl_screen);
    }
    R->IPeriod=HPeriod;
    return(INT_NONE);
  }

  /* End of screen reached... */

  /* Wait a while if we got here too quickly */
  do {
     NewTimer=sdl_getticks();
  } while ((NewTimer - OldTimer) < SCREEN_DELAY);
 
  OldTimer=sdl_getticks();

  /* Set IPeriod to the beginning of next screen */
  R->IPeriod=VPeriod-HPeriod*192;

  /* Check joysticks */
  Joysticks(); 

  /* Autofire emulation */
  ACount=(ACount+1)&0x07;
  if(ACount>3)
  {
    if(AutoA) { JoyState[0]|=0x0040;JoyState[1]|=0x0040; }
    if(AutoB) { JoyState[0]|=0x4000;JoyState[1]|=0x4000; }
  }

  /* Writing interrupt timestamp into sound log */
  if(LogSnd&&SndStream) fputc(0xFF,SndStream);

  /* Flush any accumulated sound changes */
  /* Refresh screen if needed */
  if(UCount) UCount--; else { UCount=UPeriod-1; RefreshScreen();  }

  /* Setting VDPStatus flags */
  VDPStatus=(VDPStatus&0xDF)|0x80;

  /* Checking sprites: */
  if(ScrMode) CheckSprites();

  /* If exit requested, return INT_QUIT */
  if(ExitNow) return(INT_QUIT);

  /* Generate VDP interrupt . */
  return(VKey&&(VDP[1]&0x20)? INT_IRQ:INT_NONE);
}



/** VDPOut() *************************************************/
/** Emulator calls this function to write byte V into a VDP **/
/** register R.                                             **/
/*************************************************************/
void VDPOut(register byte R,register byte V)
{ 
  register byte J;

  switch(R)  
  {
    case  0: switch(((V&0x0E)>>1)|(VDP[1]&0x18))
             {
               case 0x10: J=0;break;
               case 0x00: J=1;break;
               case 0x01: J=2;break;
               case 0x08: J=3;break;
               default:   J=ScrMode;
             }   
             if(J!=ScrMode)
             {
               ChrTab=VRAM+((long)(VDP[2]&SCR[J].R2)<<10);
               ChrGen=VRAM+((long)(VDP[4]&SCR[J].R4)<<11);
               ColTab=VRAM+((long)(VDP[3]&SCR[J].R3)<<6);
               SprTab=VRAM+((long)(VDP[5]&SCR[J].R5)<<7);
               SprGen=VRAM+((long)VDP[6]<<11);
               ScrMode=J;
             }
             break;
    case  1: switch(((VDP[0]&0x0E)>>1)|(V&0x18))
             {
               case 0x10: J=0;break;
               case 0x00: J=1;break;
               case 0x01: J=2;break;
               case 0x08: J=3;break;
               default:   J=ScrMode;
             }   
             if(J!=ScrMode)
             {
               ChrTab=VRAM+((long)(VDP[2]&SCR[J].R2)<<10);
               ChrGen=VRAM+((long)(VDP[4]&SCR[J].R4)<<11);
               ColTab=VRAM+((long)(VDP[3]&SCR[J].R3)<<6);
               SprTab=VRAM+((long)(VDP[5]&SCR[J].R5)<<7);
               SprGen=VRAM+((long)VDP[6]<<11);
               ScrMode=J;
             }
             break;       
    case  2: ChrTab=VRAM+((long)(V&SCR[ScrMode].R2)<<10);break;
    case  3: ColTab=VRAM+((long)(V&SCR[ScrMode].R3)<<6);break;
    case  4: ChrGen=VRAM+((long)(V&SCR[ScrMode].R4)<<11);break;
    case  5: SprTab=VRAM+((long)(V&SCR[ScrMode].R5)<<7);break;
    case  6: V&=0x3F;SprGen=VRAM+((long)V<<11);break;
    case  7: FGColor=V>>4;BGColor=V&0x0F;break;

  }
  VDP[R]=V;
  return;
} 

/** CheckSprites() *******************************************/
/** This function is periodically called to check for the   **/
/** sprite collisions and 5th sprite, and set appropriate   **/
/** bits in the VDP status register.                        **/
/*************************************************************/
void CheckSprites(void)
{
  register word LS,LD;
  register byte DH,DV,*PS,*PD,*T;
  byte I,J,N,*S,*D;

  VDPStatus=(VDPStatus&0x9F)|0x1F;
  for(N=0,S=SprTab;(N<32)&&(S[0]!=208);N++,S+=4);

  if(Sprites16x16)
  {
    for(J=0,S=SprTab;J<N;J++,S+=4)
      if(S[3]&0x0F)
        for(I=J+1,D=S+4;I<N;I++,D+=4)
          if(D[3]&0x0F)
          {
            DV=S[0]-D[0];
            if((DV<16)||(DV>240))
            {
              DH=S[1]-D[1];
              if((DH<16)||(DH>240))
              {
                PS=SprGen+((long)(S[2]&0xFC)<<3);
                PD=SprGen+((long)(D[2]&0xFC)<<3);
                if(DV<16) PD+=DV; else { DV=256-DV;PS+=DV; }
                if(DH>240) { DH=256-DH;T=PS;PS=PD;PD=T; }
                while(DV<16)
                {
                  LS=((word)*PS<<8)+*(PS+16);
                  LD=((word)*PD<<8)+*(PD+16);
                  if(LD&(LS>>DH)) break;
                  else { DV++;PS++;PD++; }
                }
                if(DV<16) { VDPStatus|=0x20;return; }
              }
            }
          }
  }
  else
  {
    for(J=0,S=SprTab;J<N;J++,S+=4)
      if(S[3]&0x0F)
        for(I=J+1,D=S+4;I<N;I++,D+=4)
          if(D[3]&0x0F)
          {
            DV=S[0]-D[0];
            if((DV<8)||(DV>248))
            {
              DH=S[1]-D[1];
              if((DH<8)||(DH>248))
              {
                PS=SprGen+((long)S[2]<<3);
                PD=SprGen+((long)D[2]<<3);
                if(DV<8) PD+=DV; else { DV=256-DV;PS+=DV; }
                if(DH>248) { DH=256-DH;T=PS;PS=PD;PD=T; }
                while((DV<8)&&!(*PD&(*PS>>DH))) { DV++;PS++;PD++; }
                if(DV<8) { VDPStatus|=0x20;return; }
              }
            }
          }
  }
}

/** Play() ***************************************************/
/** Log and play sound of given frequency (Hz) and volume   **/
/** (0..255) via given channel (0..3).                      **/
/*************************************************************/
#if 0
void Play(int C,int F,int V)
{
  static char SndHeader[20] =
    "SND\032\001\005\074\0\0\0\0\0\0\0\0\0\376\003\002";
  static char SndOff[17] =
    "\0\0\0\0\001\0\0\0\002\0\0\0\003\0\0\0";
  static byte OldLogSnd=0;
  byte Buf[4];   

  if(LogSnd&&SndName)
  {
    /* If SndStream closed, open it */
    if(!SndStream)
      if(SndStream=fopen(SndName,"wb"))
        fwrite(SndHeader,1,19,SndStream);
      else
        SndName=NULL;
    /* If SndStream ok, log into it */
    if(SndStream)
    {
      /* Log sound change, if SndStream open */
      Buf[0]=C;Buf[1]=F&0xFF;Buf[2]=F>>8;Buf[3]=V;
      fwrite(Buf,1,4,SndStream);
    }
  }
  else if(OldLogSnd&&SndStream) fwrite(SndOff,1,16,SndStream);

  /* Save LogSnd value for the next time */
  OldLogSnd=LogSnd;

}
#endif
