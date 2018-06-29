/** ColEm: portable Coleco emulator **************************/
/**                                                         **/
/**                          Help.h                         **/
/**                                                         **/
/** This file contains help information printed out by the  **/
/** main() routine when started with option "-help".        **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

char *HelpText[] =
{
  "\nUsage: funnymu [-option1 [-option2...]] [filename]",
  "\n[filename] = Name of the file to load as a cartridge [CART.ROM]",
#ifdef ZLIB
  "  This program will transparently uncompress singular GZIPped",
  "  and PKZIPped files.",
#endif
  "\n[-option]  =",
  "  -verbose <level>    - Select debugging messages [5]",
  "                        0 - Silent       1 - Startup messages",
  "                        2 - VDP          4 - Hmmm",
  "                                        16 - Sound",
  "  -hperiod <period>   - Number of CPU cycles per HBlank [215]",
  "  -vperiod <period>   - Number of CPU cycles per VBlank [60000]",
  "  -uperiod <period>   - Number of VBlanks per screen update [2]",
  "  -help               - Print this help page",
  "  -autoa/-noautoa     - Autofire/No autofire for button A [-noautoa]",
  "  -autob/-noautob     - Autofire/No autofire for button B [-noautob]",
#ifdef DEBUG
  "  -trap <address>     - Trap execution when PC reaches address [FFFFh]",
#endif
#ifdef SOUND
  "  -nosound            - Don't emulate sound [-nosound]",
#endif
#ifdef UNIX
#ifdef MITSHM
  "  -shm/-noshm         - Use/don't use MIT SHM extensions for X [-shm]",
#endif
  "  -saver/-nosaver     - Save/don't save CPU when inactive [-saver]",
#endif
#ifdef MSDOS
  "  -vsync/-novsync     - Sync/Don't sync screen updates [-novsync]",
#endif
  "\nKeyboard bindings:",
  "  [LSHIFT]   - Left controller Left FIRE button",
  "  [RSHIFT]   - Left controller Right FIRE button",
  "  [LCONTROL] - Right controller FIRE button",
  "  [TAB]      - Right controller FIRE button",
#ifdef DEBUG
  "  [F1]       - Go into the built-in debugger",
#endif
  "  [F2]       - Turn soundtrack log on/off",
#ifdef MSDOS
  "  [F3]       - Toggle joystick 1 on/off",
  "  [F4]       - Toggle joystick 2 on/off",
#ifdef GIFLIB
  "  [F5]       - Make a screen snapshot (SNAPxxxx.GIF)",
#endif
#endif
#ifdef SOUND
  "  [F12]      - Exit",
  "  [PGUP]     - Increase sound volume",
  "  [PGDOWN]   - Decrease sound volume",
#endif
  0
};
