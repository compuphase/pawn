/* File input/output functions
 *
 * (c) Copyright 2004-2011, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#pragma library File

const filemode:
    {
    io_read = 0,        /* file must exist */
    io_write,           /* creates a new file */
    io_readwrite,       /* opens an existing file, or creates a new file */
    io_append,          /* appends to file (write-only) */
    }

const seek_whence:
    {
    seek_start = 0,
    seek_current,
    seek_end,
    }

const EOF = -1;

native File: fopen(const name[], filemode: mode = io_readwrite);
native bool: fclose(File: handle);
native File: ftemp();
native bool: fremove(const name[]);
native bool: fcopy(const source[], const target[]);
native bool: frename(const oldname[], const newname[]);
native bool: fcreatedir(const name[]);

native       fwrite(File: handle, const string[]);
native       fread(File: handle, string[], size = sizeof string, bool: pack = true);
native bool: fputchar(File: handle, value, bool: utf8 = true);
native       fgetchar(File: handle, bool: utf8 = true);
native       fblockwrite(File: handle, const buffer[], size = sizeof buffer);
native       fblockread(File: handle, buffer[], size = sizeof buffer);

native       fseek(File: handle, position = 0, seek_whence: whence = seek_start);
native       flength(File: handle);
native       fexist(const pattern[]);
native bool: fmatch(name[], const pattern[], index = 0, size = sizeof name);
native bool: fstat(name[], &size = 0, &timestamp = 0, &mode = 0, &inode = 0);
native bool: fattrib(const name[], timestamp=0, attrib=0x0f);
native       filecrc(const name[]);

native       readcfg(const filename[]=``'', const section[]=``'', const key[], value[], size=sizeof value, const defvalue[]=``'', bool:pack=true);
native       readcfgvalue(const filename[]=``'', const section[]=``'', const key[], defvalue=0);
native bool: writecfg(const filename[]=``'', const section[]=``'', const key[], const value[]);
native bool: writecfgvalue(const filename[]=``'', const section[]=``'', const key[], value);
native bool: deletecfg(const filename[]=``'', const section[]=``'', const key[]=``'');
