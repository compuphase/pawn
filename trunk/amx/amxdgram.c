/*  Datagram sending/receiving module for the Pawn Abstract Machine
 *
 *  This module uses the UDP protocol (from the TCP/IP protocol suite).
 *
 *  Copyright (c) ITB CompuPhase, 2005-2011
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
 *  Version: $Id: amxdgram.c 4611 2011-12-05 17:46:53Z thiadmer $
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "osdefs.h"
#if defined __LINUX__ || defined __FreeBSD__ || defined __OpenBSD__
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/ioctl.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <unistd.h>
#else
  #include <malloc.h>
  #include <winsock.h>
#endif
#include "amx.h"


#define SRC_BUFSIZE     22
#define BUFLEN          512
#define AMX_DGRAMPORT   9930  /* default port */

#if !defined SOCKET_ERROR
  #define SOCKET_ERROR -1
#endif

static int sLocal;

static unsigned long udp_GetHostAddr(const char *host,int index)
{
  unsigned long addr=inet_addr(host);
  if (addr==0xffffffffL) {
    struct hostent *phost=gethostbyname(host);
    if (phost!=NULL) {
      /* count the number of addresses in the list */
      int count;
      for (count=0; phost->h_addr_list[count]!=0; count++)
        /* nothing */;
      if (index<count)
        addr=*(unsigned long *)phost->h_addr_list[index];
    } /* if */
  } /* if */
  return addr;
}

static int udp_Open(void)
{
#if defined __WIN32 || defined _WIN32 || defined WIN32
  WORD wVersionRequested = MAKEWORD(1,1);
  WSADATA wsaData;
#endif
  int optval = 1;

  #if defined __WIN32 || defined _WIN32 || defined WIN32
    WSAStartup(wVersionRequested, &wsaData);
  #endif

  if ((sLocal=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    return -1;

  if (setsockopt(sLocal, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) == -1)
    return -1;

  return sLocal;
}

static int udp_Close(void)
{
  if (sLocal>=0) {
    #if defined __WIN32 || defined _WIN32 || defined WIN32
      closesocket(sLocal);
    #else
      close(sLocal);
    #endif
  } /* if */

  #if defined __WIN32 || defined _WIN32 || defined WIN32
    WSACleanup();
  #endif

  return 0;
}

static int udp_Send(const char *host,short port,const char *message,int size)
{
  struct sockaddr_in sRemote;

  if (sLocal<0)
    return -1;

  memset((void *)&sRemote,sizeof sRemote,0);
  sRemote.sin_family=AF_INET;
  sRemote.sin_port=htons(port);
  sRemote.sin_addr.s_addr= (host==NULL) ? htonl(INADDR_BROADCAST) : udp_GetHostAddr(host,0);

  if (sendto(sLocal,message,size,0,(struct sockaddr *)&sRemote,sizeof sRemote)==-1)
    return -1;

  return size;
}

/* This call is blocking
 * if source is not NULL, it must point to a buffer that can contain at least
 * 22 characters.
 */
static int udp_Receive(char *message,size_t maxmsg,char *source)
{
  struct sockaddr_in sSource;
  int slen=sizeof(sSource);
  int size;

  size=recvfrom(sLocal, message, maxmsg - 1, 0, (struct sockaddr *)&sSource, &slen);
  if (size==-1)
    return -1;
  assert((size_t)size < maxmsg);
  message[size] = '\0';
  if (source!=NULL)
    sprintf(source, "%s:%d", inet_ntoa(sSource.sin_addr), ntohs(sSource.sin_port));

  return size;
}

static int udp_IsPacket(void)
{
  int result;
  fd_set rdset;
  struct timeval time;

  /* the select() function waits until the socket can be read, or until a
   * time-out occurs; the time-out is set to 1 microsecond (the shortest
   * delay possible).
   */
  time.tv_sec=0;
  time.tv_usec=1;
  FD_ZERO(&rdset);
  FD_SET(sLocal,&rdset);
  result=select(sLocal+1,&rdset,NULL,NULL,&time);
  if (result==SOCKET_ERROR)
    return -1;

  return result != 0;
}

static int udp_Listen(short port)
{
  struct sockaddr_in sFrom;

  memset((void *)&sFrom,sizeof sFrom,0);
  sFrom.sin_family=AF_INET;
  sFrom.sin_port=htons(port);
  sFrom.sin_addr.s_addr=htonl(INADDR_ANY);
  if (bind(sLocal,(struct sockaddr *)&sFrom,sizeof sFrom)==-1)
    return -1;

  return 0;
}

static AMX_IDLE PrevIdle = NULL;
static int idxReceiveString = -1;
static int idxReceivePacket = -1;
static short dgramPort = 0;
static int dgramBound = 0;

/* sendstring(const message[], const destination[]="")
 * destination has the format "127.0.0.1:9930"; when set to an empty string,
 * a broadcast is sent.
 * To mark the text as a "string", the function inserts a "byte order mark" in
 * front of it. It does this for Extended ASCII strings too, although this is
 * not entirely correct.
 * Returns true on success, false on failure.
 */
static cell AMX_NATIVE_CALL n_sendstring(AMX *amx, const cell *params)
{
  int r = 0, length;
  cell *cstr;
  char *host, *message, *ptr;
  short port=AMX_DGRAMPORT;

  cstr = amx_Address(amx, params[1]);
  amx_UTF8Len(cstr, &length);

  if ((message = alloca(length + 3 + 1)) != NULL) {
    /* insert the byte order mark (BOM) */
    message[0]='\xef';
    message[1]='\xbb';
    message[2]='\xbf';
    /* if this is a wide string, convert it to UTF-8 */
    if ((ucell)*cstr<=UNPACKEDMAX) {
      ptr=message+3;
      while (*cstr!=0)
        amx_UTF8Put(ptr, &ptr, length - (ptr-message), *cstr++);
      *ptr='\0';
    } else {
      amx_GetString(message+3, cstr, 0, UNLIMITED);
    } /* if */

    amx_StrParam(amx, params[2], host);
    if (host != NULL && (ptr=strchr(host,':'))!=NULL && isdigit(ptr[1])) {
      *ptr++='\0';
      port=(short)atoi(ptr);
    } /* if */
    r= (udp_Send(host,port,message,strlen(message)+1) > 0);
  } /* if */

  return r;
}

/* sendpacket(const packet[], size, const destination[]="")
 * destination has the format "127.0.0.1:9930"; when set to an empty string,
 * a broadcast is sent.
 * Returns true on success, false on failure.
 */
static cell AMX_NATIVE_CALL n_sendpacket(AMX *amx, const cell *params)
{
  cell *cstr;
  char *host, *ptr;
  short port=AMX_DGRAMPORT;

  cstr = amx_Address(amx, params[1]);
  amx_StrParam(amx, params[3], host);
  if (host != NULL && (ptr=strchr(host,':'))!=NULL && isdigit(ptr[1])) {
    *ptr++='\0';
    port=(short)atoi(ptr);
  } /* if */
  return (udp_Send(host,port,(const char *)cstr,params[2] * sizeof(cell)) > 0);
}

/* listenport(port)
 * A program must call this function from main() or another start-up function
 * because the module will use the default port 9930 otherwise.
 */
static cell AMX_NATIVE_CALL n_listenport(AMX *amx, const cell *params)
{
  (void)amx;
  dgramPort = (short)params[1];
  return 0;
}

static int AMXAPI amx_DGramIdle(AMX *amx, int AMXAPI Exec(AMX *, cell *, int))
{
  char message[BUFLEN], source[SRC_BUFSIZE];
  cell *amx_addr_src;
  int len, chars;
  int err=0;

  assert(idxReceiveString >= 0 || idxReceivePacket >= 0);

  if (PrevIdle != NULL)
    PrevIdle(amx, Exec);

  /* set up listener (first call only) */
  if (!dgramBound) {
    if (dgramPort==0)
      dgramPort=AMX_DGRAMPORT;  /* use default port if none was set */
    if (udp_Listen(dgramPort)==-1)
      return AMX_ERR_GENERAL;
    dgramBound=1;
  } /* if */

  if (udp_IsPacket()) {
    len=udp_Receive(message, sizeof message / sizeof message[0], source);
    amx_PushString(amx,&amx_addr_src,source,1,0);
    /* check the presence of a byte order mark: if it is absent, the received
     * packet is no string; also check the packet size against string length
     */
    if ((message[0]!='\xef' || message[1]!='\xbb' || message[2]!='\xbf'
        || len!=(int)strlen(message)+1 || idxReceiveString<0) && idxReceivePacket>=0)
    {
      /* receive as "packet" */
      amx_Push(amx,len);
      amx_PushArray(amx,NULL,(cell*)message,len);
      err=Exec(amx,NULL,idxReceivePacket);
    } else {
      const char *msg=message;
      if (msg[0]=='\xef' && msg[1]=='\xbb' && msg[2]=='\xbf')
        msg+=3;                 /* skip BOM */
      /* optionally convert from UTF-8 to a wide string */
      if (amx_UTF8Check(msg,&chars)==AMX_ERR_NONE) {
        cell *array=alloca((chars+1)*sizeof(cell));
        cell *ptr=array;
        if (array!=NULL) {
          while (err==AMX_ERR_NONE && *msg!='\0')
            amx_UTF8Get(msg,&msg,ptr++);
          *ptr=0;               /* zero-terminate */
          amx_PushArray(amx,NULL,array,chars+1);
        } /* if */
      } else {
        amx_PushString(amx,NULL,msg,1,0);
      } /* if */
      err=Exec(amx,NULL,idxReceiveString);
    } /* if */
    while (err==AMX_ERR_SLEEP)
      err=Exec(amx,NULL,AMX_EXEC_CONT);
    amx_Release(amx,amx_addr_src);
  } /* if */

  return err;
}

#if defined __cplusplus
  extern "C"
#endif
AMX_NATIVE_INFO dgram_Natives[] = {
  { "sendstring", n_sendstring },
  { "sendpacket", n_sendpacket },
  { "listenport", n_listenport },
  { NULL, NULL }        /* terminator */
};

int AMXEXPORT AMXAPI amx_DGramInit(AMX *amx)
{
  dgramBound = 0;
  if (udp_Open()==-1)
    return AMX_ERR_GENERAL;

  /* see whether there is an @receivestring() function */
  if (amx_FindPublic(amx,"@receivestring",&idxReceiveString)==AMX_ERR_NONE
      || amx_FindPublic(amx,"@receivepacket",&idxReceivePacket)==AMX_ERR_NONE)
  {
    if (amx_GetUserData(amx,AMX_USERTAG('I','d','l','e'),(void**)&PrevIdle)!=AMX_ERR_NONE)
      PrevIdle=NULL;
    amx_SetUserData(amx,AMX_USERTAG('I','d','l','e'),amx_DGramIdle);
  } /* if */

  return amx_Register(amx,dgram_Natives,-1);
}

int AMXEXPORT AMXAPI amx_DGramCleanup(AMX *amx)
{
  (void)amx;
  udp_Close();
  return AMX_ERR_NONE;
}
