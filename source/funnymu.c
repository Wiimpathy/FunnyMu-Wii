/** FunnyMu: portable Coleco emulator **************************/
/**                                                         **/
/**                          funnymu.c                      **/
/**                                                         **/
/** This file contains generic main() procedure statrting   **/
/** the emulation.                                          **/
/**                                                         **/
/** Copyright (C) Paul Hayter 2001                          **/
/**     Heavily based on ColEm by Marat Fayzullin           **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include "SDL.h"
#include "funny.h"
#include "Help.h"
#include <gccore.h>
#include <ogcsys.h>
#include <sdcard/wiisd_io.h>
#include "fat.h"
#include <dirent.h>
#include <sys/stat.h>
#include <wiiuse/wpad.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* Used to quit game with Home cf. funnysdl.c */
bool GameLoop;

char *Options[]=
{ 
  "verbose","hperiod","vperiod","uperiod","help",
  "autoa","noautoa","autob","noautob","logsnd",
  "trap","sound","nosound","shm","noshm","saver",
  "nosaver","vsync","novsync",0
};

extern int   UseSound;   /* Sound mode (#ifdef SOUND)           */
extern int   UseSHM;     /* Use SHM X extension (#ifdef MITSHM) */
extern int   SaveCPU;    /* Pause when inactive (#ifdef UNIX)   */
extern int   SyncScreen; /* Sync screen updates (#ifdef MSDOS)  */

extern void WII_VideoStart();
extern void WII_VideoStop();
extern int PauseAudio(int Switch);
extern void ResetAudio();
extern void __exception_setreload(int t);

// Plugin's paths
char *PluginDevice = NULL;
char *PluginPathGame;

char * rom_filename;
int mode=0;

char * LoadRom(char * path, char * file);

void menu_fic()
{
	iprintf(" B -pour relire --R Retour---\n");
}

void menu_rep()
{
	iprintf(" --- A -suite- R -Retour --- \n --- X -sous rep- ---\n");
}
void menu_finrep()
{
	iprintf(" --- R -Retour- X -sous rep ---\n");
}

/** main() ***************************************************/
/** This is a main() function used in Unix and MSDOS ports. **/
/** It parses command line arguments, sets emulation        **/
/** parameters, and passes control to the emulation itself. **/
/*************************************************************/
#undef main
int SDL_main(int argc,char *argv[]) {

#ifdef DEBUG
  CPU.Trap=0xFFFF;CPU.Trace=0;
#endif

#ifdef MSDOS
  Verbose=0;
#elif GEKKO
  Verbose=0;
#else
  Verbose=5;
#endif

	char GamesDir[FILENAME_MAX];
	char *CartName = malloc(256);
	memset(CartName, 0x00, 256);

	bool Autoboot = false;
  
	/* Copy the paths sent by the arguments or make a default path. */
	if(argc > 1)
	{
		Autoboot = true;
		PluginDevice = argv[1];
		strcpy(CartName, argv[2]);
	}
	else
	{
		sprintf(GamesDir, "/apps/funny/roms/");
	}

	__exception_setreload(8);

	fatInitDefault();

	bool FirstRun = true;
	GameLoop = true;

	while(1)
	{

		/* Initialize SDL just once */
		if(!InitMachine() && FirstRun)
		{ 
		return(1);
		}

		/*  Open file explorer */
		if(!Autoboot && FirstRun)
		{
			CartName = (char *)LoadRom(GamesDir, CartName);
			FirstRun = false;
		}

		/* Quit game and load/reload explorer */
		if(!GameLoop)
	    	{
			TrashFunny();
			TrashMachine();

			if(Autoboot)
			{
		  		memset(CartName, 0x00, 256);
		  		sprintf(GamesDir, PluginDevice);
			}
			else
			{
				memset(CartName, 0x00, 256);
				sprintf(GamesDir, "/apps/funny/roms/");
			}

			CartName = (char *)LoadRom(GamesDir, CartName);
			GameLoop = true;
		}

		while(GameLoop)
		{
	      		StartFunny(CartName, PluginDevice);
		}
	}

	SDL_Quit();
	TrashFunny();
	TrashMachine();
	return(0);
}
