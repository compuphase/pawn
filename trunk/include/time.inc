/* Date/time functions
 *
 * (c) Copyright 2001-2011, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#pragma library Time

native gettime(&hour=0, &minute=0, &second=0);
native settime(hour=cellmin, minute=cellmin, second=cellmin);
native getdate(&year=0, &month=0, &day=0);
native setdate(year=cellmin, month=cellmin, day=cellmin);

native settimestamp(seconds1970);
native cvttimestamp(seconds1970, &year=0, &month=0, &day=0, &hour=0, &minute=0, &second=0);

native settimer(milliseconds, bool: singleshot=false);
native tickcount(&granularity=0);
native delay(milliseconds);

forward @timer();
