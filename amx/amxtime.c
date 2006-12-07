/*  Date/time module for the Pawn Abstract Machine
 *
 *  Copyright (c) ITB CompuPhase, 2001-2006
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
 *  Version: $Id: amxtime.c 3649 2006-10-12 13:13:57Z thiadmer $
 */
#include <time.h>
#include <assert.h>
#include "amx.h"
#if defined __WIN32__ || defined _WIN32
  #include <windows.h>
  #include <mmsystem.h>
#endif

#define CELLMIN   (-1 << (8*sizeof(cell) - 1))

#if !defined CLOCKS_PER_SEC
  #define CLOCKS_PER_SEC CLK_TCK
#endif
#if defined __WIN32__ || defined _WIN32 || defined WIN32
  static int timerset = 0;
  /* timeGetTime() is more accurate on WindowsNT if timeBeginPeriod(1) is set */
  #define INIT_TIMER()    \
    if (!timerset) {      \
      timeBeginPeriod(1); \
      timerset=1;         \
    }
#else
  #define INIT_TIMER()
#endif
static unsigned long timestamp;
static unsigned long timelimit;
static int timerepeat;

static const unsigned char monthdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static int wrap(int value, int min, int max)
{
  if (value<min)
    value=max;
  else if (value>max)
    value=min;
  return value;
}

static unsigned long gettimestamp(void)
{
  unsigned long value;

  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    value=timeGetTime();        /* this value is already in milliseconds */
  #else
    value=(cell)clock();
    #if CLOCKS_PER_SEC!=1000
      /* convert to milliseconds */
      value=(cell)((1000L * (value+CLOCKS_PER_SEC/2)) / CLOCKS_PER_SEC);
    #endif
  #endif
  return value;
}

/* settime(hour, minute, second)
 * Always returns 0
 */
static cell AMX_NATIVE_CALL n_settime(AMX *amx, const cell *params)
{
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    SYSTEMTIME systim;

    GetLocalTime(&systim);
    if (params[1]!=CELLMIN)
      systim.wHour=(WORD)wrap((int)params[1],0,23);
    if (params[2]!=CELLMIN)
      systim.wMinute=(WORD)wrap((int)params[2],0,59);
    if (params[3]!=CELLMIN)
      systim.wSecond=(WORD)wrap((int)params[3],0,59);
    SetLocalTime(&systim);
  #else
    /* Linux/Unix (and some DOS compilers) have stime(); on Linux/Unix, you
     * must have "root" permission to call stime()
     */
    time_t sec1970;
    struct tm gtm;

    (void)amx;
    time(&sec1970);
    gtm=*localtime(&sec1970);
    if (params[1]!=CELLMIN)
      gtm.tm_hour=wrap((int)params[1],0,23);
    if (params[2]!=CELLMIN)
      gtm.tm_min=wrap((int)params[2],0,59);
    if (params[3]!=CELLMIN)
      gtm.tm_sec=wrap((int)params[3],0,59);
    sec1970=mktime(&gtm);
    stime(&sec1970);
  #endif
  (void)amx;
  return 0;
}

/* gettime(&hour, &minute, &second)
 * The return value is the number of seconds since 1 January 1970 (Unix system
 * time).
 */
static cell AMX_NATIVE_CALL n_gettime(AMX *amx, const cell *params)
{
  time_t sec1970;
  struct tm gtm;
  cell *cptr;

  assert(params[0]==(int)(3*sizeof(cell)));

  time(&sec1970);

  /* on DOS/Windows, the timezone is usually not set for the C run-time
   * library; in that case gmtime() and localtime() return the same value
   */
  gtm=*localtime(&sec1970);
  if (amx_GetAddr(amx,params[1],&cptr)==AMX_ERR_NONE)
    *cptr=gtm.tm_hour;
  if (amx_GetAddr(amx,params[2],&cptr)==AMX_ERR_NONE)
    *cptr=gtm.tm_min;
  if (amx_GetAddr(amx,params[3],&cptr)==AMX_ERR_NONE)
    *cptr=gtm.tm_sec;

  /* the time() function returns the number of seconds since January 1 1970
   * in Universal Coordinated Time (the successor to Greenwich Mean Time)
   */
  return sec1970;
}

/* setdate(year, month, day)
 * Always returns 0
 */
static cell AMX_NATIVE_CALL n_setdate(AMX *amx, const cell *params)
{
  int maxday;

  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    SYSTEMTIME systim;

    GetLocalTime(&systim);
    if (params[1]!=CELLMIN)
      systim.wYear=(WORD)wrap((int)params[1],1970,2099);
    if (params[2]!=CELLMIN)
      systim.wMonth=(WORD)wrap((int)params[2],1,12);
    maxday=monthdays[systim.wMonth - 1];
    if (systim.wMonth==2 && ((systim.wYear % 4)==0 && ((systim.wYear % 100)!=0 || (systim.wYear % 400)==0)))
      maxday++;
    if (params[3]!=CELLMIN)
      systim.wDay=(WORD)wrap((int)params[3],1,maxday);
    SetLocalTime(&systim);
  #else
    /* Linux/Unix (and some DOS compilers) have stime(); on Linux/Unix, you
     * must have "root" permission to call stime()
     */
    time_t sec1970;
    struct tm gtm;

    (void)amx;
    time(&sec1970);
    gtm=*localtime(&sec1970);
    if (params[1]!=CELLMIN)
      gtm.tm_year=params[1]-1900;
    if (params[2]!=CELLMIN)
      gtm.tm_mon=params[2]-1;
    if (params[3]!=CELLMIN)
      gtm.tm_mday=params[3];
    sec1970=mktime(&gtm);
    stime(&sec1970);
  #endif
  (void)amx;
  return 0;
}

/* getdate(&year, &month, &day)
 * The return value is the number of days since the start of the year. January
 * 1 is day 1 of the year.
 */
static cell AMX_NATIVE_CALL n_getdate(AMX *amx, const cell *params)
{
  time_t sec1970;
  struct tm gtm;
  cell *cptr;

  assert(params[0]==(int)(3*sizeof(cell)));

  time(&sec1970);

  gtm=*localtime(&sec1970);
  if (amx_GetAddr(amx,params[1],&cptr)==AMX_ERR_NONE)
    *cptr=gtm.tm_year+1900;
  if (amx_GetAddr(amx,params[2],&cptr)==AMX_ERR_NONE)
    *cptr=gtm.tm_mon+1;
  if (amx_GetAddr(amx,params[3],&cptr)==AMX_ERR_NONE)
    *cptr=gtm.tm_mday;

  return gtm.tm_yday+1;
}

/* tickcount(&granularity)
 * Returns the number of milliseconds since start-up. For a 32-bit cell, this
 * count overflows after approximately 24 days of continuous operation.
 */
static cell AMX_NATIVE_CALL n_tickcount(AMX *amx, const cell *params)
{
  cell *cptr;

  assert(params[0]==(int)sizeof(cell));

  INIT_TIMER();
  #if defined __WIN32__ || defined _WIN32 || defined WIN32
    if (amx_GetAddr(amx,params[1],&cptr)==AMX_ERR_NONE)
      *cptr=1000;               /* granularity = 1 ms */
  #else
    if (amx_GetAddr(amx,params[1],&cptr)==AMX_ERR_NONE)
      *cptr=(cell)CLOCKS_PER_SEC;       /* in Unix/Linux, this is often 100 */
  #endif
  return gettimestamp() & 0x7fffffff;
}

/* delay(milliseconds)
 * Pauses for (at least) the requested number of milliseconds.
 */
static cell AMX_NATIVE_CALL n_delay(AMX *amx, const cell *params)
{
  unsigned long stamp;

  (void)amx;
  assert(params[0]==(int)sizeof(cell));

  INIT_TIMER();
  stamp=gettimestamp();
  while (gettimestamp()-stamp < (unsigned long)params[1])
    /* nothing */;
  return 0;
}

/* settimer(milliseconds, bool: singleshot = false)
 * Sets the delay until the @timer() callback is called. The timer may either
 * be single-shot or repetitive.
 */
static cell AMX_NATIVE_CALL n_settimer(AMX *amx, const cell *params)
{
  (void)amx;
  assert(params[0]==(int)(2*sizeof(cell)));
  timestamp=gettimestamp();
  timelimit=params[1];
  timerepeat=(int)(params[2]==0);
  return 0;
}

/* bool: gettimer(&milliseconds, bool: &singleshot = false)
 * Retrieves the timer set with settimer(); returns true if a timer
 * was set up, or false otherwise.
 */
static cell AMX_NATIVE_CALL n_gettimer(AMX *amx, const cell *params)
{
  cell *cptr;

  assert(params[0]==(int)(2*sizeof(cell)));
  if (amx_GetAddr(amx,params[1],&cptr)==AMX_ERR_NONE)
    *cptr=timelimit;
  if (amx_GetAddr(amx,params[1],&cptr)==AMX_ERR_NONE)
    *cptr=timerepeat;
  return timelimit>0;
}


#if !defined AMXTIME_NOIDLE
static AMX_IDLE PrevIdle = NULL;
static int idxTimer = -1;

static int AMXAPI amx_TimeIdle(AMX *amx, int AMXAPI Exec(AMX *, cell *, int))
{
  int err=0;

  assert(idxTimer >= 0);

  if (PrevIdle != NULL)
    PrevIdle(amx, Exec);

  if (timelimit>0 && (gettimestamp()-timestamp)>=timelimit) {
    if (timerepeat)
      timestamp+=timelimit;
    else
      timelimit=0;      /* do not repeat single-shot timer */
    err = Exec(amx, NULL, idxTimer);
    while (err == AMX_ERR_SLEEP)
      err = Exec(amx, NULL, AMX_EXEC_CONT);
  } /* if */

  return err;
}
#endif


#if defined __cplusplus
  extern "C"
#endif
const AMX_NATIVE_INFO time_Natives[] = {
  { "gettime",   n_gettime },
  { "settime",   n_settime },
  { "getdate",   n_getdate },
  { "setdate",   n_setdate },
  { "tickcount", n_tickcount },
  { "settimer",  n_settimer },
  { "gettimer",  n_gettimer },
  { "delay",     n_delay },
  { NULL, NULL }        /* terminator */
};

int AMXEXPORT amx_TimeInit(AMX *amx)
{
  #if !defined AMXTIME_NOIDLE
    /* see whether there is a @timer() function */
    if (amx_FindPublic(amx,"@timer",&idxTimer) == AMX_ERR_NONE) {
      if (amx_GetUserData(amx, AMX_USERTAG('I','d','l','e'), (void**)&PrevIdle) != AMX_ERR_NONE)
        PrevIdle = NULL;
      amx_SetUserData(amx, AMX_USERTAG('I','d','l','e'), amx_TimeIdle);
    } /* if */
  #endif

  return amx_Register(amx, time_Natives, -1);
}

int AMXEXPORT amx_TimeCleanup(AMX *amx)
{
  (void)amx;
  #if !defined AMXTIME_NOIDLE
    PrevIdle = NULL;
  #endif
  return AMX_ERR_NONE;
}
