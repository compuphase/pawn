/* An assortment of additional functions in the DLL version of the
 * Abstract Machine.
 *
 * (c) Copyright 2000-2011, ITB CompuPhase
 * This file is provided as is (no warranties).
 */

enum
    {
    Ok,
    Okcancel,
    okCancel,
    Yesno,
    yesNo,
    Yesnocancel,
    yesNocancel,
    yesnoCancel,
    }
enum
    {
    noicon,
    information,
    exclamation,
    question,
    stop
    }
native messagebox(const message[], const caption[], buttons=Ok, icons=noicon, timeout=0);

/* Types:
 *    i = integer (16/32 bit)
 *    l = 32-bit integer
 *    h = handle (16/32 bit)
 *    p = packed string
 *    s = (unpacked) string
 *    w = 16-bit unsigned integer
 * lower case = by value, upper case = by reference
 *
 * The types noted as "16/32 bit" are converted to 16-bit in Win16 environments
 * prior to the call to the DLL.
 *
 * calldll() attaches "32" to the dll name if it cannot load the DLL as you
 * provide it. So it loads "user32" if you specify "user" and you are in Win32.
 * calldll() also appends a "A" to the function name if it cannot find the
 * function as specified. So it call "GetWindowTextA" if you ask for
 * "GetWindowText". By this means, the interface is more compatible between
 * 16-bit and 32-bit platforms.
 */
native calldll(const dllname[], const function[], const typestr[]="", ...);

native loaddll(const dllname[]);
native freedll(const dllname[]);

native iswin32();

native balloon(&Balloon:handle, text[]="", x=0, y=0, timeout=-1);
native balloonfont(&Balloon:handle, const font[]="", height=16, weight=400, italic=0);

stock wGetCursorPos(&x, &y)
    /* Get the current position of the mouse cursor relative to the upper
     * left corner of the screen
     */
    {
    new point[2]
    calldll(!"user","GetCursorPos","I",point)
    if (!iswin32())
        {
        point[1] = point[0] >> 16
        point[0] &= 0xffff
        }
    x = point[0]
    y = point[1]
    }


