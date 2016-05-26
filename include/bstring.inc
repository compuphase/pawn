/* Minimalistic wrapper for the "Better String library" by Paul Hsieh
 *
 * (c) Copyright 2004-2011, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#pragma library BString

/* native functions */
native String: bstring(const source[] = ``'');
native String: bstrdup(String: source);
native String: bstrcat(String: target, String: source);
native         bstrlen(String: source);
native String: bstrmid(String: source, start = 0, length = cellmax);
native         bstrtoarray(target[], size = sizeof target, String: source,
                           bool: packed = true);

/* user-defined operators */
native String:operator=(String:oper) = bstrdup;
native String:operator+(String:oper1, String:oper2) = bstrcat;
