/** ColEm: portable Coleco emulator **************************/
/**                                                         **/
/**                         Common.h                        **/
/**                                                         **/
/** This file contains screen refresh drivers which are     **/
/** common for both X11 and VGA implementations. It is      **/
/** included either from Unix.c or MSDOS.c.                 **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include "funnysdl.h"

#define TOPBORDER 0

static void RefreshSprites(byte Y);

/** RefreshScreen() ******************************************/
/** Refresh screen. This function is called in the end of   **/
/** refresh cycle to show the entire screen.                **/
/*************************************************************/
void RefreshScreen(void) {
   //static int part;

   if ( SDL_MUSTLOCK(sdl_screen) ) {
        SDL_UnlockSurface(sdl_screen);
    }
    /* Update just the part of the display that we've changed */
    SDL_UpdateRect(sdl_screen, 0, 0, 0,0);
}

/** RefreshSprites() *****************************************/
/** This function is called from RefreshLine#() to refresh  **/
/** sprites.                                                **/
/*************************************************************/
void RefreshSprites(register byte Y)
{
  register byte H;
  register int C;
  register byte *PT,*AT;
  register int L,K;
  register unsigned int M;
  register Uint8 *P,*Q;

  H=Sprites16x16? 16:8;
  C=0;M=0;L=0;AT=SprTab-4;
  do
  {
    M<<=1;AT+=4;L++;    /* Iterating through SprTab */
    K=AT[0];            /* K = sprite Y coordinate */
    if(K==208) break;   /* Iteration terminates if Y=208 */
    if(K>256-H) K-=256; /* Y coordinate may be negative */

    /* Mark all valid sprites with 1s, break at 4 sprites */
    if((Y>K)&&(Y<=K+H)) { M|=1;if(++C==4) break; }
  }
  while(L<32);

  for(;M;M>>=1,AT-=4)
    if(M&1)
    {
      C=AT[3];                  /* C = sprite attributes */
      L=C&0x80? AT[1]-32:AT[1]; /* Sprite may be shifted left by 32 */
      C&=0x0F;                  /* C = sprite color */

      if((L<256)&&(L>-H)&&C)
      {
        K=AT[0];                /* K = sprite Y coordinate */
        if(K>256-H) K-=256;     /* Y coordinate may be negative */

        P=(Uint8 *)sdl_screen->pixels+((HEIGHT-192)/2)*sdl_screen->pitch*2+sdl_screen->pitch*Y*2+(L*4);
        Q=P+sdl_screen->pitch;

        PT=SprGen+((int)(H>8? AT[2]&0xFC:AT[2])<<3)+Y-K-1;

        /* Mask 1: clip left sprite boundary */
        K=L>=0? 0x0FFFF:(0x10000>>-L)-1;
        /* Mask 2: clip right sprite boundary */
        if(L>256-H) K^=((0x00200>>(H-8))<<(L-257+H))-1;
        /* Get and clip the sprite data */
        K&=((int)PT[0]<<8)|(H>8? PT[16]:0x00);

        C=sdl_colours[C];
        /* Draw left 8 pixels of the sprite */
        if(K&0xFF00)
        {
          if(K&0x8000) *(Uint32*)(P)=C;
          if(K&0x8000) *(Uint32*)(Q)=C;
          if(K&0x4000) *(Uint32*)(P+4)=C;
          if(K&0x4000) *(Uint32*)(Q+4)=C;
          if(K&0x2000) *(Uint32*)(P+8)=C;
          if(K&0x2000) *(Uint32*)(Q+8)=C;
          if(K&0x1000) *(Uint32*)(P+12)=C;
          if(K&0x1000) *(Uint32*)(Q+12)=C;
          if(K&0x0800) *(Uint32*)(P+16)=C;
          if(K&0x0800) *(Uint32*)(Q+16)=C;
          if(K&0x0400) *(Uint32*)(P+20)=C;
          if(K&0x0400) *(Uint32*)(Q+20)=C;
          if(K&0x0200) *(Uint32*)(P+24)=C;
          if(K&0x0200) *(Uint32*)(Q+24)=C;
          if(K&0x0100) *(Uint32*)(P+28)=C;
          if(K&0x0100) *(Uint32*)(Q+28)=C;
        }

        /* Draw right 8 pixels of the sprite */
        if(K&0x00FF)
        {
          if(K&0x0080) *(Uint32*)(P+32)=C;
          if(K&0x0080) *(Uint32*)(Q+32)=C;
          if(K&0x0040) *(Uint32*)(P+36)=C;
          if(K&0x0040) *(Uint32*)(Q+36)=C;
          if(K&0x0020) *(Uint32*)(P+40)=C;
          if(K&0x0020) *(Uint32*)(Q+40)=C;
          if(K&0x0010) *(Uint32*)(P+44)=C;
          if(K&0x0010) *(Uint32*)(Q+44)=C;
          if(K&0x0008) *(Uint32*)(P+48)=C;
          if(K&0x0008) *(Uint32*)(Q+48)=C;
          if(K&0x0004) *(Uint32*)(P+52)=C;
          if(K&0x0004) *(Uint32*)(Q+52)=C;
          if(K&0x0002) *(Uint32*)(P+56)=C;
          if(K&0x0002) *(Uint32*)(Q+56)=C;
          if(K&0x0001) *(Uint32*)(P+60)=C;
          if(K&0x0001) *(Uint32*)(Q+60)=C;
        }
      }
    }
}

/** RefreshLine0() *******************************************/
/** Refresh line Y (0..191) of SCREEN0, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void RefreshLine0(register byte Y)
{
  register byte X,K,Offset;
  register byte *T;
  register int FC,BC;
  Uint32 C;
  Uint8 *P;

  P=(Uint8 *)sdl_screen->pixels+sdl_screen->pitch*2*(HEIGHT-192)/2+sdl_screen->pitch*Y*2;
  //XPal[0]=BGColor? XPal[BGColor]:XPal0;

  C = sdl_colours[BGColor];

  if(!ScreenON) {
   for (X=0;X<40;X++) {
      *(Uint16*)(P) = C;
      P+=4;
   }
  } else {
    BC=sdl_colours[BGColor];
    FC=sdl_colours[FGColor];
    T=ChrTab+(Y>>3)*40;
    Offset=Y&0x07;

/* Clear left edge and centre */
    for (X=0;X<(WIDTH-240)/2;X++) {
       *(Uint16*)(P) = C;
       P+=4;
    }
    // memset(P,BC,(sdl_screen->pitch-480)/2);

    P+=(sdl_screen->pitch-480)/2;

    for(X=0;X<40;X++)
    {
      K=ChrGen[((int)*T<<3)+Offset];
      *(Uint16*)(P)=K&0x80? FC:BC;
      *(Uint16*)(P+4)=K&0x40? FC:BC;
      *(Uint16*)(P+8)=K&0x20? FC:BC;
      *(Uint16*)(P+12)=K&0x10? FC:BC;
      *(Uint16*)(P+16)=K&0x08? FC:BC;
      *(Uint16*)(P+20)=K&0x04? FC:BC;
      P+=24;
      T++;
    }

/* Clear right border */
    for (X=0;X<(WIDTH-240)/2;X++) {
       *(Uint16*)(P) = C;
       P+=4;
    }
    // memset(P,BC,(sdl_screen->pitch-480)/2);
  }

  /* RefreshBorder(Y); */
}

/** RefreshLine1() *******************************************/
/** Refresh line Y (0..191) of SCREEN1, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void RefreshLine1(register byte Y)
{
  register byte X,K,Offset;
  register int FC,BC;
  register byte *T;
  register Uint8  *P,*Q;
  register Uint32 C;

  C = sdl_colours[BGColor];

  P=(Uint8 *)sdl_screen->pixels+sdl_screen->pitch*2*(HEIGHT-192)/2+sdl_screen->pitch*Y*2;
  Q=P+sdl_screen->pitch;

  if(!ScreenON) {
   for (X=0;X<128;X++) {
      *(Uint32*)(P) = C;
      *(Uint32*)(Q) = C;
      *(Uint32*)(P+4) = C;
      *(Uint32*)(Q+4) = C;
      P+=8; Q+=8;
   }
   //memset(P,BGColor,sdl_screen->pitch);
  } else {
    T=ChrTab+(Y>>3)*32;
    Offset=Y&0x07;

    for(X=0;X<32;X++)
    {
      K=*T;
      BC=ColTab[K>>3];
      K=ChrGen[((int)K<<3)+Offset];
      FC=sdl_colours[BC>>4];
      BC=sdl_colours[BC&0x0F];
      *(Uint32*)(P)=K&0x80? FC:BC;
      *(Uint32*)(Q)=K&0x80? FC:BC;
      *(Uint32*)(P+4)=K&0x40? FC:BC;
      *(Uint32*)(Q+4)=K&0x40? FC:BC;
      *(Uint32*)(P+8)=K&0x20? FC:BC;
      *(Uint32*)(Q+8)=K&0x20? FC:BC;
      *(Uint32*)(P+12)=K&0x10? FC:BC;
      *(Uint32*)(Q+12)=K&0x10? FC:BC;
      *(Uint32*)(P+16)=K&0x08? FC:BC;
      *(Uint32*)(Q+16)=K&0x08? FC:BC;
      *(Uint32*)(P+20)=K&0x04? FC:BC;
      *(Uint32*)(Q+20)=K&0x04? FC:BC;
      *(Uint32*)(P+24)=K&0x02? FC:BC;
      *(Uint32*)(Q+24)=K&0x02? FC:BC;
      *(Uint32*)(P+28)=K&0x01? FC:BC;
      *(Uint32*)(Q+28)=K&0x01? FC:BC;
      P+=32; Q+=32;

      T++;
    }

    RefreshSprites(Y);
  }

  /* RefreshBorder(Y); */
}

/** RefreshLine2() *******************************************/
/** Refresh line Y (0..191) of SCREEN2, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void RefreshLine2(register byte Y)
{
  register byte X,K,Offset;
  register int FC,BC;
  register byte *T,*PGT,*CLT;
  register int I;
  register Uint8  *P,*Q;
  register Uint32 C;

  C = sdl_colours[BGColor];

  P=(Uint8 *)sdl_screen->pixels+sdl_screen->pitch*2*(HEIGHT-192)/2+sdl_screen->pitch*Y*2;
  Q=P+sdl_screen->pitch;

  if(!ScreenON) {
   for (X=0;X<128;X++) {
      *(Uint32*)(P) = C;
      *(Uint32*)(Q) = C;
      *(Uint32*)(P+4) = C;
      *(Uint32*)(Q+4) = C;
      P+=8; Q+=8;
   }
  } else
  {
    I=(int)(Y&0xC0)<<5;
    PGT=ChrGen+I;
    CLT=ColTab+I;
    T=ChrTab+(Y>>3)*32;
    Offset=Y&0x07;

    for(X=0;X<32;X++)
    {
      I=((int)*T<<3)+Offset;
      K=PGT[I];
      BC=CLT[I];
      FC=sdl_colours[BC>>4];
      BC=sdl_colours[BC&0x0F];


      *(Uint32*)(P)=K&0x80? FC:BC;
      *(Uint32*)(Q)=K&0x80? FC:BC;
      *(Uint32*)(P+4)=K&0x40? FC:BC;
      *(Uint32*)(Q+4)=K&0x40? FC:BC;
      *(Uint32*)(P+8)=K&0x20? FC:BC;
      *(Uint32*)(Q+8)=K&0x20? FC:BC;
      *(Uint32*)(P+12)=K&0x10? FC:BC;
      *(Uint32*)(Q+12)=K&0x10? FC:BC;
      *(Uint32*)(P+16)=K&0x08? FC:BC;
      *(Uint32*)(Q+16)=K&0x08? FC:BC;
      *(Uint32*)(P+20)=K&0x04? FC:BC;
      *(Uint32*)(Q+20)=K&0x04? FC:BC;
      *(Uint32*)(P+24)=K&0x02? FC:BC;
      *(Uint32*)(Q+24)=K&0x02? FC:BC;
      *(Uint32*)(P+28)=K&0x01? FC:BC;
      *(Uint32*)(Q+28)=K&0x01? FC:BC;
      P+=32; Q+=32;


      T++;
    }

    RefreshSprites(Y);
  }    

}

/** RefreshLine3() *******************************************/
/** Refresh line Y (0..191) of SCREEN3, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void RefreshLine3(register byte Y)
{
  register byte X,K,Offset;
  register byte *T;
  register Uint8  *P,*Q;
  register Uint32 C;

  C = sdl_colours[BGColor];

  P=(Uint8 *)sdl_screen->pixels+sdl_screen->pitch*2*(HEIGHT-192)/2+sdl_screen->pitch*Y*2;
  Q=P+sdl_screen->pitch;

  if(!ScreenON) {
   for (X=0;X<128;X++) {
      *(Uint32*)(P) = C;
      *(Uint32*)(Q) = C;
      *(Uint32*)(P+4) = C;
      *(Uint32*)(Q+4) = C;
      P+=8; Q+=8;
   }
  } else
  {
    T=ChrTab+(Y>>3)*32;
    Offset=(Y&0x1C)>>2;

    for(X=0;X<32;X++)
    {
      K=ChrGen[((int)*T<<3)+Offset];
      *(Uint32*)(P)=*(Uint32*)(P+4)=*(Uint32*)(P+8)=*(Uint32*)(P+12)=sdl_colours[K>>4];
      *(Uint32*)(Q)=*(Uint32*)(Q+4)=*(Uint32*)(Q+8)=*(Uint32*)(Q+12)=sdl_colours[K>>4];
      *(Uint32*)(P+16)=*(Uint32*)(P+20)=*(Uint32*)(P+24)=*(Uint32*)(P+12)=sdl_colours[K&0x0f];
      *(Uint32*)(Q+16)=*(Uint32*)(Q+20)=*(Uint32*)(Q+24)=*(Uint32*)(Q+12)=sdl_colours[K&0x0f];
      P+=32; Q+=32;

      T++;
    }

    RefreshSprites(Y);
  }

}
