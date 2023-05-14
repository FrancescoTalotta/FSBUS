#ifndef __FSBUS_H
#define __FSBUS_H

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

# if defined (BUILD_DLL_EXPORTS)
#   define DECLSPEC __declspec(dllexport)
# else
#   define DECLSPEC __declspec(dllimport)
# endif

#ifndef bool
#define bool	int
#define true	1
#define false	0
#endif
#ifndef BOOL
#define BOOL	int
#define TRUE	1
#define FALSE	0
#endif


//------------------------------------------------------------------------
// UVAR is a union for the fsuipc variables to and from flightsim
// In any of the user defined objects is a member called u, which
// is of this type.
typedef union 
{
        int             i32;
        unsigned int    ui32;
        short           i16;
        unsigned short  ui16;
        char            i8;
        unsigned char   ui8;
        __int64         i64;
        double          dbl;
        unsigned char   raw[8];
} UVAR;

// UTYPE is used by declaration of flightsim objects.
// please refer to the fsuipc documentation and decide, what
// type corresponds to a specific fsuipc offset.
typedef enum 
{
    TP_I8, TP_UI8,      // signed, unsigned 8Bit vars
    TP_I16, TP_UI16,    // signed, unsigned 16Bit vars
    TP_I32, TP_UI32,    // signed, unsigned 32Bit vars
    TP_I64,             // signed 64Bit vars
    TP_DBL              // 8Byte floating point vars
} UTYPE;

// Three different classes for the poll frequency can be
// assigned to any fsuipc variable. The standard timings are:
// NONE: this fsuipc offset is never read
// LAZY: the variable is read any 3s
// NORMAL: the variable is read any 300ms
// Quick: the variable is read any 100ms
//
typedef enum 
{
    FS_NONE, FS_LAZY, FS_NORMAL, FS_QUICK
} FSINTERVAL;

// Any event is returned to the application in one callback
// function. The object pointer is passed, which gives you
// access to all informations. The function GetType() returns
// the type of the polymorphic object pointer.
// You can simply typecast the object pointer to one of the
// derived classes. Look at the default callback function in main.cpp
typedef enum 
{
    FSBUSOBJ, FSOBJ, TIMEROBJ, SOUNDOBJ
} OBJTYPE;

// An object of type FSBUSOBJ controls any of the fsbus hardware
// interfaces. The FSBUSTYPE determines which kind of hardware.
typedef enum 
{
    BTP_D_IN,		// Digital In (switches buttons)
	BTP_ROTARY,		// 
	BTP_A_IN,		// Analogue in (pots)
	BTP_D_OUT,		// Digital out (LEDs)
	BTP_DISPLAY,	// 7 segment displays
	BTP_A_OUT,		// Analogue out (8bit analogue; servo)
	BTP_V_OUT		// Analogue variable length (stepper)
} FSBUSTYPE;

// three different modes to define the beheavior after an error occurs
typedef enum 
{
	EM_STOPEXECUTION,	// error message to console; stop executing  
	EM_RESUMERETURN,	// error message to ErrorText; return to current function
	EM_THROWEXCEPTION   // throw an exception of type LPSTR (C++ applications)
} ERRORMODE;

// this is for usage in calibrate function 
typedef struct 
{
	int		ival;
	int		oval;
} CALTAB;

// the udp extension has a type field, which determines the decoding of a 
// received data block. Release 1.x supports UDP_RAW only
typedef enum 
{
	UDP_RAW, 
	UDP_FSBUS, 
	UDP_XPLANE
} FSBUSUDPTYPE;

// the SFSBNET type describes a type UDP_FSBUS data block.
#define FSBUS_MAGIC 0xa93b


typedef struct 
{
    unsigned short    magic;
    unsigned short    seq;
    unsigned short    destcid;
    unsigned short    srccid;
    unsigned short    cmd;
    unsigned short    param;
    unsigned short    length;
} SFSBNET;


// an fsbus app must not include the winsock stuff
#ifndef _WINSOCKAPI_
#define SOCKET	int
#endif

// each udp service is described with a FSUDP type.
// An instance is created with the MkUdpService call
typedef struct sfsudp
{
	FSBUSUDPTYPE		type;
    SOCKET				s;
    int					port;
    ULONG				addr;
	int					xport;
	void				(* cb)(struct sfsudp* p);
    char                rbuf[2100];
	int					rcount;
} FSUDP;

DECLSPEC FSUDP*  MkUdpInterface (FSBUSUDPTYPE tp, int inport, void(* cb)(FSUDP* p));
DECLSPEC bool 	 UdpSend(FSUDP* udp, unsigned char* buf, int count);
DECLSPEC void	 UdpDestination(FSUDP* udp, char* host, int port);

//------------------------------------------------------------------------
// exported

int DECLSPEC			GetFsbusDLLVersion ();
BOOL DECLSPEC			CheckIn ();
BOOL DECLSPEC			CheckOut ();
BOOL DECLSPEC			FsbusMux (int maxtime);
int DECLSPEC			BCD2Int (unsigned short bcd);
unsigned int DECLSPEC	Int2BCD (int i);
BOOL DECLSPEC			Enable(int oid);
BOOL DECLSPEC			Disable(int oid);
extern char DECLSPEC	ErrorText[];
extern ERRORMODE DECLSPEC ErrorMode;

void DECLSPEC			Error(LPSTR fmt, ...);
BOOL DECLSPEC           MkFsObject (int oid, char* nm, void(* cb)(int oid, int val, double dbl), DWORD offset,int datasz, UTYPE datatp, FSINTERVAL intvl, int flags);
BOOL DECLSPEC           MkSound (int oid, char* nm, void(* cb)(int oid, int val, double dbl), char* soundfile);
BOOL DECLSPEC           MkLoopSound (int oid, char* nm, void(* cb)(int oid, int val, double dbl), char* soundfile);
BOOL DECLSPEC           MkTimer (int oid, char* nm, void(* cb)(int oid, int val, double dbl), DWORD tm, int flg);
BOOL DECLSPEC           MkFsbusObject(FSBUSTYPE ftp, int oid, char* nm, void(* cb)(int oid, int val, double dbl), int c, int r, int flg);

BOOL DECLSPEC           DisplayOptions (int oid, int SegCount, int SegOffs, bool LeadZero, int DecPoint);

BOOL DECLSPEC           FsbusOpen (LPSTR dev);
BOOL DECLSPEC           FsbusClose();

BOOL DECLSPEC           FsbusWrite (int oid, int v);
BOOL DECLSPEC           FsbusWriteRaw(unsigned char* buf, int count);

BOOL DECLSPEC           FsbusWriteFmt2(int cid, int rid, int val);
BOOL DECLSPEC           FsbusWriteFmt3(int cid, int rid, int val);
BOOL DECLSPEC           FsbusWriteFmtVar(int cid, int rid, int val);

BOOL DECLSPEC           FsConnect();
BOOL DECLSPEC           FsDisconnect();
void DECLSPEC			FsInvalidate();
void DECLSPEC			FsbusInvalidate();

BOOL DECLSPEC           FsReadDirect(int offset, int sz, void* dest);
BOOL DECLSPEC           FsWriteDirect(int offset, int sz, void* dest);
BOOL DECLSPEC           FsSetPollTiming (int quick, int normal, int lazy);
BOOL DECLSPEC			FsGetCompareBuffers (int oid, UVAR** newvar, UVAR** lastvar);

BOOL DECLSPEC			FsWriteInt (int oid, int i32);
BOOL DECLSPEC			FsWriteDbl (int oid, double d);
BOOL DECLSPEC			FsWriteInt64 (int oid, __int64 i64);
BOOL DECLSPEC			FsWriteUnion (int oid, UVAR u);

void DECLSPEC           Keyboard(const char* p);
void DECLSPEC           ExtKeyboard(const char* p);
void DECLSPEC           Play(int oid);
void DECLSPEC           Stop(int oid);
void DECLSPEC           Vol(int oid, int vol);
void DECLSPEC           Pan(int oid, int pan);
void DECLSPEC           Rewind(int oid);
void DECLSPEC           SetTime(int oid, DWORD tm);
int DECLSPEC			Calibrate (int val, CALTAB* t, int count);
void DECLSPEC			FsbusPowerOffOptions (int oid, int pwrtype, int offval);
void DECLSPEC			SetPower (int pwrtype, int val);

#define PWRTYPE_MAINBUS	1
#define PWRTYPE_AVIONIC	2


//------------------------------------------------------------------------
typedef struct
{
    int                 id;
	char*				name;
    OBJTYPE             type;
    int                 flags;
#define FLG_DISABLED        0x0002
#define FLG_LEADZERO        0x0004
#define FLG_ONESHOT         0x0008
#define FLG_LOOP			0x0010


#define FLG_WRITEQUEUED     0x0100
#define FLG_UPDATE          0x0101
#define FLG_INVALID			0x0200
#define FLG_PWROFF			0x0400
	int                 tag;
    UVAR                u;
	void				(* cb)(int oid, int val, double dbl);
} DOBJECT;

//----------------------------------------------------------------------
typedef struct
{
    DOBJECT         o;
    DWORD           offs;
    FSINTERVAL      interval;
    UTYPE           datatype;
    int             datasize;
    UVAR            urd;
    UVAR            ucompare;
    UVAR            uwr;
} FSOBJECT;

//---------------------------------------------------------------------------
typedef struct
{
    DOBJECT o;
    DWORD   time;
    DWORD   nextevent;
} TIMEROBJECT;


//----------------------------------------------------
typedef struct
{
    DOBJECT            o;
#if defined (BUILD_DLL_EXPORTS)
    LPDIRECTSOUNDBUFFER dsb;
#else
    LPVOID  dsb;
#endif
} SOUNDOBJECT;


//----------------------------------------------------
typedef struct
{
    DOBJECT             o;
    FSBUSTYPE           fsbustype;
    unsigned char       cid;
    unsigned char       rid;
    unsigned char       segcount;
    unsigned char       segoffs;
    unsigned char       LastCommaPos;
    unsigned char       CommaIndex;
    bool                bLeadzero;
	UVAR				uoff;
	bool				bUsePowerOffValue;
	int					PowerTrigger;
} FSBUSOBJECT;


#define DISPLAY_BLANK   -10000
#define DISPLAY_BAR     -10001
#define DISPLAY_STD     -10002

#define OID_GROUP_MASK	0x0fe0		// recommended mask for oid groups masks the upper 7bit of an oid to
                                    // identify a group id. The lower 5bit are control id's
                                    // Since is doesn't concern the DLL, you may change. 
#define OID_CONTROL_BITS	5		// number of bits for the id's of the controls									

#ifdef __cplusplus
}
#endif

#endif



