/* Core functions
 *
 * (c) Copyright 1998-2005, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#if defined _core_included
  #endinput
#endif
#define _core_included
#pragma library Core

native heapspace();

native funcidx(const name[]);

native numargs();
native getarg(arg, index=0);
native setarg(arg, index=0, value);

native tolower(c);
native toupper(c);
native swapchars(c);

native random(max);

native min(value1, value2);
native max(value1, value2);
native clamp(value, min=cellmin, max=cellmax);

native getproperty(id=0, const name[]="", value=cellmin, string[]="");
native setproperty(id=0, const name[]="", value=cellmin, const string[]="");
native deleteproperty(id=0, const name[]="", value=cellmin);
native existproperty(id=0, const name[]="", value=cellmin);

