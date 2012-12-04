/* String functions
 *
 * (c) Copyright 2005-2012, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#pragma library String

native strlen(const string[]);
native strpack(dest[], const source[], maxlength=sizeof dest);
native strunpack(dest[], const source[], maxlength=sizeof dest);
native strcopy(dest[], const source[], maxlength=sizeof dest);
native strcat(dest[], const source[], maxlength=sizeof dest);

native strmid(dest[], const source[], start=0, end=cellmax, maxlength=sizeof dest);
native bool: strins(string[], const substr[], index, maxlength=sizeof string);
native bool: strdel(string[], start, end);

native strcmp(const string1[], const string2[], bool:ignorecase=false, length=cellmax);
native strfind(const string[], const sub[], bool:ignorecase=false, index=0);

native strval(const string[], index=0);
native valstr(dest[], value, bool:pack=true);
native bool: ispacked(const string[]);

native strformat(dest[], size=sizeof dest, bool:pack=true, const format[], {Fixed,Float,_}:...);

native uudecode(dest[], const source[], maxlength=sizeof dest);
native uuencode(dest[], const source[], numbytes, maxlength=sizeof dest);
native memcpy(dest[], const source[], index=0, numbytes, maxlength=sizeof dest);

stock bool: strequal(const string1[], const string2[], bool:ignorecase=false, length=cellmax)
    return strcmp(string1, string2, ignorecase, length) == 0;

