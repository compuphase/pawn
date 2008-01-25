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
 *
 *  Version: $Id: term_ga.c 3902 2008-01-23 17:40:01Z thiadmer $
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <graphapp.h>
#include "term_ga.h"

#define NUM_COLUMNS     80
#define NUM_LINES       30
#define KEYQUEUE_SIZE   32
#define BUFFERSIZE      2048

static App *app = NULL;
static Window *win = NULL;
static Font *font = NULL;
static TCHAR  *lines = NULL;
static unsigned long keyqueue[KEYQUEUE_SIZE];
static int keyq_start,keyq_end;
static int csrx = 0, csry = 0;
static int autowrap = 0;
static unsigned char attrib = 0x07;

static
void window_redraw(Window *w, Graphics *g)
{
  Rect r;
  Point p;
  int l, h;
  int length;
  char *buffer;

  assert(lines != NULL);
  /* Each wide char is encoded in at most 6 UTF8 characters. For the Base Plane,
   * 3 UTF8 characters per Unicode character is enough
   */
  buffer = malloc((6 * NUM_COLUMNS + 1) * sizeof(char));
  if (buffer != NULL) {
    r = get_window_area(w);

    set_rgb(g, rgb(240,240,240)); //??? attrib
    fill_rect(g, r);

    set_rgb(g, rgb(0,0,0));       //??? attrib
    set_font(g, font);
    set_text_direction(g, LR_TB);
    p = pt(0,0);
    h = font_height(font);
    for (l = 0; l < NUM_LINES; l++) {
      #if defined _UNICODE
        /* convert the line to UTF8 */
        int c;
        char *ptr;
        for (c = 0, ptr = buffer; c < NUM_COLUMNS; c++)
          amx_UTF8Put(ptr, &ptr, 6, lines + l * NUM_COLUMNS + c);
        *ptr = '\0';
        length = (int)(ptr - buffer);
      #else
        /* assume line is ASCII */
        memcpy(buffer, lines + l * NUM_COLUMNS, NUM_COLUMNS);
        buffer[NUM_COLUMNS] = '\0';
        length = NUM_COLUMNS;
      #endif

      /* draw the line */
      draw_utf8(g, p, buffer, length);
      p.y += h;
      if (p.y > r.height)
        break;
    } /* if */

    free(buffer);
  } /* if */
}

/* dx = in columns, dy = in lines */
void scroll_window(int dx, int dy)
{
  Graphics *g;
  Rect r;
  Point p;

  /* a negative value scrolls up */
  assert(lines != NULL);
  if (dy < 0) {
    assert(-dy < NUM_LINES);
    memmove(lines,lines-dy*NUM_COLUMNS,(NUM_LINES+dy)*NUM_COLUMNS*sizeof(TCHAR));
    memset(lines+(NUM_LINES+dy)*NUM_COLUMNS*sizeof(TCHAR), __T(' '), -dy*NUM_COLUMNS);
  } else if (dy > 0) {
    assert(dy < NUM_LINES);
    memmove(lines+dy*NUM_COLUMNS,lines,(NUM_LINES-dy)*NUM_COLUMNS*sizeof(TCHAR));
    memset(lines, __T(' '), dy*NUM_COLUMNS);
  } /* if */
  csry += dy;
  if (csry < 0)
    csry = 0;
  if (csry >= NUM_LINES)
    csry=NUM_LINES - 1;

  assert(font != NULL);
  dx *= font_width(font, "x", 1);
  dy *= font_height(font);

  g = get_window_graphics(win);
  r = get_window_area(win);
  p = pt(r.x + dx, r.y + dy);
  copy_rect(g, p, g, r);
  if (dy > 0) {
    /* moving window contents downwards */
    redraw_rect(win, rect(0,0,r.width,dy));
  } else if (dy < 0) {
    /* moving window contents upwards */
    redraw_rect(win, rect(0,r.height+dy,r.width,0-dy));
  } /* if */
  if (dx > 0) {
    /* moving window contents to the right */
    redraw_rect(win, rect(0,0,dx,r.height));
  } else if (dx < 0) {
    /* moving window contents to the left */
    redraw_rect(win, rect(r.width+dx,0,0-dx,r.height));
  } /* if */
  del_graphics(g);
}

static
void refresh_screen(int top, int bottom)
{
  Rect r;
  int h;

  if (top != bottom) {
    assert(win != NULL);
    r = get_window_area(win);
    assert(font != NULL);
    h = font_height(font);
    redraw_rect(win, rect(0, top * h, r.width, (bottom - top) * h));
  } /* if */

  //??? set to draw a caret
}

#if 0
static
void window_fkey_action(Window *w, unsigned long key)
{
}
#endif

static
void window_key_action(Window *w, unsigned long key)
{
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    /* translate the Enter key from '\n' (GraphApp convention) to '\r'
     * (AMXCONS convention when running under Microsoft Windows)
     */
    if (key == '\n')
      key = '\r';
  #endif

  if ((keyq_end+1)%KEYQUEUE_SIZE==keyq_start) {
    beep(app);
  } else {
    keyqueue[keyq_end]=key;
    keyq_end=(keyq_end+1)%KEYQUEUE_SIZE;
  } /* if */
}

static
void window_close(Window *w)
{
  /* we cannot delete the window on this notification (GraphApp refers to it
   * on return from the function), so we just mark the global variable
   * as invalid
   */
  win = NULL;
}

int createconsole(int argc, char *argv[])
{
  if (win != NULL)
    return 1;

  if (app != NULL)      /* delete existing partial data structures */
    deleteconsole();

  lines = malloc(NUM_LINES*NUM_COLUMNS*sizeof(TCHAR));
  if (lines == NULL)
    return 0;
  memset(lines, __T(' '), NUM_LINES * NUM_COLUMNS);

  app = new_app(argc, argv);
  if (app == NULL) {
    deleteconsole();
    return 0;
  } /* if */

  font = new_font(app, "unifont", PLAIN | PORTABLE_FONT, 16);
  if (font == NULL) {
    deleteconsole();
    return 0;
  } /* if */

  win = new_window(app,
                   rect(0,0,
                        NUM_COLUMNS*font_width(font,"x",1),
                        NUM_LINES*font_height(font)),
                   "Pawn console",
                   TITLEBAR|CLOSEBOX|MAXIMIZE|MINIMIZE|CENTRED);
  on_window_redraw(win, window_redraw);
  on_window_close (win, window_close);
  on_window_key_down(win, window_key_action);     /* normal keys (including CR) */
  show_window(win);

  /* handle any pending events */
  while (do_event(app))
    /* nothing */;

  csrx = 0;
  csry = 0;
  autowrap = 0;
  attrib = 0x07;

  return 1;
}

int deleteconsole(void)
{
  if (font != NULL) {
    del_font(font);
    font = NULL;
  } /* if */
  if (app != NULL) {
    del_app(app);
    app = NULL;
  } /* if */
  if (lines != NULL) {
    free(lines);
    lines = NULL;
  } /* if */
  win = NULL;
  return 1;
}

int amx_putstr(const TCHAR *format)
{
  if (createconsole(0, NULL)) {
    int pos, i;

    pos=csry * NUM_COLUMNS + csrx;
    assert(lines!=NULL);
    for (i=0; string[i]!=__T('\0'); i++) {
      if (csry<NUM_LINES && csrx<NUM_COLUMNS) {
        if (string[i]==__T('\r')) {
          csrx=0;
          pos=csry * NUM_COLUMNS + csrx;
        } else if (string[i]==__T('\n')) {
          csrx=0;
          csry++;
          if (csry>=NUM_LINES)
            scroll_window(0, -1);
          pos=csry * NUM_COLUMNS + csrx;
        } else if (string[i]==__T('\b')) {
          if (csrx>0) {
            csrx--;
            pos--;
            lines[pos]=__T(' ');
            //??? lines[pos+1]=attrib;
          } /* if */
        } else {
          lines[pos]=string[i];
          //??? lines[pos+1]=attrib;
          pos++;
          csrx++;
          if (csrx>=NUM_COLUMNS && autowrap) {
            csrx=0;
            csry++;
            if (csry>=NUM_LINES)
              scroll_window(0, -1);
            pos=csry * NUM_COLUMNS + csrx;
          } /* if */
        } /* if */
      } /* if */
    } /* for */
    refresh_screen(csry,csry+1);
  } /* if */
  return 0;
}

int amx_printf(const TCHAR *format,...)
{
  int cnt;
  TCHAR buffer[BUFFERSIZE];
  va_list argptr;

  va_start(argptr,format);
  cnt=_vstprintf(buffer,format,argptr);
  va_end(argptr);
  amx_putstr(buffer);
  return cnt;
}

int amx_putchar(int c)
{
  if (createconsole(0, NULL)) {
    if (csry<NUM_LINES && csrx<NUM_COLUMNS) {
      int pos=csry*NUM_COLUMNS+csrx;
      assert(lines!=NULL);
      if (c==__T('\r')) {
        csrx=0;
      } else if (c==__T('\n')) {
        csrx=0;
        csry++;
        if (csry>=NUM_LINES)
          scroll_window(0, -1);
      } else if (c==__T('\b')) {
        if (csrx>0) {
          csrx--;
          pos--;
          lines[pos]=__T(' ');
          //??? lines[pos+1]=attrib;
        } /* if */
      } else {
        lines[pos]=(TCHAR)c;
        //??? lines[pos+1]=attrib;
        csrx++;
        if (csrx>=NUM_COLUMNS && autowrap) {
          csrx=0;
          csry++;
          if (csry>=NUM_LINES)
            scroll_window(0, -1);
        } /* if */
      } /* if */
      refresh_screen(csry,csry+1);
    } /* if */
  } /* if */
  return 1;
}

int amx_fflush(void)
{
  return 1;
}

int amx_kbhit(void)
{
  if (createconsole(0, NULL))
    return keyq_start!=keyq_end;
  return 0;
}

int amx_getch(void)
{
  int c=-1;

  if (createconsole(0, NULL)) {
    while (keyq_start==keyq_end && app!=NULL) {
      wait_event(app);
      do_event(app);
    } /* while */
    c=(int)keyqueue[keyq_start];
    keyq_start=(keyq_start+1)%KEYQUEUE_SIZE;
  } /* if */
  return c;
}

TCHAR *amx_gets(TCHAR *string,int size)
{
  int c=-1,num=0;

  if (createconsole(0, NULL)) {
    while (num+1<size && !(c==__T('\r') || c==__T('\n'))) {
      c=amx_getch();
      if (c<0)
        break;
      if (c==__T('\r'))
        c=__T('\n');    /* translate carriage return to newline */
      if (c==__T('\b')) {
        if (num>0)
          string[--num]=__T(' ');
      } else {
        string[num++]=(TCHAR)c;
      } /* if */
      amx_putchar(c);   /* echo the character read */
    } /* while */
    if (num<size)
      string[num]=__T('\0');
    return string;
  } /* if */

  return 0;
}

int amx_termctl(int cmd,int value)
{
  switch (cmd) {
  case 0:
    return 1;           /* simple "is terminal support available" check */

  case 1:
    if (createconsole(0, NULL)) {
      autowrap=value;
      return 1;
    } /* if */
    return 0;

  case 2:
    return 0;

  case 3:
    return 0;

  case 4:
    while (app != NULL && win != NULL && do_event(app))
      /* nothing */;
    if (win == NULL)
      deleteconsole();
    return (win != NULL);

  default:
    return 0;
  } /* switch */
}

void amx_clrscr(void)
{
  if (createconsole(0, NULL)) {
    assert(lines != NULL);
    memset(lines, __T(' '), NUM_LINES * NUM_COLUMNS);
    csrx = csry = 0;
    refresh_screen(0, NUM_LINES);
  } /* if */
}

void amx_clreol(void)
{
  if (createconsole(0, NULL)) {
    int i;
    int size=NUM_COLUMNS-csrx;
    int pos=csry*NUM_COLUMNS+csrx;
    assert(lines!=NULL);
    for (i=0; i<size; i++) {
      lines[pos+i]=__T(' ');
      //??? lines[pos+i+1]=attrib;
    } /* for */
    refresh_screen(csry, csry + 1);
  } /* if */
}

void amx_gotoxy(int x,int y)
{
  if (createconsole(0, NULL)) {
    if (x>0 && x<=NUM_COLUMNS)
      csrx=x-1;
    if (y>0 && y<=NUM_LINES)
      csry=y-1;
    refresh_screen(0, 0); /* only to set the cursor at the correct location */
  } /* if */
}

void amx_wherexy(int *x,int *y)
{
  if (createconsole(0, NULL)) {
    if (x!=NULL)
      *x=csrx+1;
    if (y!=NULL)
      *y=csry+1;
  } /* if */
}

unsigned int amx_setattr(int foregr,int backgr,int highlight)
{
  int prev=0;
  if (createconsole(0, NULL)) {
    int f,b,h;

    f=attrib & 0x07;
    b=(attrib >> 4) & 0x0f;
    h=(attrib & 0x08) ? 1 : 0;
    prev=(b << 8) | f | (h << 15);
    if (foregr>=0 && foregr<8)
      f=foregr;
    if (backgr>=0 && backgr<8)
      b=backgr;
    if (highlight>=0)
      h=highlight!=0;
    attrib=(TCHAR)((b << 4) | f | (h << 3));
  } /* if */
  return prev;
}

void amx_console(int columns, int lines, int flags)
{
  createconsole(0, NULL); /* columns and lines are currently not supported */
}
