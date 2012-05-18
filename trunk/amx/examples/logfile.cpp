#include <stdio.h>
#include <malloc>
#include <map>
#include "amx.h"

class LogFile;
static std::map<AMX*, LogFile*> lookup;

class LogFile {
public:
  LogFile()
  {
    f = tmpfile();
  }

  ~LogFile()
  {
    fclose(f);
  }

private:
  int write(AMX* /*amx*/, const cell params[])
  {
    int r = 0;
    char *pstr;

    amx_StrParam_Type(amx, params[1], pstr, char*);
    if (pstr != NULL)
      r = fprintf(f, "%s", pstr);
    return r;
  }

  FILE *f;

public:
  static cell n_write(AMX* amx, const cell params[])
  {
    std::map<AMX*, LogFile*>::iterator p = lookup.find(amx);
    if (p != lookup.end())
      return p->second->write(amx, params);
    return 0;
  }

};

extern "C"
int amx_LogFileInit(AMX* amx)
{
  LogFile* lf = new LogFile;
  if (lf) {
    lookup.insert(std::make_pair(amx, lf));

    static AMX_NATIVE_INFO nativelist[] = {
      { "write",  LogFile::n_write },
      { 0, 0 }        /* terminator */
    };
    return amx_Register(amx, nativelist, -1);
  } /* if */
  return AMX_ERR_MEMORY;
}

extern "C"
int amx_LogFileExit(AMX* amx)
{
  std::map<AMX*, LogFile*>::iterator p = lookup.find(amx);
  if (p != lookup.end()) {
    delete p->second;
    lookup.erase(p);
  } /* if */
  return AMX_ERR_NONE;
}
