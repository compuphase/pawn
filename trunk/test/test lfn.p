/* TEST LFN
 *
 * This file is for regression testing of the Pawn compiler and the abstract
 * machine. It contains many conditionally compiled segments that must certify
 * the correct behaviour of the compiler or the abstract machine. Note that
 * "correct behaviour" may mean that the compiler or the abstract machine
 * aborts with an error.
 */

/* NOTES:
 * 1. this file must have a filename containing at least one space character,
 *    see test 16 in test.rexx.
 */

#include <console>

main()
    {
    printf(''Hello from \''Test lfn\''\n'');
    }

