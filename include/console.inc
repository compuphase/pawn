/* Console input/output functions
 *
 * (c) Copyright 1998-2005, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#if defined _console_included
  #endinput
#endif
#define _console_included
#pragma library Console

enum
    {
    black,      /* colours as per the ANSI Escape sequences, ISO 6429 */
    red,
    green,
    yellow,
    blue,
    magenta,
    cyan,
    white,
    }

native getchar(echo=true);
native getstring(string[], maxlength=sizeof string, bool:pack=false);
native getvalue(base=10, term=0x0d, ...);    /* 0x0d == '\r' */
native print(const string[], foreground=-1, background=-1, highlight=-1);
native printf(const format[], {Float,Fixed,_}:...);

native console(columns, lines);
native clrscr();        /* also resets the cursor to (1,1) */
native clreol();
native gotoxy(x=1, y=1);
native wherexy(&x, &y);
native setattr(foreground=-1, background=-1, highlight=-1);

forward @keypressed(key);
