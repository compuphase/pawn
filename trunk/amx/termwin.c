/*  Simple terminal for Microsoft Windows
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
 *  Version: $Id: termwin.c 3902 2008-01-23 17:40:01Z thiadmer $
 */

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
#else
# include <string.h>
#endif

#include <windows.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#if defined __WIN32__ || defined _WIN32 || defined WIN32
  #include <process.h>
#endif
#include "termwin.h"

#define DEFWINLINES     30
#define DEFBUFFERLINES  DEFWINLINES
#define DEFCOLUMNS      80
#define DEFFONTHEIGHT   14
#define KEYQUEUE_SIZE   32
#define BUFFERSIZE      2048

typedef struct tagCONSOLE {
  struct tagCONSOLE *next;
  HWND hwnd;
  TCHAR *buffer;                /* text buffer */
  int lines,columns;
  int winlines;                 /* default # lines in the window */
  TCHAR attrib;                 /* text attribute */
  int csrx,csry;                /* cursor position */
  short keyqueue[KEYQUEUE_SIZE];
  int keyq_start,keyq_end;
  BOOL autowrap;
  BOOL boldfont;
  HFONT hfont;
  int cwidth,cheight;           /* character width and height */
} CONSOLE;

static CONSOLE consoleroot={ NULL };
static HWND activeconsole=NULL;

#if defined __WIN32__ || defined _WIN32 || defined WIN32
  #define EXPORT
#else
  #define EXPORT  _export
#endif

long CALLBACK EXPORT ConsoleFunc(HWND hwnd,unsigned message,WPARAM wParam,
                                  LPARAM lParam);

static BOOL InitWindowClass(HINSTANCE hinst)
{
  static BOOL initok=FALSE;
  if (!initok) {
    WNDCLASS wc;
    memset(&wc,0,sizeof wc);
    wc.style=CS_GLOBALCLASS;
    wc.lpfnWndProc=(WNDPROC)ConsoleFunc;
    wc.hInstance=hinst;
    wc.hCursor=LoadCursor(NULL, IDC_ARROW);
    wc.hIcon=LoadIcon(GetModuleHandle(NULL), __T("AppIcon"));
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName=__T("TermWin:Console");
    initok=RegisterClass(&wc);
  } /* if */
  return initok;
}

static CONSOLE *Hwnd2Console(HWND hwnd)
{
  CONSOLE *con;
  for (con=consoleroot.next; con!=NULL && con->hwnd!=hwnd; con=con->next)
    /* nothing */;
  return con;
}

static void DoDeleteConsole(CONSOLE *con)
{
  CONSOLE *prev;

  assert(con!=NULL);

  /* unlink first, to avoid a recursive delete when destroying the window */
  for (prev=&consoleroot; prev->next!=NULL && prev->next!=con; prev=prev->next)
    /* nothing */;
  if (prev->next==con)
    prev->next=con->next;

  /* free memory and resources */
  if (con->hwnd!=NULL && IsWindow(con->hwnd))
    DestroyWindow(con->hwnd);
  if (con->hfont!=0)
    DeleteObject(con->hfont);
  if (con->buffer!=NULL)
    free(con->buffer);
  free(con);
}

HWND CreateConsole(HINSTANCE hinst,HWND hwndParent,int columns,int lines,int bufferlines,int fontsize,
                   DWORD style)
{
  CONSOLE *con, *prev;

  /* allocate a structure to hold the information */
  con=malloc(sizeof(CONSOLE));
  if (con==NULL)
    return NULL;
  memset(con,0,sizeof(CONSOLE));

  /* link to the list */
  for (prev=&consoleroot; prev->next!=NULL; prev=prev->next)
    /* nothing */;
  prev->next=con;

  /* fill in information */
  if (lines>=bufferlines)
    lines=bufferlines;
  con->lines=bufferlines;
  con->columns=columns;
  con->winlines=lines;
  con->cheight=fontsize;
  con->attrib=0xf0;             /* black on white */
  con->autowrap=TRUE;
  con->buffer=malloc(bufferlines*columns*2*sizeof(TCHAR));
  /* clear in the console
   * just like the DOS console buffer, our console consists of character/
   * attribute pairs.
   */
  if (con->buffer!=NULL) {
    int i;
    int size=bufferlines*columns*2;
    for (i=0; i<size; i+=2) {
      con->buffer[i]=__T(' ');
      con->buffer[i+1]=con->attrib;
    } /* for */
  } /* if */

  if (style==0)
    style=WS_OVERLAPPEDWINDOW;
  if (hinst==NULL)
    hinst=GetModuleHandle(NULL);

  /* create the window */
  InitWindowClass(hinst);
  con->hwnd=CreateWindow(__T("TermWin:Console"),__T("Pawn console"),style,
                         CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
                         hwndParent,NULL,hinst,NULL);

  /* check whether all is ok */
  if (IsWindow(con->hwnd) && con->buffer!=NULL) {
    ShowWindow(con->hwnd,SW_SHOWNORMAL);
    return con->hwnd;
  } /* if */
  /* when we arrive here, something was initialized correctly and something else was not */
  DoDeleteConsole(con);
  return NULL;
}

BOOL DeleteConsole(HWND hconsole)
{
  CONSOLE *con=Hwnd2Console(hconsole);
  if (con!=NULL)
    DoDeleteConsole(con);
  return con!=NULL;
}

BOOL SetActiveConsole(HWND hconsole)
{
  CONSOLE *con=Hwnd2Console(hconsole);
  if (con!=NULL)
    activeconsole=con->hwnd;
  return con!=NULL;
}

HWND GetConsoleByIndex(int index)
{
  CONSOLE *con;

  for (con=consoleroot.next; con!=NULL && index>0; con=con->next)
    index--;
  if (con!=NULL)
    return con->hwnd;
  return NULL;
}

static CONSOLE *ActiveConsole(void)
{
  CONSOLE *con;

  for (con=consoleroot.next; con!=NULL && con->hwnd!=activeconsole; con=con->next)
    /* nothing */;
  if (con==NULL) {      /* active console "disappeared", switch to first console */
    /* create a console if there are none left */
    if (consoleroot.next==NULL)
      CreateConsole(NULL,HWND_DESKTOP,DEFCOLUMNS,DEFWINLINES,DEFBUFFERLINES,DEFFONTHEIGHT,0);
    con=consoleroot.next;
    activeconsole= (con!=NULL) ? con->hwnd : NULL;
  } /* if */
  /* if "con" still is NULL here, then the following holds:
   * 1. the "active console" (if there was one) disappeared
   * 2. there are no consoles left at all
   * 3. a new console could not be created
   */
  return con;
}

#if defined __WIN32__ || defined _WIN32 || defined WIN32
static void ConsoleThreadProc(void *dummy)
{
  MSG msg;

  (void)dummy;

  /* initialize the screen */
  amx_console(DEFCOLUMNS,DEFWINLINES,0);

  /* message loop to process user input */
  while (amx_termctl(4,0)) {
    if (GetMessage(&msg,NULL,0,0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } /* if */
  } /* while */
}

BOOL CreateConsoleThread(void)
{
  AllocConsole();
  if ((HANDLE)_beginthread(ConsoleThreadProc, 0, NULL)==INVALID_HANDLE_VALUE)
    return FALSE;
  return TRUE;
}
#endif

static void SetConsoleFont(CONSOLE *con,int height)
{
static TCHAR *ConsoleFonts[] = {
  #if defined _UNICODE
    __T("Monotype.com"),
  #endif
    __T("Andante"),
    __T("ProggyClean"),
    __T("Bitstream Vera Sans Mono"),
    __T("Lucida Console"),
    __T("Monaco"),
    __T("Andale Mono"),
    __T("Courier New"),
    __T("FixedSys"),
    NULL };
  HDC hdc;
  SIZE size;
  HFONT hfontOrg;
  int weight,index;

  assert(con!=NULL);

  /* remove the existing font (if any) */
  if (con->hfont!=0)
    DeleteObject(con->hfont);

  /* make a new font */
  weight= con->boldfont ? FW_BOLD : FW_NORMAL;
  index=0;
  do {
    con->hfont=CreateFont(height, 0, 0, 0, weight, FALSE, 0, 0, ANSI_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
                          FIXED_PITCH|FF_DONTCARE, ConsoleFonts[index]);
  } while (con->hfont==NULL && ConsoleFonts[++index]!=NULL);
  /* get character size */
  hdc=GetDC(con->hwnd);
  hfontOrg=SelectObject(hdc,con->hfont);
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    GetTextExtentPoint32(hdc,__T("x"),1,&size);
  #else
    GetTextExtentPoint(hdc,__T("x"),1,&size);
  #endif
  SelectObject(hdc,hfontOrg);
  ReleaseDC(con->hwnd,hdc);
  con->cwidth=(int)size.cx;
  con->cheight=(int)size.cy;
}

static void ClampToScreen(RECT *rc)
{
  int cx=GetSystemMetrics(SM_CXSCREEN);
  int cy=GetSystemMetrics(SM_CYSCREEN);

  if (rc->left<0)
    OffsetRect(rc,-rc->left,0);
  if (rc->top<0)
    OffsetRect(rc,0,-rc->top);
  if (rc->right>cx) {
    /* first try to move left */
    OffsetRect(rc,cx-rc->right,0);
    /* also check not to exceed the left edge */
    if (rc->left<0)
      rc->left=0;
  } /* if */
  if (rc->bottom>cy) {
    /* first try to move up */
    OffsetRect(rc,0,cy-rc->bottom);
    /* also check not to exceed the top edge */
    if (rc->top<0)
      rc->top=0;
  } /* if */
}

static void RefreshCaretPos(CONSOLE *con)
{
  assert(con!=NULL);
  assert(IsWindow(con->hwnd));
  //??? should scroll so that the caret is visible
  SetCaretPos(con->cwidth*con->csrx-GetScrollPos(con->hwnd,SB_HORZ),
              con->cheight*(con->csry+1)-2-GetScrollPos(con->hwnd,SB_VERT));
}

static void RefreshScreen(CONSOLE *con,int startline,int endline)
{
  RECT rect;
  assert(con!=NULL);
  assert(IsWindow(con->hwnd));
  GetClientRect(con->hwnd,&rect);
  rect.top=startline*con->cheight;
  rect.bottom=endline*con->cheight;
  InvalidateRect(con->hwnd,NULL/*&rect*/,FALSE);
  RefreshCaretPos(con);
}

static void ScrollScreen(CONSOLE *con,int dx,int dy)
{
  int x,y,linesize,pos;

  assert(con!=NULL);
  assert(con->buffer!=NULL);

  /* vertical scrolling */
  if (dy!=0) {
    linesize=con->columns*2;
    /* a positive value scrolls up */
    if (dy>=0) {
      for (y=0; y<con->lines-1; y++)
        memcpy(con->buffer+y*linesize,con->buffer+(y+1)*linesize,linesize*sizeof(TCHAR));
      pos=((con->lines-1)*con->columns)*2;
      for (x=0; x<con->columns*2; x+=2) {
        con->buffer[pos+x]=__T(' ');
        con->buffer[pos+x+1]=con->attrib;
      } /* for */
    } else {
      for (y=con->lines-1; y>0; y++)
        memcpy(con->buffer+y*linesize,con->buffer+(y-1)*linesize,linesize*sizeof(TCHAR));
      pos=((con->lines-1)*con->columns)*2;
      for (x=0; x<con->columns*2; x+=2) {
        con->buffer[pos+x]=__T(' ');
        con->buffer[pos+x+1]=con->attrib;
      } /* for */
    } /* if */
    con->csry-=dy;
    if (con->csry<0)
      con->csry=0;
    if (con->csry>=con->lines)
      con->csry=con->lines-1;
  } /* if */

  /* horizontal scrolling */
  /* ??? to be implemented */

  RefreshScreen(con,0,con->lines);
}

static void SetTextAttribute(HDC hdc,TCHAR attrib)
{
  int fore=attrib & 0x07;
  int back=(attrib >> 4) & 0x0f;
  int highlight=(attrib & 0x08) ? 1 : 0;

  switch (fore) {
  case 0:   /* black */
    SetTextColor(hdc,highlight ? RGB(128,128,128) : RGB(0,0,0));
    break;
  case 1:   /* red */
    SetTextColor(hdc,highlight ? RGB(255,0,0) : RGB(128,0,0));
    break;
  case 2:   /* green */
    SetTextColor(hdc,highlight ? RGB(0,255,0) : RGB(0,128,0));
    break;
  case 3:   /* yellow */
    SetTextColor(hdc,highlight ? RGB(255,255,0) : RGB(128,128,0));
    break;
  case 4:   /* blue */
    SetTextColor(hdc,highlight ? RGB(0,0,255) : RGB(0,0,128));
    break;
  case 5:   /* magenta */
    SetTextColor(hdc,highlight ? RGB(255,0,255) : RGB(128,0,128));
    break;
  case 6:   /* cyan */
    SetTextColor(hdc,highlight ? RGB(0,255,255) : RGB(0,128,128));
    break;
  case 7:   /* white */
    SetTextColor(hdc,highlight ? RGB(255,255,255) : RGB(192,192,192));
    break;
  } /* switch */

  switch (back) {
  case 0:   /* black */
    SetBkColor(hdc,RGB(0,0,0));
    break;
  case 1:   /* red */
    SetBkColor(hdc,RGB(128,0,0));
    break;
  case 2:   /* green */
    SetBkColor(hdc,RGB(0,128,0));
    break;
  case 3:   /* yellow */
    SetBkColor(hdc,RGB(128,128,0));
    break;
  case 4:   /* blue */
    SetBkColor(hdc,RGB(0,0,128));
    break;
  case 5:   /* magenta */
    SetBkColor(hdc,RGB(128,0,128));
    break;
  case 6:   /* cyan */
    SetBkColor(hdc,RGB(0,128,128));
    break;
  case 7:   /* white */
    SetBkColor(hdc,RGB(192,192,192));
    break;
  } /* switch */
}

long CALLBACK EXPORT ConsoleFunc(HWND hwnd,unsigned message,WPARAM wParam,
                                 LPARAM lParam)
{
  CONSOLE *con;
  PAINTSTRUCT ps;
  RECT rect;

  switch (message) {
  case WM_CHAR:
    if ((con=Hwnd2Console(hwnd))!=NULL) {
      /* store in a key queue */
      if ((con->keyq_end+1)%KEYQUEUE_SIZE==con->keyq_start) {
        MessageBeep(MB_OK);
        break;
      } /* if */
      con->keyqueue[con->keyq_end]=(short)wParam;
      con->keyq_end=(con->keyq_end+1)%KEYQUEUE_SIZE;
    } /* if */
    break;

  case WM_CREATE:
    /* The "hwnd" member of the CONSOLE structure has not yet been set, which
     * means that Hwnd2Console() cannot work on the real "hwnd". There should
     * at every instant be only one CONSOLE structure with a NULL handle,
     * however.
     */
    if ((con=Hwnd2Console(NULL))!=NULL) {
      con->hwnd=hwnd;
      SetConsoleFont(con,con->cheight);
      GetWindowRect(hwnd, &rect);
      SetRect(&rect,rect.left,rect.top,
              rect.left+con->cwidth*con->columns,
              rect.top+con->cheight*con->winlines);
      AdjustWindowRect(&rect,GetWindowLong(hwnd,GWL_STYLE),FALSE);
      if (con->winlines<con->lines)
        rect.right+=GetSystemMetrics(SM_CXVSCROLL);
      ClampToScreen(&rect);
      SetWindowPos(hwnd,NULL,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,0);
    } /* if */
    break;

  case WM_DESTROY:
    if ((con=Hwnd2Console(hwnd))!=NULL)
      DoDeleteConsole(con);
    /* if there are no consoles left, abort the program */
    if (consoleroot.next==NULL)
      ExitProcess(0);
    break;

  case WM_GETMINMAXINFO:
    if ((con=Hwnd2Console(hwnd))!=NULL) {
      MINMAXINFO FAR *lpmmi=(MINMAXINFO FAR*)lParam;
      int rx,ry,hsize,vsize;
      GetClientRect(hwnd,&rect);
      rx= (rect.right < con->columns*con->cwidth) ? con->columns*con->cwidth-rect.right : 0;
      ry= (rect.bottom < con->lines*con->cheight) ? con->lines*con->cheight-rect.bottom : 0;
      hsize= (ry>0) ? GetSystemMetrics(SM_CXVSCROLL) : 0;
      vsize= (rx>0) ? GetSystemMetrics(SM_CYHSCROLL) : 0;
      SetRect(&rect,0,0,con->cwidth*con->columns+hsize,con->cheight*con->lines+vsize);
      AdjustWindowRect(&rect,GetWindowLong(hwnd,GWL_STYLE),FALSE);
      lpmmi->ptMaxTrackSize.x=rect.right-rect.left;
      lpmmi->ptMaxTrackSize.y=rect.bottom-rect.top;
      lpmmi->ptMaxSize=lpmmi->ptMaxTrackSize;
    } /* if */
    break;

  case WM_SYSKEYDOWN:
  case WM_KEYDOWN:
    if ((con=Hwnd2Console(hwnd))!=NULL) {
      TCHAR str[20];
      int i;
      str[0]=__T('\0');
      switch (LOWORD(wParam)) {
      case VK_F1:
      case VK_F2:
      case VK_F3:
      case VK_F4:
      case VK_F5:
      case VK_F6:
      case VK_F7:
      case VK_F8:
      case VK_F9:
      case VK_F10:
      case VK_F11:
      case VK_F12:
        if (LOWORD(wParam)<=VK_F5)
          _stprintf(str,__T("\033[%d~\n"),LOWORD(wParam)-VK_F1+11);
        else if (LOWORD(wParam)==VK_F10)
          _stprintf(str,__T("\033[%d~\n"),LOWORD(wParam)-VK_F6+17);
        else
          _stprintf(str,__T("\033[%d~\n"),LOWORD(wParam)-VK_F11+23);
        break;
      case VK_ADD:
      case VK_SUBTRACT:
        /* check Ctrl key */
        if ((GetKeyState(VK_CONTROL) & 0x8000)!=0) {
          POINT pt;
          int newheight=con->cheight;
          int oldheight=newheight;
          int incr= (LOWORD(wParam)==VK_SUBTRACT) ? -1 : 1;
          do {
            newheight+=incr;
            /* make a new font, re-create a caret and redraw everything */
            SetConsoleFont(con,newheight);
          } while (newheight>5 && (oldheight==con->cheight || con->hfont==NULL));
          if (con->hfont==NULL) /* reset to original on failure */
            SetConsoleFont(con,oldheight);
          GetClientRect(hwnd,&rect);
          DestroyCaret();
          CreateCaret(hwnd,NULL,con->cwidth,2);
          RefreshCaretPos(con);
          /* redraw the window */
          InvalidateRect(hwnd,NULL,TRUE);
          /* resize the window */
          SetRect(&rect,0,0,con->cwidth*con->columns,con->cheight*con->winlines);
          AdjustWindowRect(&rect,GetWindowLong(hwnd,GWL_STYLE),FALSE);
          pt.x=pt.y=0;
          ClientToScreen(hwnd,&pt);
          OffsetRect(&rect,pt.x,pt.y);
          ClampToScreen(&rect);
          SetWindowPos(hwnd,NULL,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,
                       SWP_NOZORDER);
        } /* if */
        break;
      case VK_UP:
        _tcscpy(str,__T("\033[A"));
        break;
      case VK_DOWN:
        _tcscpy(str,__T("\033[B"));
        break;
      case VK_RIGHT:
        _tcscpy(str,__T("\033[C"));
        break;
      case VK_LEFT:
        _tcscpy(str,__T("\033[D"));
        break;
      case VK_HOME:
        _tcscpy(str,__T("\033[1~"));
        break;
      case VK_END:
        _tcscpy(str,__T("\033[4~"));
        break;
      case VK_INSERT:
        _tcscpy(str,__T("\033[2~"));
        break;
      case VK_DELETE:
        _tcscpy(str,__T("\033[3~"));
        break;
      case VK_PRIOR:  /* PageUp */
        _tcscpy(str,__T("\033[5~"));
        break;
      case VK_NEXT:   /* PageDown */
        _tcscpy(str,__T("\033[6~"));
        break;
      default:
        return DefWindowProc(hwnd,message,wParam,lParam);
      } /* switch */
      for (i=0; str[i]!=__T('\0'); i++) {
        if ((con->keyq_end+1)%KEYQUEUE_SIZE!=con->keyq_start) {
          con->keyqueue[con->keyq_end]=(short)str[i];
          con->keyq_end=(con->keyq_end+1)%KEYQUEUE_SIZE;
        } /* if */
      } /* for */
    } /* if */
    break;

  case WM_KILLFOCUS:
    HideCaret(hwnd);
    DestroyCaret();
    break;
  case WM_SETFOCUS:
    if ((con=Hwnd2Console(hwnd))!=NULL) {
      CreateCaret(hwnd,NULL,con->cwidth,2);
      RefreshCaretPos(con);
      ShowCaret(hwnd);
    } /* if */
    break;

  case WM_LBUTTONDOWN:
    SetFocus(hwnd);
    break;

  case WM_PAINT:
    HideCaret(hwnd);
    BeginPaint(hwnd, &ps);
    if ((con=Hwnd2Console(hwnd))!=NULL && con->buffer!=NULL) {
      TCHAR *string;
      string=malloc(con->columns*sizeof(TCHAR));
      if (string!=NULL) {
        int l,c,bpos,start;
        TCHAR attr;
        HFONT hfontOrg;
        int scrollx=GetScrollPos(hwnd,SB_HORZ);
        int scrolly=GetScrollPos(hwnd,SB_VERT);
        GetClientRect(hwnd,&rect);
        hfontOrg=SelectObject(ps.hdc,con->hfont);
        SetBkMode(ps.hdc,OPAQUE);
        for (l=0; l<con->lines; l++) {
          bpos=l*con->columns*2;
          c=0;
          while (c<con->columns) {
            /* find stretches with the same attribute */
            attr=con->buffer[bpos+1];
            start=c;
            while (c<con->columns && con->buffer[bpos+1]==attr) {
              assert(c-start>=0);
              assert(c-start<con->columns);
              string[c-start]=con->buffer[bpos];
              c++;
              bpos+=2;
            } /* if */
            SetTextAttribute(ps.hdc,attr);
            TextOut(ps.hdc,start*con->cwidth-scrollx,l*con->cheight-scrolly,string,c-start);
          } /* while */
        } /* for */
        SelectObject(ps.hdc,hfontOrg);
        free(string);
      } /* if */
    } /* if */
    EndPaint(hwnd, &ps);
    ShowCaret(hwnd);
    break;

  case WM_SIZE:
    if ((con=Hwnd2Console(hwnd))!=NULL) {
      int rx,ry;
      /* add/remove/recalculate scroll bars */
      GetClientRect(hwnd,&rect);
      rx= (rect.right < con->columns*con->cwidth) ? con->columns*con->cwidth-rect.right : 0;
      ry= (rect.bottom < con->lines*con->cheight) ? con->lines*con->cheight-rect.bottom : 0;
      /* adjust scrolling position, if necessary */
      if (GetScrollPos(hwnd,SB_HORZ)>=rx) {
        SetScrollPos(hwnd,SB_HORZ,rx,FALSE);
        InvalidateRect(hwnd,NULL,FALSE);
      } /* if */
      if (GetScrollPos(hwnd,SB_VERT)>=ry) {
        SetScrollPos(hwnd,SB_VERT,ry,FALSE);
        InvalidateRect(hwnd,NULL,FALSE);
      } /* if */
      SetScrollRange(hwnd,SB_HORZ,0,rx,TRUE);
      SetScrollRange(hwnd,SB_VERT,0,ry,TRUE);
    } /* if */
    break;

  case WM_HSCROLL:
    if ((con=Hwnd2Console(hwnd))!=NULL) {
      int scrollx=GetScrollPos(hwnd,SB_HORZ);
      int oldpos=scrollx;
      int min,max;
      GetScrollRange(hwnd,SB_HORZ,&min,&max);
      switch (LOWORD(wParam)) {
      case SB_TOP:
        scrollx=min;
        break;
      case SB_BOTTOM:
        scrollx=max;
        break;
      case SB_LINELEFT:
        scrollx=(scrollx>min) ? scrollx-1 : min;
        break;
      case SB_LINERIGHT:
        scrollx=(scrollx<max) ? scrollx+1 : max;
        break;
      case SB_PAGELEFT:
        scrollx=(scrollx>min) ? scrollx-50 : min;
        break;
      case SB_PAGERIGHT:
        scrollx=(scrollx<max) ? scrollx+50 : max;
        break;
      case SB_THUMBTRACK:
        scrollx=(int)HIWORD(wParam);
        break;
      } /* switch */
      if (oldpos!=scrollx) {
        SetScrollPos(hwnd,SB_HORZ,scrollx,TRUE);
        InvalidateRect(hwnd,NULL,FALSE);
        RefreshCaretPos(con);
      } /* if */
    } /* if */
    break;
  case WM_VSCROLL:
    if ((con=Hwnd2Console(hwnd))!=NULL) {
      int scrolly=GetScrollPos(hwnd,SB_VERT);
      int oldpos=scrolly;
      int min,max;
      GetScrollRange(hwnd,SB_VERT,&min,&max);
      switch (LOWORD(wParam)) {
      case SB_TOP:
        scrolly=min;
        break;
      case SB_BOTTOM:
        scrolly=max;
        break;
      case SB_LINELEFT:
        scrolly=(scrolly>min) ? scrolly-1 : min;
        break;
      case SB_LINERIGHT:
        scrolly=(scrolly<max) ? scrolly+1 : max;
        break;
      case SB_PAGELEFT:
        scrolly=(scrolly>min) ? scrolly-50 : min;
        break;
      case SB_PAGERIGHT:
        scrolly=(scrolly<max) ? scrolly+50 : max;
        break;
      case SB_THUMBTRACK:
        scrolly=(int)HIWORD(wParam);
        break;
      } /* switch */
      if (oldpos!=scrolly) {
        SetScrollPos(hwnd,SB_VERT,scrolly,TRUE);
        InvalidateRect(hwnd,NULL,FALSE);
        RefreshCaretPos(con);
      } /* if */
    } /* if */
    break;

  default:
    return DefWindowProc(hwnd,message,wParam,lParam);
  } /* switch */
  return 0L;
}


int amx_putstr(const TCHAR *string)
{
  CONSOLE *con;
  if ((con=ActiveConsole())!=NULL) {
    int pos, i;
    int top=con->csry;

    pos=(con->csry*con->columns+con->csrx)*2;
    assert(con->buffer!=NULL);
    for (i=0; string[i]!=__T('\0'); i++) {
      if (con->csry<con->lines && con->csrx<con->columns) {
        if (string[i]==__T('\r')) {
          con->csrx=0;
          pos=(con->csry*con->columns+con->csrx)*2;
        } else if (string[i]==__T('\n')) {
          con->csrx=0;
          con->csry++;
          if (con->csry>=con->lines)
            ScrollScreen(con,0,1);
          pos=(con->csry*con->columns+con->csrx)*2;
        } else if (string[i]==__T('\b')) {
          if (con->csrx>0) {
            con->csrx--;
            pos-=2;
            con->buffer[pos]=__T(' ');
            con->buffer[pos+1]=con->attrib;
          } /* if */
        } else {
          con->buffer[pos]=string[i];
          con->buffer[pos+1]=con->attrib;
          pos+=2;
          con->csrx+=1;
          if (con->csrx>=con->columns && con->autowrap) {
            con->csrx=0;
            con->csry++;
            if (con->csry>=con->lines)
              ScrollScreen(con,0,1);
            pos=(con->csry*con->columns+con->csrx)*2;
          } /* if */
        } /* if */
      } /* if */
    } /* for */
    RefreshScreen(con,top,con->csry+1);
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
  CONSOLE *con;
  if ((con=ActiveConsole())!=NULL) {
    if (con->csry<con->lines && con->csrx<con->columns) {
      int pos=(con->csry*con->columns+con->csrx)*2;
      assert(con->buffer!=NULL);
      if (c==__T('\r')) {
        con->csrx=0;
      } else if (c==__T('\n')) {
        con->csrx=0;
        con->csry++;
        if (con->csry>=con->lines)
          ScrollScreen(con,0,1);
      } else if (c==__T('\b')) {
        if (con->csrx>0) {
          con->csrx--;
          pos-=2;
          con->buffer[pos]=__T(' ');
          con->buffer[pos+1]=con->attrib;
        } /* if */
      } else if (c==__T('\t')) {
        while (con->csrx % 8!=0 && con->csrx<con->columns) {
          con->buffer[pos]=' ';
          con->buffer[pos+1]=con->attrib;
          con->csrx+=1;
          if (con->csrx>=con->columns && con->autowrap) {
            con->csrx=0;
            con->csry++;
            if (con->csry>=con->lines)
              ScrollScreen(con,0,1);
          } /* if */
        } /* while */
      } else {
        con->buffer[pos]=(TCHAR)c;
        con->buffer[pos+1]=con->attrib;
        con->csrx+=1;
        if (con->csrx>=con->columns && con->autowrap) {
          con->csrx=0;
          con->csry++;
          if (con->csry>=con->lines)
            ScrollScreen(con,0,1);
        } /* if */
      } /* if */
      RefreshScreen(con,con->csry,con->csry+1);
    } /* if */
  } /* if */
  return 1;
}

int amx_fflush(void)
{
  CONSOLE *con;
  if ((con=ActiveConsole())!=NULL && IsWindow(con->hwnd))
    UpdateWindow(con->hwnd);
  return 1;
}

static void ProcessMessages(void)
{
  MSG msg;

  while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  } /* while */
}

int amx_kbhit(void)
{
  CONSOLE *con;

  ProcessMessages();
  if ((con=ActiveConsole())!=NULL)
    return con->keyq_start!=con->keyq_end;
  return 0;
}

int amx_getch(void)
{
  CONSOLE *con;
  int c=-1;

  if ((con=ActiveConsole())!=NULL) {
    while (con->keyq_start==con->keyq_end)
      ProcessMessages();
    c=con->keyqueue[con->keyq_start];
    con->keyq_start=(con->keyq_start+1)%KEYQUEUE_SIZE;
  } /* if */
  return c;
}

TCHAR *amx_gets(TCHAR *string,int size)
{
  int c=-1,num=0;

  if (ActiveConsole()!=NULL) {
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

  case 1: {
    CONSOLE *con;
    if ((con=ActiveConsole())!=NULL) {
      con->autowrap=(BOOL)value;
      return 1;
    } /* if */
    return 0;
  } /* case */

  case 2: {
    HWND hconsole=GetConsoleByIndex(value);
    while (hconsole==NULL) {
      CreateConsole(NULL,HWND_DESKTOP,DEFCOLUMNS,DEFWINLINES,DEFBUFFERLINES,DEFFONTHEIGHT,0);
      hconsole=GetConsoleByIndex(value);
    } /* while */
    return SetActiveConsole(hconsole);
  } /* case */

  case 3: {
    CONSOLE *con;
    if ((con=ActiveConsole())!=NULL) {
      con->boldfont=(BOOL)value;
      SetConsoleFont(con,con->cheight);
      return 1;
    } /* if */
    return 0;
  } /* case */

  case 4: {
    MSG msg;
    while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } /* while */
    return (GetConsoleByIndex(value)!=NULL);
  } /* case */

  default:
    return 0;
  } /* switch */
}

void amx_clrscr(void)
{
  CONSOLE *con;
  if ((con=ActiveConsole())!=NULL) {
    int i;
    int size=con->lines*con->columns*2;
    assert(con->buffer!=NULL);
    for (i=0; i<size; i+=2) {
      con->buffer[i]=__T(' ');
      con->buffer[i+1]=con->attrib;
    } /* for */
    con->csrx=con->csry=0;
    RefreshScreen(con,0,con->lines);
  } /* if */
}

void amx_clreol(void)
{
  CONSOLE *con;
  if ((con=ActiveConsole())!=NULL) {
    int i;
    int size=(con->columns-con->csrx)*2;
    int pos=(con->csry*con->columns+con->csrx)*2;
    assert(con->buffer!=NULL);
    for (i=0; i<size; i+=2) {
      con->buffer[pos+i]=__T(' ');
      con->buffer[pos+i+1]=con->attrib;
    } /* for */
    RefreshScreen(con,con->csry,con->csry+1);
  } /* if */
}

void amx_gotoxy(int x,int y)
{
  CONSOLE *con;
  if ((con=ActiveConsole())!=NULL) {
    if (x>0 && x<=con->columns)
      con->csrx=x-1;
    if (y>0 && y<=con->lines)
      con->csry=y-1;
    RefreshScreen(con,0,0);
  } /* if */
}

void amx_wherexy(int *x,int *y)
{
  CONSOLE *con;
  if ((con=ActiveConsole())!=NULL) {
    if (x!=NULL)
      *x=con->csrx+1;
    if (y!=NULL)
      *y=con->csry+1;
  } /* if */
}

unsigned int amx_setattr(int foregr,int backgr,int highlight)
{
  int prev=0;
  CONSOLE *con;
  if ((con=ActiveConsole())!=NULL) {
    int f,b,h;

    f=con->attrib & 0x07;
    b=(con->attrib >> 4) & 0x0f;
    h=(con->attrib & 0x08) ? 1 : 0;
    prev=(b << 8) | f | (h << 15);
    if (foregr>=0 && foregr<8)
      f=foregr;
    if (backgr>=0 && backgr<8)
      b=backgr;
    if (highlight>=0)
      h=highlight!=0;
    con->attrib=(TCHAR)((b << 4) | f | (h << 3));
  } /* if */
  return prev;
}

void amx_console(int columns, int lines, int flags)
{
  HWND hwnd=CreateConsole(NULL,HWND_DESKTOP,columns,lines,DEFBUFFERLINES,DEFFONTHEIGHT,0);
  SetActiveConsole(hwnd);
  (void)flags;
}
