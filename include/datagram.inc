/* Datagram sending/receiving
 *
 * (c) Copyright 2005-2011, ITB CompuPhase
 * This file is provided as is (no warranties).
 */
#pragma library DGram

native sendstring(const message[], const destination[]=``'');
native sendpacket(const packet[], size, const destination[]=``'');

native listenport(port);

forward @receivestring(const message[], const source[]);
forward @receivepacket(const packet[], size, const source[]);
