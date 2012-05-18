The "prun" files are a collection of various complete implementations of a
run-time (console mode interface) for the abstract machine of Pawn. The Pawn
manual develops these examples in several steps, but it does not contain
listings of the complete and compilable programs.

Many examples for compiling the abstract machine, for various compilers and
various options of the abstract machine, are given in the "Implementor's Guide"
for the Pawn toolkit. Below is a brief subset of those examples, using only
the ANSI core. If your compiler is not listed, please consult the
above-mentioned Implementor's Guide. In each of these examples, "prun1.c" should
be replaced by "prun2.c", "prun3.c" for the other examples.

   Borland C++ version 3.1 (16-bit)
        bcc prun1.c amx.c amxcore.c amxcons.c

   Microsoft Visual C/C++
        cl prun1.c amx.c amxcons.c amxcore.c

        (note: when running with warning level 4, option "-W4", Visual C/C++
        issues a few warnings for unused function arguments and changes of the
        default structure alignment)

   OpenWatcom C/C++
        wcl386 /l=nt prun1.c amx.c amxcore.c amxcons.c


prun1.c
        The smallest and simplest example. This program uses the functions
        from the file amxaux.c (a collection of functions that are not part of
        the AMX core, but that are occasionally useful or convenient). The
        other examples are modifications and variations on this program.

prun2.c
        This example adds a debug hook and a Ctrl-Break signal function (all
        in ANSI C) to be able to abort a script with Ctrl-C or Ctrl-Break. The
        required modifications come from the manual. It is up to you to
        replace the ANSI C "signal" handler by some other means to abort a
        run-away script.

        Note that the SIGINT signal is not very standardized, at least not
        on Win32 compilers.

prun3.c
        This example implements the virtual memory implementation, where a
        compiled script may take as much or as little memory that it requires.
        The technique by which it achieves this is OS-dependent; this example
        will only compile and run on any OS of the Win32 family.

        Depending on your compiler, you may need to define the constant STDECL
        on the compiler's command line (or the IDE options).

prun4.c
        The examples do far have only executed function main() in the compiled
        script. This example is a modification of prun1.c that accepts two
        more parameters: a function name and a parameter. The parameter is
        passed as a string to the specified function.

        This sample uses a modified "rot13" example script (one in which the
        "work horse" function is made public).

prun5.c
        Another variation on prun1.c that shows how to use the optional garbage
        collector. Please read the appendix on the garbage collector as well,
        because in the pursuit of creating a portable sample, I have "hooked"
        the garbage collector on the "debug hook" and this is actually a bad
        idea.

        This example also requires the "Better String library", and open source
        library for handling dynamic strings in a safe manner. The "Better
        String library" is made by Paul Hsieh, and it can be found, at the time
        of this writing, at http://bstring.sourceforge.net/.

prun_jit.c
        A version of prun1.c that sets up the JIT compiler to run the modules.
        This example does not set up a debug hook, because the JIT compiler
        does not support any debug hook.


logfile.cpp
        An example of creating a native function module in C++ rather than in
        C. Parts of this file are discussed in the Implementor's Guide.

power.c
        An example of a native function module in C. Parts of this file are
        discussed in the Implementor's Guide.