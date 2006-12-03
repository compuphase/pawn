/* Script Arguments support functions
 *
 * (c) Copyright 2005, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#if defined _args_included
  #endinput
#endif
#define _args_included
#pragma library Args

native argcount();
native bool: argindex(index, value[], maxlength = sizeof value, bool: pack = false);
native bool: argstr(index = 0, const option[] = "", value[] = "", maxlength = sizeof value, bool: pack = false);
native bool: argvalue(index = 0, const option[] = "", &value = cellmin);
