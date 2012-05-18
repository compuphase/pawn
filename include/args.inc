/* Script Arguments support functions
 *
 * (c) Copyright 2005-2011, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#pragma library Args

native argcount();
native bool: argindex(index, value[], maxlength = sizeof value, bool: pack = true);
native bool: argstr(index = 0, const option[] = "", value[] = "", maxlength = sizeof value, bool: pack = true);
native bool: argvalue(index = 0, const option[] = "", &value = cellmin);
