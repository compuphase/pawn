/* This file implements two the native functions: power(value,exponent)
 * and sqroot(value).
 */
#include "amx.h"

static cell n_power(AMX *amx, const cell *params)
{
  /* power(value, exponent);
   *   params[1] = value
   *   params[2] = exponent
   */
  cell result = 1;
  cell exponent = params[2];
  while (exponent-- > 0)
    result *= params[1];
  return result;
}

static cell n_sqroot(AMX *amx, const cell *params)
{
  /* sqroot(value);
   *   params[1] = value
   * This routine uses a simple successice approximation algorithm.
   */
  cell div = params[1];
  cell result = 1;
  while (div > result) {        /* end when div == result, or just below */
    div = (div + result) / 2;   /* take mean value as new divisor */
    result = params[1] / div;
  } /* while */
  return div;
}

int amx_PowerInit(AMX *amx)
{
  static AMX_NATIVE_INFO power_Natives[] = {
    { "power",  n_power },
    { "sqroot", n_sqroot },
    { 0, 0 }        /* terminator */
  };
  return amx_Register(amx, power_Natives, -1);
}

int amx_PowerCleanup(AMX *amx)
{
  return AMX_ERR_NONE;
}
