/* Process control and Foreign Function Interface (calling functions
 * in DLLs or shared libraries)
 *
 * (c) Copyright 2006-2011, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#pragma library Process

native libcall(const libname[], const funcname[], const typestring[], ...);
native bool: libfree(const libname[]="");

native PID:  procexec(const progname[]);
native       procwait(PID:pid);
native bool: procwrite(const line[], bool:appendlf=false);
native bool: procread(line[], size=sizeof line, bool:striplf=false, bool:packed = true);
