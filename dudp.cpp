//---------------------------------------------------------------------------
#include "stdafx.h"
#include <winsock.h>
#include "fsbusdll.h"

HINSTANCE hwsocklib = (HINSTANCE)1;

typedef int (PASCAL FAR *pWSAStartup)(IN WORD wVersionRequired, OUT LPWSADATA lpWSAData);
typedef SOCKET (PASCAL FAR *psocket) (IN int af,IN int type,IN int protocol);
typedef int (PASCAL FAR *psendto) (IN SOCKET s,IN const char FAR * buf,IN int len,IN int flags,IN const struct sockaddr FAR *to,IN int tolen);
typedef int (PASCAL FAR *precvfrom )(IN SOCKET s,__out_bcount_part(len, return) __out_data_source(NETWORK) char FAR * buf,IN int len,IN int flags,
						 __out_bcount(*fromlen) struct sockaddr FAR * from,IN OUT int FAR * fromlen);
typedef int (PASCAL FAR *pbind) (IN SOCKET s,IN const struct sockaddr FAR *addr,IN int namelen);
typedef int (PASCAL FAR *pioctlsocket) (IN SOCKET s,IN long cmd,IN OUT u_long FAR *argp);
typedef int (PASCAL FAR *pclosesocket) ( IN SOCKET s);
typedef int (PASCAL FAR *pWSAGetLastError)(void);
typedef u_short (PASCAL FAR *phtons) (IN u_short hostshort);
typedef unsigned long (PASCAL FAR *pinet_addr) (IN const char FAR * cp);
typedef char FAR * (PASCAL FAR *pinet_ntoa) (IN struct in_addr in);
typedef struct hostent FAR * (PASCAL FAR *pgethostbyaddr)(IN const char FAR * addr,IN int len,IN int type);
typedef struct hostent FAR * (PASCAL FAR *pgethostbyname)(IN const char FAR * name);

pWSAStartup			__WSAStartup;
psocket				__socket;
psendto				__sendto;
precvfrom			__recvfrom;
pbind				__bind;
pioctlsocket		__ioctlsocket;
pclosesocket		__closesocket;
pWSAGetLastError	__WSAGetLastError;
phtons				__htons;
pinet_addr			__inet_addr;
pinet_ntoa			__inet_ntoa;
pgethostbyaddr		__gethostbyaddr;
pgethostbyname		__gethostbyname;

//#define MAXHANDLERS	8
FSUDP* udp[MAXHANDLERS];
int UdpHandlerCount;

bool LoadWsock()
{
	hwsocklib = LoadLibrary( "wsock32.dll");

	if (!hwsocklib)
	{
        printf ("wsock32.dll not available");
        return false;
    }
	UdpHandlerCount = 0 ;

	__WSAStartup = reinterpret_cast <pWSAStartup>(GetProcAddress (hwsocklib, "WSAStartup"));
	__socket = reinterpret_cast <psocket>(GetProcAddress (hwsocklib, "socket"));
	__sendto = reinterpret_cast <psendto>(GetProcAddress (hwsocklib, "sendto"));
	__recvfrom = reinterpret_cast <precvfrom>(GetProcAddress (hwsocklib, "recvfrom"));
	__bind = reinterpret_cast <pbind>(GetProcAddress (hwsocklib, "bind"));
	__ioctlsocket = reinterpret_cast <pioctlsocket>(GetProcAddress (hwsocklib, "ioctlsocket"));
	__closesocket = reinterpret_cast <pclosesocket>(GetProcAddress (hwsocklib, "closesocket"));
	__WSAGetLastError = reinterpret_cast <pWSAGetLastError>(GetProcAddress (hwsocklib, "WSAGetLastError"));
	__htons = reinterpret_cast <phtons>(GetProcAddress (hwsocklib, "htons"));
	__inet_addr = reinterpret_cast <pinet_addr>(GetProcAddress (hwsocklib, "inet_addr"));
	__inet_ntoa = reinterpret_cast <pinet_ntoa>(GetProcAddress (hwsocklib, "inet_ntoa"));
	__gethostbyaddr = reinterpret_cast <pgethostbyaddr>(GetProcAddress (hwsocklib, "gethostbyaddr"));
	__gethostbyname = reinterpret_cast <pgethostbyname>(GetProcAddress (hwsocklib, "gethostbyname"));

	if (!__WSAStartup || !__socket || !__sendto || !__recvfrom || !__bind
	|| !__ioctlsocket || !__closesocket || !__WSAGetLastError || !__htons
	|| !__inet_addr || !__inet_ntoa || !__gethostbyaddr || !__gethostbyname)
	{
        printf ("cannot load functions in wsock32.dll");
		FreeLibrary(hwsocklib);
		hwsocklib = NULL;
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------
FSUDP* MkUdpInterface (FSBUSUDPTYPE tp, int inport, void(* cb)(FSUDP* udp))
{
	WSADATA wsaData;

	if (hwsocklib == (HINSTANCE)1)
		LoadWsock();

	if (!hwsocklib) 
	{
		Error("wsock32.lib was not loaded");
		return NULL;
	}

	int iResult = __WSAStartup(MAKEWORD(1,1), &wsaData);
	if (iResult != 0) 
	{
		Error("WSAStartup failed: %d\n", __WSAGetLastError());
		return NULL;
	}

	if (UdpHandlerCount >= MAXHANDLERS)
	{
		Error ("number of udp handlers exceeds the maximum of %d", MAXHANDLERS);
		return NULL;
	}

	FSUDP* u = new FSUDP;

	u->s = __socket (PF_INET, SOCK_DGRAM, 0);
    if (u->s == INVALID_SOCKET)
    {
		Error ("socket error %d", __WSAGetLastError());
		return NULL;
    }
	u->cb = cb;
	u->port = inport;
	u->addr = NULL;
	u->xport = 0;

	if (inport)
	{
		struct sockaddr_in	cli;
		cli.sin_family = AF_INET;
		cli.sin_addr.s_addr = INADDR_ANY;
		cli.sin_port = __htons (inport);
		if (__bind (u->s,(LPSOCKADDR)&cli, sizeof(cli)) == SOCKET_ERROR)
		{
			DWORD dw = __WSAGetLastError();
			Error ("udp bind error %d", dw);
			__closesocket(u->s);
			return NULL;
		}
	}
	unsigned long nonblocking = 1;
	if (__ioctlsocket(u->s, FIONBIO, &nonblocking) != 0)
    {
        DWORD dw = __WSAGetLastError();
		Error("ioctrlsocket error %d", dw);
		__closesocket(u->s);
		return NULL;
    }
	udp[UdpHandlerCount++] = u;
	return u;
}

void UdpDestination(FSUDP* udp, char* host, int port)
{
	udp->addr = __inet_addr (host);
	if (udp->addr == (ULONG)INADDR_NONE)
    {
        struct hostent* phe = __gethostbyname (host);
        if (phe)
	        udp->addr = *(u_long FAR *) phe->h_addr_list[0];
    }
	udp->xport = port;
}


void UdpExecute(FSUDP* udp)
{
	if (!hwsocklib) 
		return;
    struct sockaddr_in sender;
	int	fromlen = sizeof(sender);

	for (;;)
	{
		udp->rcount = __recvfrom (udp->s, udp->rbuf,sizeof(udp->rbuf),0,(struct sockaddr*)&sender,&fromlen);
		udp->addr = sender.sin_addr.s_addr;

		switch (udp->rcount)
		{
		case -1:
			return;
		case 0:
			udp->cb(udp);
			return;
		default:
			udp->cb(udp);
			break;
		}
	}	
}

bool UdpSend(FSUDP* udp, unsigned char* buf, int count)
{
	if (!hwsocklib) 
	{
		Error("wsock32.lib was not loaded");
		return false;
	}
	if (!udp->addr || !udp->xport)
	{
		Error("udp: no destination defined");
		return false;
	}
	struct sockaddr_in	receiver;	
	receiver.sin_family = AF_INET;
    receiver.sin_port = __htons (udp->xport);
	receiver.sin_addr.s_addr = udp->addr;
	
	if (__sendto (udp->s,(LPSTR)buf,count,0,(const sockaddr*)&receiver, sizeof(receiver)) == SOCKET_ERROR)
    {
        return false;
    }
	return true;
}

/*

void __fastcall  DUDP :: OnUDP()
{
    SFSBNET* pfs = (SFSBNET*)rbuf;
    char* p = pfs->data;
    int cnt = pfs->length;
    while (cnt)
    {
        int n=1;
        for (--cnt; cnt; cnt--,n++)
            if (p[n] & 0x80)
                break;
        int cid = (p[0] & 0x7c) >> 2;
        int rid = ((p[0] & 0x02) << 6) | p[1];
        unsigned int val = 0;
        for (int i=n; i>2; i--)
            val = (val << 7) | p[i];
        val = (val << 1) | (p[0] & 0x01);
        if (val & 0x00200000)
            val |= 0xffc00000;
        p += n;
//        frmbu->SetValue (rid, (int)val);
    }
//    com->Write(pfs->data, pfs->length);
}

//---------------------------------------------------------------------------
bool DUDP :: AddUdpCommand (int cid, int rid, int val)
{
    SFSBNET* pfs = (SFSBNET*)xbuf;
    if (xoffs > 512)
    {
        Send (xbuf, xoffs);
        xoffs = 0;
    }
    if (xoffs == 0)
    {
        pfs->magic = FSBUS_MAGIC;
        pfs->seq = xseq++;
        pfs->destcid = cid;
        pfs->srccid = 0;
        pfs->cmd = CMD_GLASDATA;
        pfs->param = 0;
        pfs->length = 0;
        xoffs = sizeof(SFSBNET);
    }
    char* p = &xbuf[xoffs];
    p[0] = 0x80 | ((cid&0x1f)<<2) | ((rid&0x80)>>6) | (val&0x01);
    p[1] = rid & 0x7f;
    p[2] = (val & 0x000000fe) >> 1;
    p[3] = (val & 0x00007f00) >> 8;
    p[4] = (val & 0x003f8000) >> 15;
    xoffs += 5;
    pfs->length += 5;
    return true;
}

bool DUDP :: Send (void* buf, int count)
{
	struct sockaddr_in	sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = addr;
    sa.sin_port = htons (23868);
    if (s)
        if (sendto (s,(LPSTR)buf,count,0,(const sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR)
        {
            err = WSAGetLastError();
            return false;
        }
    return true;
}

void DUDP :: Flush ()
{
    SFSBNET* pfs = (SFSBNET*)xbuf;
    if (xoffs > 0)
    {
        Send (xbuf, xoffs);
        xoffs = 0;
    }
}

bool  DUDP :: SendFSClient (int cid, void* buf, int count)
{
    if (!s)
        return false;

    static int last = 0;
    struct s_buf {
        unsigned short  magic;
        unsigned short  sequence;
        unsigned short  destcid;
        unsigned short  sourcecid;
        unsigned short  cmd;
        unsigned short  param;
        unsigned short  len;
        unsigned short  data[512];
    } b;
    b.magic = 0xA93B;
    b.destcid = cid;
    b.sequence = ++last;
    b.sourcecid = 0;
    b.cmd = 513;
    b.param = 0;
    b.len = count;
    if (count >= sizeof(b.data))
        return false;
    // buf enthält key mouse events als unsigned shorts
    memcpy (b.data, buf, count);

	struct sockaddr_in	sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = addr;
    sa.sin_port = htons (23866);

    if (sendto (s,(LPSTR)&b,sizeof(b)+count,0,(const sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR)
    {
        err = WSAGetLastError();
        return false;
    }
    return true;
}
*/