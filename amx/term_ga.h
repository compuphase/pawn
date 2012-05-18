/*  Simple terminal using GraphApp
 *
 *  Copyright (c) ITB CompuPhase, 2004-2011
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not
 *  use this file except in compliance with the License. You may obtain a copy
 *  of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 *
 *  Version: $Id: term_ga.h 4523 2011-06-21 15:03:47Z thiadmer $
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
