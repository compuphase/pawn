/* File input/output functions
 *
 * (c) Copyright 2004-2005, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#if defined _file_included
  #endinput
#endif
#define _file_included
#pragma library File

enum filemode
    {
    io_read,            /* file must exist */
    io_write,           /* creates a new file */
    io_readwrite,       /* opens an existing file, or creates a new file */
    io_append,          /* appends to file (write-only) */
    }

enum seek_whence
    {
    seek_start,
    seek_current,
    seek_end,
    }

const EOF = -1;

native File: fopen(const name[], filemode: mode = io_readwrite);
native bool: fclose(File: handle);
native File: ftemp();
native bool: fremove(const name[]);

native       fwrite(File: handle, const string[]);
native       fread(File: handle, string[], size = sizeof string, bool: pack = false);
native bool: fputchar(File: handle, value, bool: utf8 = true);
native       fgetchar(File: handle, bool: utf8 = true);
native       fblockwrite(File: handle, const buffer[], size = sizeof buffer);
native       fblockread(File: handle, buffer[], size = sizeof buffer);

native       fseek(File: handle, position = 0, seek_whence: whence = seek_start);
native       flength(File: handle);
native       fexist(const pattern[]);
native bool: fmatch(name[], const pattern[], index = 0, size = sizeof name);
