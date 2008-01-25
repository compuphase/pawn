/*  Simple terminal using GraphApp
 *
 *  Copyright (c) ITB CompuPhase, 2004-2008
 *
 *  This software is provided "as-is", without any express or implied warranty.
 *  In no event will the authors be held liable for any damages arising from
 *  the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1.  The origin of this software must not be misrepresented; you must not
 *      claim that you wrote the original software. If you use this software in
 *      a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *  2.  Altered source versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software.
 *  3.  This notice may not be removed or altered from any source distribution.
 *  This file may be freely used. No warranties of any kind.
 *
 *  Version: $Id: term_ga.h 3902 2008-01-23 17:40:01Z thiadmer $
 */

#if !defined TERMGA_H_INCLUDED
#define TERMGA_H_INCLUDED

#if defined _UNICODE || defined __UNICODE__ || defined UNICODE
# if !defined UNICODE   /* for Windows */
#   define UNICODE
# endif
# if !defined _UNICODE  /* for C library */
#   define _UNICODE
# endif
#endif

#if defined _UNICODE
# include <tchar.h>
#elif !defined __T
  typedef char          TCHAR;
# define __T(string)    string
# define _tcschr        strchr
# define _tcscpy        strcpy
# define _tcsdup        strdup
# define _tcslen        strlen
# define _stprintf      sprintf
# define _vstprintf     vsprintf
#endif

#ifdef __cplusplus
  extern "C" {
#endif

int createconsole(int argc, char *argv[]);
int deleteconsole(void);

int      amx_printf(const TCHAR*,...);
int      amx_putstr(const TCHAR*);
int      amx_putchar(int);
int      amx_fflush(void);
int      amx_kbhit(void);
int      amx_getch(void);
TCHAR*   amx_gets(TCHAR*,int);
int      amx_termctl(int,int);
void     amx_clrscr(void);
void     amx_clreol(void);
void     amx_gotoxy(int x,int y);
void     amx_wherexy(int *x,int *y);
unsigned amx_setattr(int foregr,int backgr,int highlight);
void     amx_console(int columns, int lines, int flags);

#ifdef __cplusplus
  }
#endif

#endif /* TERMGA_H_INCLUDED */
