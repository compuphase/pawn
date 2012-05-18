/* Rational number support
 * Loads either Float.inc or Fixed.inc, depending on the configuration
 *
 * (c) Copyright 2004-2011, ITB CompuPhase
 * This file is provided as is (no warranties).
 */

/* first try to include floating point support */
#if !defined NOFLOAT
  #tryinclude <float>
#endif
#if defined _inc_float || defined _inc_float_inc
  #define Rational:             Float:
  #define rval(%1)              strfloat(%1)
  #define rround(%1,%2)         floatround(%1,%2)
  #define rsqroot(%1)           floatsqroot(%1)
  #define rpower(%1,%2)         floatpower(%1,%2)
  #define rabs(%1)              floatabs(%1)

  #define rround_round          floatround_round
  #define rround_floor          floatround_floor
  #define rround_ceil           floatround_ceil
  #define rround_tozero         floatround_tozero
  #define rround_unbiased       floatround_unbiased
  #endinput
#endif

/* loading floating point support failed, try fixed point support */
#if !defined NOFIXED
  #tryinclude <fixed>
#endif
#if defined _inc_fixed || defined _inc_fixed_inc
  #define Rational:             Fixed:
  #define rval(%1)              strfixed(%1)
  #define rround(%1,%2)         fround(%1,%2)
  #define rsqroot(%1)           fsqroot(%1)
  #define rpower(%1,%2)         fpower(%1,%2)
  #define rabs(%1)              fabs(%1)

  #define rround_round          fround_round
  #define rround_floor          fround_floor
  #define rround_ceil           fround_ceil
  #define rround_tozero         fround_tozero
  #define rround_unbiased       fround_unbiased
  #endinput
#endif

/* if arrived here, neither floating point, nor fixed point support is available */
#error Rational number support is unavailable (disabled or not installed)

