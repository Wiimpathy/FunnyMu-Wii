#include "SDL.h"   /* All SDL App's need this */
#include <SDL_mixer.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_gfxPrimitives.h>

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>
#include <wiiuse/wpad.h>

#include "funny.h"
#include "funnysdl.h"
#include "another_ttf.h"

#define VERSION "FunnyMu Wii 0.2"

#define WIDTH  256
#define HEIGHT 208

/* Analog sticks sensitivity */
#define ANALOG_SENSITIVITY 30

/* Delay before held keys triggering */
/* higher is the value, less responsive is the key update */
#define HELD_DELAY 10

/* Direction & selection update speed when a key is being held */
/* lower is the value, faster is the key update */
#define HELD_SPEED 4

#define WPAD_BUTTONS_HELD (WPAD_BUTTON_UP | WPAD_BUTTON_DOWN | WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | \
		WPAD_CLASSIC_BUTTON_UP | WPAD_CLASSIC_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_RIGHT)

#define PAD_BUTTONS_HELD  (PAD_BUTTON_UP | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT)

#define HELPTXT_Y 207
#define SETTINGTXT_Y 216


TTF_Font *text_fontSmall;
enum {Regular, Bold, Italic, Underline};
SDL_Color Red = {255,0,0,0};
SDL_Color Green = {0,255,0,0};
SDL_Color Blue = {0,0,255,0};
SDL_Color Yellow = {255,255,0,0};
SDL_Color DarkGreen = {0,100,0,0};
SDL_Color White = {255,255,255,0};
SDL_Color Black = {0,0,0,0};
SDL_Color DarkGrey = {128,128,128,0};

SDL_Surface *openDialog;

typedef struct
{
  u8 flags;
  char filename[256];
}FILES;

FILES filelist[1000];

static int numberFiles = 34;

/* Globals */
#define PLAYBUF_SIZE 1024
static Uint8 playbuf[PLAYBUF_SIZE];
static volatile int snd_read = 0;
static SDL_AudioSpec spec;

int SaveCPU  = 1;               /* 1 = freeze when focus out */
int UseSHM   = 1;               /* 1 = use MITSHM            */
int UseSound = 1;               /* 1 = use sound             */

SDL_Surface *sdl_screen;
Uint32  sdl_colours[16];
char sdl_title[]="FunnyMu v0.41";

extern int   mode;
extern char * rom_filename;
extern void WII_VideoStart();
extern void WII_VideoStop();
//extern int PauseAudio();
extern void ResetAudio();
extern void WII_ChangeSquare(int xscale, int yscale, int xshift, int yshift);
int SN76496Init(int chip, int clock, int gain, int sample_rate);
void SN76496Update(int chip, uint16 *buffer, int length);
void SN76496Write(int chip, int data);

u16 MenuInput;

static SDL_Joystick *sdlJoystick[6] =   /* SDL's joystick structures */
{
	NULL, NULL, NULL, NULL, NULL, NULL
};

void sdl_setpalette(void) {
  /* struct { Uint8 R,G,B; } Palette[16] =
   {
     {0x00,0x00,0x00},{0x00,0x00,0x00},{0x20,0xC0,0x20},{0x60,0xE0,0x60},
     {0x20,0x20,0xE0},{0x40,0x60,0xE0},{0xA0,0x20,0x20},{0x40,0xC0,0xE0},
     {0xE0,0x20,0x20},{0xE0,0x60,0x60},{0xC0,0xC0,0x20},{0xC0,0xC0,0x80},
     {0x20,0x80,0x20},{0xC0,0x40,0xA0},{0xA0,0xA0,0xA0},{0xE0,0xE0,0xE0}
   };*/

struct { Uint8 R,G,B; } Palette[16] = {
    /* palette from MAME by S. Young */
    {0x00, 0x00, 0x00},	// 0 transparent (should be set on VDP as bg color)
    {0x00, 0x00, 0x00},	// 1 black
    {0x21, 0xc8, 0x42},	// 2 medium green
    {0x5e, 0xdc, 0x78}, // 3 light green
    {0x54, 0x55, 0xed},	// 4 dark blue
    {0x7d, 0x76, 0xfc},	// 5 light blue
    {0xd4, 0x52, 0x4d},	// 6 dark red
    {0x42, 0xeb, 0xf5},	// 7 cyan
    {0xfc, 0x55, 0x54},	// 8 medium red
    {0xff, 0x79, 0x78},	// 9 light red
    {0xd4, 0xC1, 0x54},	// A dark yellow
    {0xe6, 0xCe, 0x80},	// B light yellow
    {0x21, 0xb0, 0x3b},	// C dark green
    {0xc9, 0x5b, 0xba},	// D magenta
    {0xcc, 0xcc, 0xcc},	// E gray
    {0xff, 0xff, 0xff}	// F white
	};
   int i;

   //printf("set_palette\n");
   for (i=0;i<16;i++) {
      sdl_colours[i] = SDL_MapRGB(sdl_screen->format, 
        Palette[i].R, Palette[i].G, Palette[i].B);
      sdl_colours[i]+=sdl_colours[i]<<16;
   }
}

static void _sdl_snd_callback(void *udata, Uint8 *stream, int len)
{
  int i;
  for(i = 0; i < len; ++i)
    {
      if(snd_read == PLAYBUF_SIZE) {
         SN76496Update(0,(uint16 *)playbuf,PLAYBUF_SIZE/2);
         snd_read=0;
      }
      stream[i] = (playbuf[snd_read++] -0x6000)>>1;
    }
}

int sound_init(void) {

   SDL_AudioSpec wanted;

   wanted.freq = 22050;
   wanted.format = AUDIO_S16;
   wanted.channels = 1;
   wanted.samples = 256;
   wanted.callback = _sdl_snd_callback;
   wanted.userdata = NULL;

   if(SDL_OpenAudio(&wanted, &spec) < 0)
   {
      /*fprintf(stderr, "sdl: Couldn't open audio: %s!\n", SDL_GetError());
	    SDL_Delay(2000);*/
      return 0;
   } else {
      printf("sdl audio opened\n");
   }

   SN76496Init(0,VDP_CLOCK/15,0,spec.freq);
   SDL_PauseAudio(0);
   return(0);
}


int init_sdl(void) {

	//printf("\nInitializing SDL.\n");

	/* init SDL TTF */
	TTF_Init();

	//printf("\nTTF_Init OK.\n");

	SDL_RWops* fontsmallRW = SDL_RWFromMem((char *)another_ttf, another_ttf_size);
	text_fontSmall = TTF_OpenFontRW(fontsmallRW, 1, 12); 
 
	//printf("\nTTF_OpenFontRW OK.\n");
 
	/* Initialize defaults, Video and Audio */
	if((SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)==-1)) { 
		printf("Could not initialize SDL: %s.\n", SDL_GetError());
		exit(-1);
	}
	atexit(SDL_Quit);
	sound_init();
	SDL_Init( SDL_INIT_JOYSTICK);

	int i, nPadsConnected;
	/* Scan the joysticks */
	nPadsConnected = SDL_NumJoysticks();
	for (i = 0; i < nPadsConnected && i < 6; i++)
	{
		/* Open the joysticks for use */
		sdlJoystick[i] = SDL_JoystickOpen(i);
		  /* joystick connected? */
		 /* if (sdlJoystick[i] != NULL)		
		  printf( "Joystick %d connected!",SDL_JoystickName(i));*/	
	}

	sdl_screen = SDL_SetVideoMode(512, 420, 16, SDL_FULLSCREEN);
	openDialog = SDL_CreateRGBSurface(0, 320, 240, 32, 0, 0, 0, 0);
	sdl_setpalette();
	//SDL_WM_SetCaption(sdl_title,sdl_title);
	SDL_ShowCursor(SDL_DISABLE);

	return(0);
}

Uint32 sdl_getticks(void) {
   return(SDL_GetTicks());
}


/** This function is called on signals ***********************/
static void OnBreak(int Arg) { ExitNow=1;signal(Arg,OnBreak); }

/** InitMachine() ********************************************/
/** Allocate resources needed by Unix/X-dependent code.     **/
/*************************************************************/
int InitMachine(void)
{
  init_sdl();
  /* Catch all signals */
#ifndef WIN32
  signal(SIGHUP,OnBreak);signal(SIGINT,OnBreak);
  signal(SIGQUIT,OnBreak);signal(SIGTERM,OnBreak);
#endif
  return(1);
}

/** TrashMachine() *******************************************/
/** Deallocate all resources taken by InitMachine().        **/
/*************************************************************/
void TrashMachine(void)
{
  /*unsigned long L;
  int J;*/

  //if(Verbose) printf("Shutting down...\n");
	SDL_Delay(50);
}

static void pushKeyboardEvent( SDLKey key, bool pressed )
{
  SDL_Event event;
  event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
  event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
  event.key.keysym.scancode = 0;
  event.key.keysym.sym = key;
  event.key.keysym.unicode = 0;
  SDL_PushEvent( &event );
}

void Joysticks(void)
{
	SDL_Event sdl_event;

	while (SDL_PollEvent(&sdl_event) > 0)
	{
		switch (sdl_event.type) {		
		case SDL_KEYDOWN:
			switch(sdl_event.key.keysym.sym)
			{
			case SDLK_F2:  LogSnd=!LogSnd;break;
			case SDLK_ESCAPE: Reset6502(&CPU); break;

			case SDLK_F12: ExitNow=1;break;
			#ifdef DEBUG
			case SDLK_F1:  CPU.Trace=!CPU.Trace;break;
			#endif

			case SDLK_z: KEYTBL[13]&=0xf5; break;
			case SDLK_a: KEYTBL[13]&=0xee; break;
			case SDLK_q: KEYTBL[13]&=0xe7; break;
			case SDLK_2: KEYTBL[13]&=0xcf; break;
			case SDLK_x: KEYTBL[13]&=0xed; break;
			case SDLK_s: KEYTBL[13]&=0xde; break;
			case SDLK_w: KEYTBL[13]&=0xf3; break;
			case SDLK_3: KEYTBL[13]&=0x9f; break;
			case SDLK_c: KEYTBL[13]&=0xdd; break;
			case SDLK_d: KEYTBL[13]&=0xbe; break;
			case SDLK_e: KEYTBL[13]&=0xeb; break;
			case SDLK_4: KEYTBL[13]&=0xd7; break;
			case SDLK_v: KEYTBL[13]&=0xbd; break;
			case SDLK_f: KEYTBL[13]&=0xfc; break;
			case SDLK_r: KEYTBL[13]&=0xdb; break;
			case SDLK_5: KEYTBL[13]&=0xb7; break;
			case SDLK_b: KEYTBL[13]&=0xf9; break;
			case SDLK_g: KEYTBL[13]&=0xfa; break;
			case SDLK_t: KEYTBL[13]&=0xbb; break;
			case SDLK_6: KEYTBL[13]&=0xaf; break;
			case SDLK_1: KEYTBL[14]&=0xf3; break;
			case SDLK_BACKSPACE: KEYTBL[13]&=0xf6; break;

			case SDLK_LCTRL:  KEYTBL[7]&=0x7f; break;
			case SDLK_LSHIFT: KEYTBL[13]&=0x7f; break;
			case SDLK_RSHIFT: KEYTBL[14]&=0x7f; break;
			case SDLK_TAB: KEYTBL[11]&=0x7f; break;

			case SDLK_COLON: KEYTBL[7]&=0xf5; break;
			case SDLK_p: KEYTBL[7]&=0xee; break;
			case SDLK_SEMICOLON: KEYTBL[7]&=0xe7; break;
			case SDLK_SLASH: KEYTBL[7]&=0xcf; break;
			case SDLK_0: KEYTBL[7]&=0xed; break;
			case SDLK_o: KEYTBL[7]&=0xde; break;
			case SDLK_l: KEYTBL[7]&=0xf3; break;
			case SDLK_PERIOD: KEYTBL[7]&=0x9f; break;
			case SDLK_9: KEYTBL[7]&=0xdd; break;
			case SDLK_i: KEYTBL[7]&=0xbe; break;
			case SDLK_k: KEYTBL[7]&=0xeb; break;
			case SDLK_COMMA: KEYTBL[7]&=0xd7; break;
			case SDLK_8: KEYTBL[7]&=0xbd; break;
			case SDLK_u: KEYTBL[7]&=0xfc; break;
			case SDLK_j: KEYTBL[7]&=0xdb; break;
			case SDLK_m: KEYTBL[7]&=0xb7; break;
			case SDLK_7: KEYTBL[7]&=0xf9; break;
			case SDLK_y: KEYTBL[7]&=0xfa; break;
			case SDLK_h: KEYTBL[7]&=0xbb; break;
			case SDLK_n: KEYTBL[7]&=0xaf; break;
			case SDLK_RETURN: KEYTBL[7]&=0xf6; break;
			case SDLK_SPACE: KEYTBL[11]&=0xf3; break;

			case SDLK_DOWN:   KEYTBL[14]&=0xfd;break;
			case SDLK_UP:     KEYTBL[14]&=0xf7;break;
			case SDLK_LEFT:   KEYTBL[14]&=0xdf;break;
			case SDLK_RIGHT:  KEYTBL[14]&=0xfb;break;
      default: break;
			}
		break;
		case SDL_KEYUP:
			switch(sdl_event.key.keysym.sym)
			{
			case SDLK_z: KEYTBL[13]|=0x0a; break;
			case SDLK_a: KEYTBL[13]|=0x11; break;
			case SDLK_q: KEYTBL[13]|=0x18; break;
			case SDLK_2: KEYTBL[13]|=0x30; break;
			case SDLK_x: KEYTBL[13]|=0x12; break;
			case SDLK_s: KEYTBL[13]|=0x21; break;
			case SDLK_w: KEYTBL[13]|=0x0c; break;
			case SDLK_3: KEYTBL[13]|=0x60; break;
			case SDLK_c: KEYTBL[13]|=0x22; break;
			case SDLK_d: KEYTBL[13]|=0x41; break;
			case SDLK_e: KEYTBL[13]|=0x14; break;
			case SDLK_4: KEYTBL[13]|=0x28; break;
			case SDLK_v: KEYTBL[13]|=0x42; break;
			case SDLK_f: KEYTBL[13]|=0x03; break;
			case SDLK_r: KEYTBL[13]|=0x24; break;
			case SDLK_5: KEYTBL[13]|=0x48; break;
			case SDLK_b: KEYTBL[13]|=0x06; break;
			case SDLK_g: KEYTBL[13]|=0x05; break;
			case SDLK_t: KEYTBL[13]|=0x44; break;
			case SDLK_6: KEYTBL[13]|=0x50; break;
			case SDLK_1: KEYTBL[14]|=0x0c; break;
			case SDLK_BACKSPACE: KEYTBL[13]|=0x09; break;

			case SDLK_COLON: KEYTBL[7]|=0x0a; break;
			case SDLK_p: KEYTBL[7]|=0x11; break;
			case SDLK_SEMICOLON: KEYTBL[7]|=0x18; break;
			case SDLK_SLASH: KEYTBL[7]|=0x30; break;
			case SDLK_0: KEYTBL[7]|=0x12; break;
			case SDLK_o: KEYTBL[7]|=0x21; break;
			case SDLK_l: KEYTBL[7]|=0x0c; break;
			case SDLK_PERIOD: KEYTBL[7]|=0x60; break;
			case SDLK_9: KEYTBL[7]|=0x22; break;
			case SDLK_i: KEYTBL[7]|=0x41; break;
			case SDLK_k: KEYTBL[7]|=0x14; break;
			case SDLK_COMMA: KEYTBL[7]|=0x28; break;
			case SDLK_8: KEYTBL[7]|=0x42; break;
			case SDLK_u: KEYTBL[7]|=0x03; break;
			case SDLK_j: KEYTBL[7]|=0x24; break;
			case SDLK_m: KEYTBL[7]|=0x48; break;
			case SDLK_7: KEYTBL[7]|=0x06; break;
			case SDLK_y: KEYTBL[7]|=0x05; break;
			case SDLK_h: KEYTBL[7]|=0x44; break;
			case SDLK_n: KEYTBL[7]|=0x50; break;
			case SDLK_RETURN: KEYTBL[7]|=0x09; break;
			case SDLK_SPACE: KEYTBL[11]|=0x0c; break;

			case SDLK_LCTRL:  KEYTBL[7]|=0x80; break;
			case SDLK_LSHIFT: KEYTBL[13]|=0x80; break;
			case SDLK_RSHIFT: KEYTBL[14]|=0x80; break;
			case SDLK_TAB: KEYTBL[11]|=0x80; break;

			case SDLK_DOWN:   KEYTBL[14]|=0x02;break;
			case SDLK_UP:     KEYTBL[14]|=0x08;break;
			case SDLK_LEFT:   KEYTBL[14]|=0x20;break;
			case SDLK_RIGHT:  KEYTBL[14]|=0x04;break;
      default: break;
			}
		break;

		case SDL_QUIT:
			ExitNow=1;
			break;

		case SDL_JOYHATMOTION:
			pushKeyboardEvent(SDLK_LEFT,false);
			pushKeyboardEvent(SDLK_RIGHT,false);
			pushKeyboardEvent(SDLK_UP,false);
			pushKeyboardEvent(SDLK_DOWN,false);
			switch( sdl_event.jhat.value) {	
			case SDL_HAT_LEFT:
				pushKeyboardEvent(SDLK_LEFT,true);				
			break;
			case SDL_HAT_RIGHT:
				pushKeyboardEvent(SDLK_RIGHT,true);				
			break;
			case SDL_HAT_UP:				
				pushKeyboardEvent(SDLK_UP,true);
				break;
			case SDL_HAT_DOWN:
				pushKeyboardEvent(SDLK_DOWN,true);		
			break;
			}
		break;
				

		case SDL_JOYBUTTONDOWN:	
			switch (sdl_event.jbutton.which)
			{
				/* Wiimote */
				case 0:							
					if(sdl_event.jbutton.button==0)			/* Wiimote 1 Button A */
						Reset6502(&CPU);
					else if(sdl_event.jbutton.button==1)		/* Wiimote 1 Button B */
						//KEYTBL[13]&=0xde; //S
						pushKeyboardEvent(SDLK_s,true);
					else if(sdl_event.jbutton.button==2)		/* Wiimote 1 Button 1 */
						//KEYTBL[13]&=0x7f; //Lshift
						pushKeyboardEvent(SDLK_LSHIFT,true);
					else if(sdl_event.jbutton.button==3)		/* Wiimote 1 Button 2 */
						//KEYTBL[14]&=0x7f; //Rshift
						pushKeyboardEvent(SDLK_RSHIFT,true);
					else if(sdl_event.jbutton.button==6)		/* Wiimote 1 Button Home */
					{
            GameLoop = false;
						ExitNow=1;
						break;
					}
				break;
			}
			break;
		
		case SDL_JOYBUTTONUP:
			switch (sdl_event.jbutton.which) 
			{
				case 0:							/* Wiimote */
					if(sdl_event.jbutton.button==1)
						//KEYTBL[13]|=0x21; //S
						pushKeyboardEvent(SDLK_s,false);
					else if(sdl_event.jbutton.button==2)
						//KEYTBL[13]|=0x80; //Lshift
						pushKeyboardEvent(SDLK_LSHIFT,false);
					else if(sdl_event.jbutton.button==3)
						//KEYTBL[14]|=0x80; //Lshift
						pushKeyboardEvent(SDLK_RSHIFT,false);
				break;
			}
		break;
		}
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Nunchuck/Classic controller Left stick x
 */
static int wpad_StickX(WPADData *data, u8 right)
{
	struct joystick_t* js = NULL;

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
			js = right ? NULL : &data->exp.nunchuk.js;
			break;

		case WPAD_EXP_CLASSIC:
			js = right ? &data->exp.classic.rjs : &data->exp.classic.ljs;
			break;

		default:
			break;
	}

	if (js)
	{
		/* raw X position */
		int pos = js->pos.x;

		/* X range calibration */
		int min = js->min.x;
		int max = js->max.x;
		int center = js->center.x;
 
		/* value returned could be above calibration limits */
		if (pos > max) return 127;
		if (pos < min) return -128;
		
		/* adjust against center position */
		pos -= center;

		/* return interpolated range [-128;127] */
		if (pos > 0)
		{
			return (int)(127.0 * ((float)pos / (float)(max - center)));
		}
		else
		{
			return (int)(128.0 * ((float)pos / (float)(center - min)));
		}
	}

	return 0;
}

/*-----------------------------------------------------------------------*/
/**
 * Nunchuck/Classic controller Left stick y
 */
static int wpad_StickY(WPADData *data, u8 right)
{
	struct joystick_t* js = NULL;

	switch (data->exp.type)
	{
		case WPAD_EXP_NUNCHUK:
			js = right ? NULL : &data->exp.nunchuk.js;
			break;

		case WPAD_EXP_CLASSIC:
			js = right ? &data->exp.classic.rjs : &data->exp.classic.ljs;
			break;

		default:
			break;
	}

	if (js)
	{
		/* raw Y position */
		int pos = js->pos.y;

		/* Y range calibration */
		int min = js->min.y;
		int max = js->max.y;
		int center = js->center.y;
 
		/* value returned could be above calibration limits */
		if (pos > max) return 127;
		if (pos < min) return -128;
		
		/* adjust against center position */
		pos -= center;

		/* return interpolated range [-128;127] */
		if (pos > 0)
		{
			return (int)(127.0 * ((float)pos / (float)(max - center)));
		}
		else
		{
			return (int)(128.0 * ((float)pos / (float)(center - min)));
		}
	}

	return 0;
}

/*-----------------------------------------------------------------------*/
/**
 * Menu inputs used in dialogs
 */
void Input_Update(void)
{
	static int held_cnt = 0;

	/* PAD status update */
	PAD_ScanPads();

	/* PAD pressed keys */
	s16 pp = PAD_ButtonsDown(0);

	/* PAD held keys (direction/selection) */
	s16 hp = PAD_ButtonsHeld(0) & PAD_BUTTONS_HELD;

	/* PAD analog sticks (handled as PAD held direction keys) */
	s8 x = PAD_StickX(0);
	s8 y = PAD_StickY(0);
	if (x > ANALOG_SENSITIVITY)       hp |= PAD_BUTTON_RIGHT;
	else if (x < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_LEFT;
	else if (y > ANALOG_SENSITIVITY)  hp |= PAD_BUTTON_UP;
	else if (y < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_DOWN;

	/* WPAD status update */
	WPAD_ScanPads();
	WPADData *data = WPAD_Data(0);

	/* WPAD pressed keys */
	u32 pw = data->btns_d;

	/* WPAD held keys (direction/selection) */
	u32 hw = data->btns_h & WPAD_BUTTONS_HELD;

	/* WPAD analog sticks (handled as PAD held direction keys) */
	x = wpad_StickX(data, 0);
	y = wpad_StickY(data, 0);

	if (x > ANALOG_SENSITIVITY)       hp |= PAD_BUTTON_RIGHT;
	else if (x < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_LEFT;
	else if (y > ANALOG_SENSITIVITY)  hp |= PAD_BUTTON_UP;
	else if (y < -ANALOG_SENSITIVITY) hp |= PAD_BUTTON_DOWN;

	/* check if any direction/selection key is being held or just being pressed/released */
	if (pp||pw) held_cnt = 0;
	else if (hp||hw) held_cnt++;
	else held_cnt = 0;
		
	/* initial delay (prevents triggering to start immediately) */
	if (held_cnt > HELD_DELAY)
	{
		/* key triggering */
		pp |= hp;
		pw |= hw;

		/* delay until next triggering (adjusts direction/selection update speed) */
		held_cnt -= HELD_SPEED;
	}

	/* Wiimote & Classic Controller direction keys */
	if(data->ir.valid)
	{
		/* Wiimote is handled vertically */
		if (pw & (WPAD_BUTTON_UP))          pp |= PAD_BUTTON_UP;
		else if (pw & (WPAD_BUTTON_DOWN))   pp |= PAD_BUTTON_DOWN;
		else if (pw & (WPAD_BUTTON_LEFT))   pp |= PAD_BUTTON_LEFT;
		else if (pw & (WPAD_BUTTON_RIGHT))  pp |= PAD_BUTTON_RIGHT;
	}
	else
	{
		/* Wiimote is handled horizontally */
		if (pw & (WPAD_BUTTON_UP))          pp |= PAD_BUTTON_LEFT;
		else if (pw & (WPAD_BUTTON_DOWN))   pp |= PAD_BUTTON_RIGHT;
		else if (pw & (WPAD_BUTTON_LEFT))   pp |= PAD_BUTTON_DOWN;
		else if (pw & (WPAD_BUTTON_RIGHT))  pp |= PAD_BUTTON_UP;
	}

	/* Classic Controller direction keys */
	if (pw & WPAD_CLASSIC_BUTTON_UP)          pp |= PAD_BUTTON_UP;
	else if (pw & WPAD_CLASSIC_BUTTON_DOWN)   pp |= PAD_BUTTON_DOWN;
	else if (pw & WPAD_CLASSIC_BUTTON_LEFT)   pp |= PAD_BUTTON_LEFT;
	else if (pw & WPAD_CLASSIC_BUTTON_RIGHT)  pp |= PAD_BUTTON_RIGHT;

	/* WPAD button keys */
	if (pw & (WPAD_BUTTON_2|WPAD_BUTTON_A|WPAD_CLASSIC_BUTTON_A))  pp |= PAD_BUTTON_A;
	if (pw & (WPAD_BUTTON_1|WPAD_BUTTON_B|WPAD_CLASSIC_BUTTON_B))  pp |= PAD_BUTTON_B;

	if (pw & (WPAD_BUTTON_HOME|WPAD_CLASSIC_BUTTON_HOME))        pp |= PAD_BUTTON_START;
	if (pw & (WPAD_BUTTON_MINUS|WPAD_CLASSIC_BUTTON_MINUS))        pp |= PAD_TRIGGER_Z;
	if (pw & (WPAD_BUTTON_PLUS|WPAD_CLASSIC_BUTTON_PLUS))        pp |= PAD_TRIGGER_L;

	/* Update menu inputs */
	MenuInput = pp;
}

/*-----------------------------------------------------------------------*/
/**
 * File_ShrinkName & File_SplitPath from Hatari
 */
void File_ShrinkName(char *pDestFileName, const char *pSrcFileName, int maxlen)
{
	int srclen = strlen(pSrcFileName);
	if (srclen < maxlen)
		strcpy(pDestFileName, pSrcFileName);  /* It fits! */
	else
	{
		assert(maxlen > 6);
		strncpy(pDestFileName, pSrcFileName, maxlen/2);
		if (maxlen&1)  /* even or uneven? */
			pDestFileName[maxlen/2-1] = 0;
		else
			pDestFileName[maxlen/2-2] = 0;
		strcat(pDestFileName, "...");
		strcat(pDestFileName, &pSrcFileName[strlen(pSrcFileName)-maxlen/2+1]);
	}
}

static int FileSortCallback(const void *f1, const void *f2)
{
  /* Special case for implicit directories */
  if(((FILES *)f1)->filename[0] == '.' || ((FILES *)f2)->filename[0] == '.')
  {
    if(strcmp(((FILES *)f1)->filename, ".") == 0) { return -1; }
    if(strcmp(((FILES *)f2)->filename, ".") == 0) { return 1; }
    if(strcmp(((FILES *)f1)->filename, "..") == 0) { return -1; }
    if(strcmp(((FILES *)f2)->filename, "..") == 0) { return 1; }
  }
  
  /* If one is a file and one is a directory the directory is first. */
  if(((FILES *)f1)->flags && !((FILES *)f2)->flags) return -1;
  if(!((FILES *)f1)->flags  && ((FILES *)f2)->flags) return 1;
  
  return strcasecmp(((FILES *)f1)->filename, ((FILES *)f2)->filename);
}

int Parse_Directory(DIR *dir)
{
	int count =0;
	struct dirent *dp = NULL;

	/* list entries */
	while ((dp=readdir(dir)) != NULL  && (count < 1000))
	{
		if (dp->d_name[0] == '.' && strcmp(dp->d_name, "..") != 0 )
			continue;

		memset(&filelist[count], 0, sizeof (FILES));
		sprintf(filelist[count].filename,"%s",dp->d_name);

		if (dp->d_type == DT_DIR)
		{
			filelist[count].flags = 1;
    		}
		count++;
	}

	/* close directory */
	closedir(dir);

	/* Sort the file list */
	qsort(filelist, count, sizeof(FILES), FileSortCallback);

	return count;
}

/*-----------------------------------------------------------------------*/
/**
 * Draw a text string using TTF.
 *
 * bool cut: define a max string length and cut if it exceeds.
 * int style: Regular, Bold, Italic, Underline.
 */
void DrawText(int x, int y, bool cut, char * text, TTF_Font *theFont, int style, SDL_Color Color,  SDL_Surface *screen)
{
	if (style == Regular)
		TTF_SetFontStyle(theFont, TTF_STYLE_NORMAL);
	else if (style == Bold)
		TTF_SetFontStyle(theFont, TTF_STYLE_BOLD);
	else if (style == Italic)
		TTF_SetFontStyle(theFont, TTF_STYLE_ITALIC);
	else if (style == Underline)
		TTF_SetFontStyle(theFont, TTF_STYLE_UNDERLINE);

	SDL_Surface *sText = TTF_RenderText_Solid( theFont, text, Color );
	SDL_Rect rcDest = {x,y,10,10};
	SDL_Rect srcDest = {0,0,235,20};

	if (cut)
		SDL_BlitSurface( sText,&srcDest, screen,&rcDest );
	else
		SDL_BlitSurface( sText,NULL, screen,&rcDest );

	SDL_FreeSurface( sText );
}

void Draw_Border(int x, int y, int w, int h)
{
	SDL_Rect box;

	/* Draw upper border: */
	box.x = x;
	box.y = y;
	box.w = w;
	box.h = 2;
	SDL_FillRect(sdl_screen, &box, SDL_MapRGB(sdl_screen->format,0xff,0x00, 0x00));

	/* Draw left border: */
	box.x = x;
	box.y = y;
	box.w = 2;
	box.h = h;
	SDL_FillRect(sdl_screen, &box, SDL_MapRGB(sdl_screen->format,0xff,0x00, 0x00));

	/* Draw bottom border: */
	box.x = x;
	box.y = y + h;
	box.w = w;
	box.h = 2;
	SDL_FillRect(sdl_screen, &box, SDL_MapRGB(sdl_screen->format,0xff,0x00, 0x00));

	/* Draw right border: */
	box.x = x + w;
	box.y = y;
	box.w = 2;
	box.h = h + 2;
	SDL_FillRect(sdl_screen, &box, SDL_MapRGB(sdl_screen->format,0xff,0x00, 0x00));
}

void Browsefiles(char *dir_path, char *selectedFile)
{
	int i;
	int loop=0;
	int fileStart=0, fileEnd=16, displayFiles=16;
	int highlight = 0;
	int realSelected = 0;
	char fileNames[256];
	char lastdir[256];


	WII_ChangeSquare(640, 480, 200, 180);

	/* Open directory */
	DIR *dir = opendir(dir_path);
	if (dir == NULL)
	{
		loop = 1;
	}

	/* Keep current path */
	sprintf(lastdir, dir_path);

	/* "Clear" filelist names */
	for (i=0;i<numberFiles;i++)
		filelist[i].filename[0] = '\0';

	numberFiles = Parse_Directory(dir);

	if ((fileEnd > numberFiles) & (displayFiles > numberFiles))
	{
		fileEnd = numberFiles;
		displayFiles = numberFiles;
	}

	SDL_Rect rect = {0,0,320,280};
	SDL_Rect box = {27, 32, 258, 164};
	SDL_FillRect(openDialog, &rect, SDL_MapRGB(openDialog->format,30,30,30)); // green sdl_screen
	SDL_FillRect(openDialog, &box, SDL_MapRGB(openDialog->format,0,0,0));
	SDL_BlitSurface(openDialog, &rect, sdl_screen, &rect);
	SDL_UpdateRect(sdl_screen, 0, 0, rect.w, rect.h);

	do
	{
		Input_Update();

		if (MenuInput & PAD_BUTTON_UP)
		{
			SDL_BlitSurface(openDialog, &rect, sdl_screen, &rect);
			highlight--;
			if (realSelected > 0)
				realSelected--;
			if (highlight < 0)
			{
				fileStart--;
				highlight = 0;
			}
			if (fileStart < 0)
				fileStart = 0;
			fileEnd = fileStart + displayFiles;
			SDL_Delay(20);
		}
		else if (MenuInput & PAD_BUTTON_DOWN)
		{
			SDL_BlitSurface(openDialog, &rect, sdl_screen, &rect);
			if (realSelected < numberFiles-1)
				realSelected++;
			if (highlight < displayFiles)
			{
				highlight++;
			}
			if (highlight > (displayFiles-1))
			{
				highlight = (displayFiles-1);
				fileStart++;
			}
			if (fileStart  > (numberFiles -displayFiles))
			{
				fileStart = numberFiles-displayFiles;
			}
				fileEnd = fileStart + displayFiles;
				SDL_Delay(20);
		}
		else if (MenuInput & PAD_BUTTON_LEFT)
		{
			SDL_BlitSurface(openDialog, &rect, sdl_screen, &rect);

			if (fileStart > 15)
			{
				fileStart = fileStart - 15;
			}
			else
			{
				fileStart = fileStart - 7;

				if (fileStart < 0)
					fileStart = 0;
			}

			realSelected = fileStart;
			highlight = 0;
			fileEnd = fileStart + displayFiles;
			SDL_Delay(20);
		}
		else if (MenuInput & PAD_BUTTON_RIGHT)
		{
			SDL_BlitSurface(openDialog, &rect, sdl_screen, &rect);
				fileStart = fileEnd;
				realSelected = fileEnd;
				highlight = 0;
				fileEnd = fileStart + displayFiles;

			if (fileStart  > (numberFiles -displayFiles))
			{
				fileStart = numberFiles-displayFiles;
				realSelected = fileStart;
				highlight = 0;
				fileEnd = fileStart + displayFiles;
			}
				SDL_Delay(20);
		}
		else if (MenuInput & PAD_BUTTON_A)
		{
			/* It's a directory */
			if (filelist[realSelected].flags == 1)
			{
				char currentdir[256];
				sprintf(currentdir,"%s/%s", lastdir, filelist[realSelected].filename);
				sprintf(lastdir, currentdir);
				/* Update the path used to load files */
				sprintf(dir_path,"%s/", currentdir);

				DIR *dir = opendir(currentdir);
				/* "Clear" filelist names */
				for (i=0;i<numberFiles;i++)
					filelist[i].filename[0] = '\0';

				numberFiles = Parse_Directory(dir);

				fileStart = 0;
				realSelected = 0;
				highlight = 0;
				fileEnd=16;
				displayFiles=16;

				if ((fileEnd > numberFiles) & (displayFiles > numberFiles))
				{
					fileEnd = numberFiles;
					displayFiles = numberFiles;
				}
			
				SDL_Rect box = {27, 32, 258, 164};
				SDL_FillRect(openDialog, &box, SDL_MapRGB(openDialog->format,0,0,0));
				SDL_BlitSurface(openDialog, &box, sdl_screen, &box);
				SDL_Rect selecter = {30,33+(0*10),252,11};
				SDL_FillRect(sdl_screen, &selecter, SDL_MapRGB(sdl_screen->format,0x00,0x64, 0x00));

				SDL_UpdateRect(sdl_screen, 0, 0, rect.w, rect.h);
				SDL_Delay(200);
			}
			/* It's a file */
			else
			{
				strncpy(selectedFile, filelist[realSelected].filename, sizeof(filelist[realSelected].filename));
				loop=1;
			}
		}
		/*else if (MenuInput & PAD_BUTTON_B)
		{
			selectedFile = NULL;
			loop=1;
		}*/
		else if(MenuInput & PAD_BUTTON_START)
		{
			//return;
			exit(0);
		}
		/* Display the files in the scrolling window */
		for (i=0;i<16;i++)
		{
			if ((fileStart + i) <= (fileStart + sizeof(filelist)))
			{
				if (i == highlight)
				{
					SDL_Rect selecter = {30,33+(i*10),252,11};
					SDL_FillRect(sdl_screen, &selecter, SDL_MapRGB(sdl_screen->format,0x00,0x64, 0x00));
				}
				File_ShrinkName(fileNames , filelist[fileStart + i].filename,39);
				DrawText(40,35+(i*10), true, fileNames, text_fontSmall, Regular, White, sdl_screen);
			}
		}
		Draw_Border(25, 30, 260, 166);
		DrawText(25,19,false,"Select a file :", text_fontSmall, Regular, White, sdl_screen);
		DrawText(10,HELPTXT_Y-2,false,"Home to exit emulator.", text_fontSmall, Regular, White, sdl_screen);
		DrawText(200,SETTINGTXT_Y-2,false, VERSION, text_fontSmall, Regular, DarkGrey, sdl_screen);

		SDL_UpdateRect(sdl_screen, 0, 0, 512, 420);
		SDL_Flip(sdl_screen);
	} while (loop == 0);


    	SDL_FillRect(sdl_screen, NULL, SDL_MapRGB(sdl_screen->format, 0, 0, 0));
	SDL_Flip(sdl_screen);
    	VIDEO_SetBlack(TRUE);
	WII_ChangeSquare(320, 240, 0, 0);
    	VIDEO_SetBlack(FALSE);

	/* remove any pending buttons */
	while (WPAD_ButtonsHeld(0))
	{
		WPAD_ScanPads();
		VIDEO_WaitVSync();
	}
}


char * LoadRom(char * path, char * file)
{
	Browsefiles(path, file);

	if (file[0] == '\0')
	{
		return file;
	}
	else
	{
		strncat(path, file, strlen(file));
		strncpy(file, path, strlen(path));
	}
	return file;
}


#include "Common.h"

