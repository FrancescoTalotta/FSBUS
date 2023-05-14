#ifndef __FSBUSDLL_H
#define __FSBUSDLL_H

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <assert.h>

#include <mmsystem.h>
#include <dsound.h>
#include <winsock.h>

#ifndef BUILD_DLL_EXPORTS
#define BUILD_DLL_EXPORTS
#endif

#include "fsbus.h"
#include "dwstring.h"

#define DECLSPEC __declspec(dllexport)

#define MAXCONTAINEROBJECTS     5000
#define MAXWRQ                  300
#define MAXPOLL                 200

bool            NewObjectIdOk(int oid);
DOBJECT*        GetObject(int oid);
DOBJECT*        GetValidObject(int oid);

int             GetInt32(FSOBJECT* po);
BOOL            Write(FSOBJECT* po);
void            Write(FSOBJECT* po, int x);
BOOL            WriteDisplay(FSBUSOBJECT* po, int v);
void            WriteDigiOut(FSBUSOBJECT* po, int i32);
void            WriteAnalogue(FSBUSOBJECT* po, int i32);
void            WriteVarLength(FSBUSOBJECT* po, int i32);

DWORD           FsExecute (int max);
FSOBJECT*       FsGetNextUpdated();

void            Connect();
void            Disconnect();
bool            IsOk();
void            ReadDirect(int offset, int sz, void* dest);
void            WriteDirect(int offset, int sz, void* dest);
void            SetPollTiming (int quick, int normal, int lazy);
int             Init();
void            Process(int NextPoll, int cnt);
void            Clear();

SOUNDOBJECT*    CreateSoundObject(int oid, int f);
void            DestroyDSoundObject(SOUNDOBJECT*);
void            OnFsbusInput (FSBUSOBJECT *po, int v);
void            OnDigiIn(FSBUSOBJECT *po, int val);
void            OnAnalogueIn(FSBUSOBJECT *po, int val);
void            OnRotaryInput(FSBUSOBJECT *po, int val);

bool			LoadWsock();
void			UdpExecute(FSUDP* udp);

//-----------------------------------------------------------------
// Supported Sims
#define SIM_ANY	        0
#define SIM_FS98	    1
#define SIM_FS2K	    2
#define SIM_CFS2	    3
#define SIM_CFS1	    4
#define SIM_FLY	        5
#define SIM_FS2k2       6

#define LIB_VERSION 1004 // 1.003
#define MAX_SIZE 0x7F00 // Largest data (kept below 32k to avoid
						// any possible 16-bit sign problems)

#define FS6IPC_MSGNAME1      "FsasmLib:IPC"
#define FS6IPC_MESSAGE_SUCCESS 1
#define FS6IPC_MESSAGE_FAILURE 0

// IPC message types
#define FS6IPC_READSTATEDATA_ID    1
#define FS6IPC_WRITESTATEDATA_ID   2

// read request structure
typedef struct tagFS6IPC_READSTATEDATA_HDR
{
  DWORD dwId;       // FS6IPC_READSTATEDATA_ID
  DWORD dwOffset;   // state table offset
  DWORD nBytes;     // number of bytes of state data to read
  void* pDest;      // destination buffer for data (client use only)
} FS6IPC_READSTATEDATA_HDR;

// write request structure
typedef struct tagFS6IPC_WRITESTATEDATA_HDR
{
  DWORD dwId;       // FS6IPC_WRITESTATEDATA_ID
  DWORD dwOffset;   // state table offset
  DWORD nBytes;     // number of bytes of state data to write
} FS6IPC_WRITESTATEDATA_HDR;


//---------------------------------------------------------------------------

bool            FsuipcOpen();
bool            FsuipcClose();
bool            FsuipcProcess();
bool            FsuipcProcessBuffer();
bool            FsuipcRead(DWORD dwOffset, DWORD dwSize, void *pDest);
bool            FsuipcWrite(DWORD dwOffset, DWORD dwSize, void *pSrce);
BOOL			_FsbusWriteExt(int cid, unsigned char* buf, int count);

bool            CreateSound();
void            DestroySound();
char*           GetSoundErrtext(HRESULT hr);
BOOL            Load(int oid, char* f);

void            CreateTimer();
void            DestroyTimer();
DWORD           NextTimerEvent();
TIMEROBJECT*    GetTimerObject();

TIMEROBJECT*    CreateTimerObject(int id, DWORD tm, int f);
void            DestroyTimerObject();

void            CreateFsbus();
void            DestroyFsbus();

extern HANDLE   hWriteCompleteEvent;
extern HANDLE   hReadEvent;
extern DOBJECT* Objects[];

BOOL			Test (int id,char* name, int cid, int rid);
BOOL			Test (int id,char* name, int cid);
BOOL			TestCR (int id,char* name, int cid, int rid);
void			OnAsyncWriteComplete();
void			OnAsyncRead();
void			OnChar(unsigned char b);

typedef union
{
		unsigned char	buf[10];
		struct 
		{
			unsigned int		checksum;
		} s_buf;
} U_KEY;

bool			CheckLicense();
unsigned int	Checksum (char* s);
bool			Ascii2Bin (char* ascii, U_KEY* key);
void			NormalText (char *p);


#define MAXHANDLERS	8
extern FSUDP* udp[MAXHANDLERS];
extern int UdpHandlerCount;


#endif