/*  DLL support functions for dynamically loadable extension libraries.
 *
 *  Copyright (c) ITB CompuPhase, 2004-2011
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not
 *  use this file except in compliance with the License. You may obtain a copy
 *  of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 *
 *  Version: $Id: dllmain.c 4523 2011-06-21 15:03:47Z thiadmer $
 */

#if defined _UNICODE || defined __UNICODE__ || defined UNICODE
# if !defined UNICODE   /* for Windows */
#   define UNICODE
# endif
# if !defined _UNICODE  /* for C library */
#   define _UNICODE
# endif
#endif

#include <assert.h>
#include <windows.h>

#if !defined UNUSED_PARAM
  #define UNUSED_PARAM(p) ((void)(p))
#endif

HINSTANCE hinstDLL;

/* Especially Watcom C/C++ does not like DLLs that do not have a LibMain()
 * set. Apparently, the start address is not set well, and some required
 * initializations are not done.
 */
#if defined __WIN32__ || defined _WIN32 || defined WIN32

  BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID lpRes)
  {
    UNUSED_PARAM(lpRes);
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      hinstDLL=hinst;
      break;
    case DLL_PROCESS_DETACH:
      break;
    } /* switch */
    return TRUE;
  }

#else

  int FAR PASCAL LibMain(HINSTANCE hinst, WORD wDataSeg, WORD wHeapSize, LPSTR lpszCmdLine)
  {
    UNUSED_PARAM(wDataSeg);
    UNUSED_PARAM(wHeapSize);
    UNUSED_PARAM(lpszCmdLine);
    hinstDLL=hinst;
    return 1;
  }

  int FAR PASCAL _export WEP(int param)
  {
    UNUSED_PARAM(param);
    return 1;
  }

#endif /* __WIN32__ */

