#ifndef __DUDP_H__
#define __DUDP_H__

#include <windows.h>
#include <winsock.h>

#define ui16 unsigned short
typedef enum 
{
	UDP_RAW, UDP_FSBUS, UDP_XPLANE
} FSBUSUDPTYPE;

typedef struct {
    ui16    magic;
    ui16    seq;

    ui16    destcid;
    ui16    srccid;
    ui16    cmd;
    ui16    param;
    ui16    length;
} SFSBNET;

#define FSBUS_MAGIC 0xa93b

typedef struct sfsudp
{
    SOCKET				s;
    int					port;
	void				(* cb)(struct sfsudp* p);
    struct sockaddr     sender;
    char                rbuf[1024];
	int					rcount;
	struct sockaddr_in	receiver;	
    ULONG				addr;
	FSBUSUDPTYPE		type;
} FSUDP;


FSUDP* MkUdpInterface (FSBUSUDPTYPE tp, int port, void(* cb)(struct sfsudp* p), LPCSTR receiver);
bool LoadWsock();
void UdpExecute(FSUDP* udp);
bool UdpSend(FSUDP* udp, unsigned char* buf, int count);


#endif