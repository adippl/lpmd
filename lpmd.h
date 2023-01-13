#ifndef _LPMD_H
#define _LPMD_H

#ifdef DEBUG
const char* daemon_sock_path="./lpmd.socket";
const char* daemon_adm_sock_path="./lpmd_adm.socket";
#else
const char* daemon_sock_path="/run/lpmd.socket";
const char* daemon_adm_sock_path="/run/lpmd_adm.socket";
#endif



#define BUF_SIZE 512

#endif // _LPMD_H
