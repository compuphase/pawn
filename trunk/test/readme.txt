Regression test for the Pawn toolkit
====================================
The REXX script and the set of source files are what I use to check the Pawn
compiler and (in a lesser extent) the Abstract Machine on a regular basis.
This test set is for version 4.0 of Pawn.

The script file assumes that it can access the Pawn compiler (PAWNCC.EXE) and
the simple run-time (PAWNRUN.EXE) in the "..\bin" directory relative from where
the "test.rexx" file resides. In my setup, I have a directory C:\Pawn\bin that
has the .EXE and .DLL files (under Microsoft Windows) files and C:\Pawn\Test
that has all the files in this ZIP file. Some of the tests require that the
PAWNRUN.EXE version includes floating point support and fixed point support. I
use a version of PAWNRUN that loads any required extension module as a
DLL/shared library (under Microsoft Windows, these are amxFloat.dll and
amxFixed.dll respectively). See the Pawn manual "Implementor's Guide" for
creating an appropriate PAWNRUN.EXE.

The REXX script gives you a prompt. At the prompt, you can type a test number,
or you can press just the Enter key (to get the next test). Typing "bye" at
the prompt aborts the test run. For every test, the script file says what you
should see (as output of the compiler and the run-time) and then executes the
commands. This means that the routine does not run unattended, you have to
verify the output yourself.

In order not to have too many tiny script files, I have used conditional
compilation quite heavily. This may make the script files rather hard to read.
Also, many of the tests are old, from the period before the semicolon became
optional. So many of these tests still end every statement with a semicolon.
I have not removed the semicolons, because that would change the tests. I
think that tests in a regression test suite should remain stable.

Best of luck,
Thiadmer Riemersma,
CompuPhase
(1/8/2011)
