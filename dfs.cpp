//---------------------------------------------------------------------------
#include "stdafx.h"

#include "fsbusdll.h"

//#pragma hdrstop

DOBJECT*        Objects[MAXCONTAINEROBJECTS];
LPSTR           FsuipcLicense = "Y98RK3FTD0QN";

char DECLSPEC	ErrorText[256] = "";
ERRORMODE DECLSPEC ErrorMode = EM_STOPEXECUTION;

MEMORYSTREAM*   pmWrqInUse;
MEMORYSTREAM*   pmWrq;
OVERLAPPED      FSBovlwr;
OVERLAPPED      FSBovlrd;
unsigned char   FSBrdbuf[256];
int             FSBcid;
int             FSBrid;
int             FSBval;
int             FSBseqno;
int             FSBRdId;

FSOBJECT*		wrq[MAXWRQ];
int				nwriteitems;
bool			bCleared;
int				LazyInterval;
int				NormalInterval;
int				QuickInterval;

STRINGLIST*		QuickPoll;
DWORD			nextquicktick;
int				nextquickidx;

STRINGLIST*		NormalPoll;
DWORD			nextnormaltick;
int				nextnormalidx;

STRINGLIST*		LazyPoll;
DWORD			nextlazytick;
int				nextlazyidx;

FSOBJECT*		PollObjects[MAXPOLL];
int				PollCount;
int				NextPoll;

FSOBJECT*		UpdObjects[MAXPOLL];
int				UpdCount;
int				NextUpd;
bool			bNewObject = false;

DWORD			FSUIPC_Version;
DWORD			FSUIPC_FS_Version;
DWORD			FSUIPC_Lib_Version;
int				FSUIPC_ConnectionType;
#define CT_FSUIPC           1
#define CT_FDSCONNECT       2
#define CT_WIDEFS           3
#define CT_FDSWIDECONNECT   4
HWND			FSUIPC_m_hWnd;       // FS6 window handle
UINT			FSUIPC_m_msg;        // id of registered window message
ATOM			FSUIPC_m_atom;       // global atom containing name of file-mapping object
HANDLE			FSUIPC_m_hMap;       // handle of file-mapping object
BYTE*			FSUIPC_m_pView;      // pointer to view of file-mapping object
BYTE*			FSUIPC_m_pNext;
CRITICAL_SECTION FSUIPC_csect;
bool			FSUIPC_OK;

void FsbusObjectReport();

//-------------------------------------------------------------------
int DECLSPEC GetFsbusDLLVersion ()
{
	return 302;
}

//-------------------------------------------------------------------
BOOL CheckIn ()
{
	ErrorText[0] = 0;

    for (int i=0; i<MAXCONTAINEROBJECTS; i++)
        Objects[i] = NULL;
    CreateSound();
    CreateTimer();
    CreateFsbus();

    nwriteitems = 0;
    QuickPoll = SLCreate();
    NormalPoll = SLCreate();
    LazyPoll = SLCreate();
    nwriteitems = 0;
    bCleared = true;
    UpdCount = 0;
    NextUpd = 0;
    LazyInterval = 3000;
    NormalInterval = 300;
    QuickInterval = 100;

	int v = GetFsbusDLLVersion ();

	char txt[256];
	sprintf_s (txt, sizeof(txt), "FSBUS.DLL %d.%d.%d", v/100, (v%100)/10, v%10 );
	printf ("%s\r\n", txt);
    printf ("www.fsbus.de\r\n\r\n");

	if (LoadWsock() == false)
	{
	    printf ("\r\nudp network support disabled\r\n");
	}
	UdpHandlerCount = 0;

	return TRUE;
}

//-------------------------------------------------------------------
BOOL CheckOut()
{
    DestroySound();
    DestroyTimer();
    DestroyFsbus();

    for (int i=0; i<MAXCONTAINEROBJECTS; i++)
    {
        if (Objects[i])
            free (Objects[i]);
    }
	return TRUE;
}

//-------------------------------------------------------------------
bool NewObjectIdOk(int oid)
{
    if (oid <= 0 || oid >= MAXCONTAINEROBJECTS)
    {
        Error ("Object-ID %d(%d,%d) out of range", oid, oid>>5, oid&0x1f);
        return false;
    }

    if (Objects[oid])
    {
        Error ("Object-ID %d(%d,%d) still in use", oid, oid>>5, oid&0x1f);
        return false;
    }
    return true;
}

DOBJECT* GetObject(int oid)
{
    if (oid <= 0 || oid >= MAXCONTAINEROBJECTS)
        return NULL;
    return Objects[oid];
}

DOBJECT* GetValidObject(int oid)
{
    if (oid <= 0 || oid >= MAXCONTAINEROBJECTS)
    {
        Error ("Object-ID %d(%d,%d) out of range", oid, oid>>5, oid&0x1f);
        return NULL;
    }
    if (Objects[oid])
        return Objects[oid];

    Error ("no Object with id %d(%d,%d)", oid, oid>>5, oid&0x1f);
    return NULL;
}

BOOL Enable(int oid)
{
    DOBJECT* po = GetValidObject(oid);
	if (!po)
		return FALSE;
    po->flags &= ~FLG_DISABLED;
	po->flags |= FLG_UPDATE;
    TIMEROBJECT *pto;

    switch (po->type)
    {
    case TIMEROBJ:
        pto = (TIMEROBJECT*) po;
        SetTime (oid, pto->time);
        break;
    }
	return TRUE;
}

BOOL Disable(int oid)
{
    DOBJECT* po = GetValidObject(oid);
	if (!po)
		return FALSE;
    po->flags |= FLG_DISABLED;
	po->flags &= ~FLG_UPDATE;
    TIMEROBJECT *pto;

    switch (po->type)
    {
    case TIMEROBJ:
        pto = (TIMEROBJECT*) po;
        SetTime (oid, pto->time);
        break;
    }
	return TRUE;
}

void Error (LPSTR fmt, ...)
{
	LPSTR p;
    int i;
    int sz = strlen(fmt);
    int max = sizeof(ErrorText)-1;
    int mi = 0;

	va_list ap;
	va_start(ap, fmt);

    for (;*fmt; fmt++)
    {
    	if (*fmt != '%')
        {
            if (mi < max)
			    ErrorText[mi++] = *fmt;
            continue;
        }
		switch (*++fmt) {
       	case '%':
            if (mi < max)
			    ErrorText[mi++] = *fmt;
			break;

        case 's':
	        p = va_arg(ap, char *);
            if ((int)(strlen(p) + sz) >= max)
                break;
            sprintf_s (&ErrorText[mi], sizeof(ErrorText)-mi, "%s", p);
            mi = strlen(ErrorText);
			break;

		case 'd':
            i = va_arg (ap, int);
            if ((5 + sz) >= max)
                break;
            sprintf_s (&ErrorText[mi], sizeof(ErrorText)-mi, "%d", i);
            mi = strlen(ErrorText);
        	break;
       	}
    }
	va_end(ap);
    ErrorText[mi] = 0;

	switch (ErrorMode)
	{
	case EM_STOPEXECUTION:
		break;
	case EM_RESUMERETURN:
		return;
//	case EM_THROWEXCEPTION:
//		throw (ErrorText);
	}
	printf ("FSBUS DLL:\r\n");
    printf ("%s\r\n", ErrorText);
    printf ("press any key to exit...\r\n");
	Sleep (1000);
    while (! _kbhit())
		Sleep(200);
	exit (1);
}

//-------------------------------------------------------------------
void FsbusObjectReport()
{
	int ifs = 0;
	int ifsd = 0;
	int ifslazy = 0;
	int ifsnorm = 0;
	int ifsquick = 0; 
	int ifsb = 0;
	int ifsbd = 0;
	int itmr = 0;
	int isnd = 0;
	int irot = 0;
	int idin = 0;
	int iain = 0;
	int idout = 0;
	int idsp = 0;
	int iaout = 0;
	int ivout = 0;

	for (int i=0; i<MAXCONTAINEROBJECTS; i++)
	{
		DOBJECT* po = Objects[i];
		FSOBJECT* pfso;
		FSBUSOBJECT* pfsb;
        if (!po)
			continue;
		switch (po->type)
		{
		case FSOBJ:
			if (po->flags & FLG_DISABLED)
				ifsd++;
			else
				ifs++;
			pfso = (FSOBJECT*)po;
			switch (pfso->interval)
			{
			case FS_LAZY:	ifslazy++; break;
			case FS_NORMAL: ifsnorm++; break;
			case FS_QUICK:  ifsquick++; break;
			}
			break;

		case FSBUSOBJ:
			if (po->flags & FLG_DISABLED)
				ifsbd++;
			else
				ifsb++;
			pfsb = (FSBUSOBJECT*)po;
			switch (pfsb->fsbustype)
			{
			case BTP_D_IN: idin++; break; 
			case BTP_ROTARY: irot++; break; 
			case BTP_A_IN: iain++; break;
			case BTP_D_OUT:	idout++; break;
			case BTP_DISPLAY: idsp++; break;
			case BTP_A_OUT: iaout++; break;
			case BTP_V_OUT: ivout++; break; 
			}
			break;
		case TIMEROBJ:
			itmr++;
			break;

		case SOUNDOBJ:
			isnd++;
			break;
		}
	}
	printf ("FS objects ......... %d\r\n", ifs+ifsd);
	printf ("   enabled .................... %d\r\n", ifs);
	printf ("   disabled ................... %d\r\n", ifsd);
	printf ("   polling .................... lazy:%d, normal:%d, quick:%d\r\n", ifslazy, ifsnorm, ifsquick);
	printf ("FSBUS objects ...... %d\r\n", ifsb+ifsbd);
	printf ("   enabled .................... %d\r\n", ifsb);
	printf ("   disabled ................... %d\r\n", ifsbd);
	printf ("   button,switch .............. %d\r\n", idin);
	printf ("   rotary ..................... %d\r\n", irot);
	printf ("   analog in .................. %d\r\n", iain);
	printf ("   led, digital out ........... %d\r\n", idout);
	printf ("   7 segment display .......... %d\r\n", idsp);
	printf ("   servo, analog out .......... %d\r\n", iaout);
	printf ("   stepper .................... %d\r\n", ivout);

	printf ("Timer objects ...... %d\r\n", itmr);
	printf ("Sound objects ...... %d\r\n", isnd);
}

//-------------------------------------------------------------------
static int bfirst = 1;

BOOL FsbusMux (int maxms)
{
	if (bNewObject)
	{
		bNewObject = false;

		SLClear(QuickPoll);
		SLClear(NormalPoll);
		SLClear(LazyPoll);

		for (int i=0; i<MAXCONTAINEROBJECTS; i++)
		{
			DOBJECT* po = Objects[i];
		    if (!po)
			    continue;
			if (po->type == FSOBJ)
			{
				FSOBJECT* pfs = (FSOBJECT*)po;
				switch (pfs->interval)
				{
				case FS_NONE:
				    break;
				case FS_QUICK:
				    SLAddObject(QuickPoll, "", (void*)po);
				    break;
				case FS_NORMAL:
				    SLAddObject(NormalPoll, "", (void*)po);
				    break;
				case FS_LAZY:
				    SLAddObject(LazyPoll, "", (void*)po);
				    break;
				}
			}
		}
	}
	if (bfirst)
	{
		bfirst = 0;
		FsbusObjectReport();
		printf ("\r\nrunning ...");

	}

    DWORD tstart = GetTickCount();
    DWORD tto = tstart + maxms;
    int gap = 0;
	DWORD tnextextpoll = 0;

    HANDLE hs[2];
    hs[0] = hWriteCompleteEvent;
    hs[1] = hReadEvent;

    // Loop until timeout reached
    while (GetTickCount() < tto)
    {
		// send all buttons and analog values to its callback function
		extern bool	bTransmitFsbusState;
		if (bTransmitFsbusState)
		{
			bTransmitFsbusState = false;
		    for (int i=0; i<MAXCONTAINEROBJECTS; i++)
			{
				DOBJECT* po = Objects[i];
				if (!po || (po->type != FSBUSOBJ) || (po->flags & FLG_DISABLED))
					continue;
				FSBUSOBJECT *pfb = (FSBUSOBJECT *)po;
				switch (pfb->fsbustype)
				{
				case BTP_D_IN:
				case BTP_A_IN:
				    if (pfb->o.cb)
						(*pfb->o.cb)(pfb->o.id, pfb->o.u.i32, 0.0);
					break;
				}
			}
		}

        // check, if async read and write of FSBUS interface are completed
        DWORD dw = WaitForMultipleObjects(2, hs, FALSE, gap);
        switch (dw)
        {
        case WAIT_OBJECT_0:
            // if there is a pending backbuffer to write, restart it now
            OnAsyncWriteComplete();
            break;

        case WAIT_OBJECT_0+1:
            OnAsyncRead();
            // data is available and is now decoded and sent to user function
            break;
        }

        // poll of max 100 objects from  fsuipc
        // gap is time until next required poll
        gap = FsExecute (100);
        // check for updated values and send each to user function
		FSOBJECT* pfo;
		while (pfo = FsGetNextUpdated())
			if (pfo->o.cb && ((pfo->o.flags & FLG_DISABLED) == 0))
				(*pfo->o.cb)(pfo->o.id, GetInt32(pfo), pfo->datatype == TP_DBL ? pfo->urd.dbl : 0.0);

        // test for timer objects
        TIMEROBJECT* pto;
        for (;;)
        {
            pto = GetTimerObject();
            if (!pto)
                break;
			if (pto->o.cb && ((pto->o.flags & FLG_DISABLED) == 0))
				(*pto->o.cb)(pto->o.id, 0, 0.0);
        }
        DWORD dwt = NextTimerEvent();
		DWORD now = GetTickCount();

		if (UdpHandlerCount)
		{
			if (now >= tnextextpoll)
			{
				// utilize the external handlers
				for (int i=0; i<UdpHandlerCount; i++)
				{
					UdpExecute(udp[i]);
				}
				now = GetTickCount();
				tnextextpoll = now + 50;
			}
		}

        // ist noch Zeit übrig ?
        if ((gap + tstart) > dwt)
            gap = dwt - now;
        if ((gap + tstart) > tto)
            gap = tto - now;
		if (UdpHandlerCount && (((DWORD)tnextextpoll - now) < (DWORD)gap))
			gap = (tnextextpoll - now);
        if (gap < 0)
            gap = 0;
    }
	return TRUE;
}

int BCD2Int (unsigned short bcd)
{
    int x=0;
    int shift = 12;
    int mul = 1000;
    for (int i=0; i<4; i++)
    {
        x += ((bcd >> shift) & 0x0f) * mul;
        mul /= 10;
        shift -= 4;
    }
    return x;
}

unsigned int Int2BCD (int i)
{
    unsigned int ui = 0;

    for (int shft=0;i;shft+=4)
    {
        ui = ui | ((i%10) << shft);
        i /= 10;
    }
    return ui;
}

//-------------------------------------------------------------------
BOOL MkFsObject(
				int oid,
				char *nm,
				void(* cb)(int oid, int val, double dbl), 
				DWORD offset,
				int datasz, 
				UTYPE datatp, 
				FSINTERVAL intvl, 
				int flags)
{
    if (!NewObjectIdOk(oid))
		return FALSE;

    if (datasz < 1 || datasz > sizeof(UVAR))
    {
        Error ("Object: %d(%d,%d) datasize out of range", oid, oid>>5, oid&0x1f);
        return FALSE;
    }
    FSOBJECT* po = (FSOBJECT*) malloc (sizeof(FSOBJECT));
    Objects[oid] = (DOBJECT*)po;

    po->o.id    = oid;
    po->o.type  = FSOBJ;
    po->o.flags = flags & (FLG_DISABLED);
    po->o.tag = 0;
    po->o.u.i64 = 0;
	po->o.cb = cb;
	po->o.name = "";
	if (nm)
		po->o.name = _strdup( nm);


    po->offs = offset;
    po->interval = intvl;
    po->datatype = datatp;
    po->datasize = datasz;
    po->urd.i32 = 0;
    po->ucompare.i64 = 0;
    po->uwr.i64 = 0;
	bNewObject = true;
	return TRUE;
}

BOOL FsGetCompareBuffers (int oid, UVAR** newvar, UVAR** lastvar)
{
	*newvar = NULL;
	*lastvar = NULL;
	DOBJECT* poo = GetValidObject(oid);
    if (poo->type != FSOBJ)
	{
        Error ("%d(%d,%d) is not an fs object", oid, oid>>5, oid&0x1f);
		return FALSE;
	}
    FSOBJECT *po = (FSOBJECT *)poo;
	*newvar = &po->urd;
	*lastvar = &po->ucompare;
	return TRUE;
}


BOOL FsWriteInt (int oid, int i32)
{
	DOBJECT* poo = GetValidObject(oid);
    if (poo->type != FSOBJ)
	{
        Error ("%d(%d,%d) is not an fs object", oid, oid>>5, oid&0x1f);

		return FALSE;
	}
    FSOBJECT *po = (FSOBJECT *)poo;
	if (po->o.flags & FLG_DISABLED)
		return TRUE;

	Write(po, i32);
	return TRUE;
}

BOOL FsWriteDbl (int oid, double d)
{
	DOBJECT* poo = GetValidObject(oid);
    if (poo->type != FSOBJ)
	{
        Error ("%d(%d,%d) is not an fs object", oid, oid>>5, oid&0x1f);
		return FALSE;
	}
    FSOBJECT *po = (FSOBJECT *)poo;
	if (po->o.flags & FLG_DISABLED)
		return TRUE;
	po->uwr.dbl = d;
	Write(po);
	return TRUE;
}

BOOL FsWriteInt64 (int oid, __int64 i64)
{
	DOBJECT* poo = GetValidObject(oid);
    if (poo->type != FSOBJ)
	{
        Error ("%d(%d,%d) is not an fs object", oid, oid>>5, oid&0x1f);
		return FALSE;
	}
    FSOBJECT *po = (FSOBJECT *)poo;
	if (po->o.flags & FLG_DISABLED)
		return TRUE;
	po->uwr.i64 = i64;
	Write(po);
	return TRUE;
}

BOOL FsWriteUnion (int oid, UVAR u)
{
	DOBJECT* poo = GetValidObject(oid);
    if (poo->type != FSOBJ)
	{
        Error ("%d is not an fs object", oid);
		return FALSE;
	}
    FSOBJECT *po = (FSOBJECT *)poo;
	if (po->o.flags & FLG_DISABLED)
		return TRUE;
	po->uwr = u;
	Write(po);
	return TRUE;
}

void FsInvalidate()
{
    for (int i=0; i<MAXCONTAINEROBJECTS; i++)
	{
		DOBJECT* po = Objects[i];
        if (!po || (po->type != FSOBJ))
			continue;
	    FSOBJECT *pfo = (FSOBJECT *)po;
        pfo->o.flags |= FLG_INVALID;
	}
}

int GetInt32(FSOBJECT* po)
{
    switch (po->datatype)
    {
    case TP_I8:
        return po->urd.i8;
    case TP_UI8:
        return po->urd.ui8;
    case TP_I16:
        return po->urd.i16;
    case TP_UI16:
        return po->urd.ui16;
    case TP_I32:
        return po->urd.i32;
    case TP_UI32:
        return po->urd.ui32;
    case TP_I64:
        return ((int)(po->urd.i64 >> 32));
    case TP_DBL:
        return (int)po->urd.dbl;
	default:
		return 0;
    }
}

void Write(FSOBJECT* po, int x)
{
    switch (po->datatype)
    {
    case TP_I8:
        po->uwr.i8 = x;
        break;
    case TP_UI8:
        po->uwr.ui8 = x;
        break;
    case TP_I16:
        po->uwr.i16 = x;
        break;
    case TP_UI16:
        po->uwr.ui16 = x;
        break;
    case TP_I32:
        po->uwr.i32 = x;
        break;
    case TP_UI32:
        po->uwr.ui32 = x;
        break;
    case TP_I64:
        po->uwr.i64 = x;
        break;
    case TP_DBL:
        po->uwr.dbl = x;
        break;
    }
    Write(po);
}

BOOL Write(FSOBJECT* po)
{
    if (po->o.flags & FLG_WRITEQUEUED)
        return TRUE;
    if (nwriteitems >= MAXWRQ)
        return FALSE;
    po->o.flags |= FLG_WRITEQUEUED;
    wrq[nwriteitems++] = po;
	return TRUE;
}


BOOL FsConnect()
{
    if (FsuipcOpen() == false)
		return false;

	int v = GetFsbusDLLVersion ();

	char txt[256];
	sprintf_s (txt, sizeof(txt), "FSBUS.DLL %d.%d.%d", v/100, (v%100)/10, v%10 );

	if (CheckLicense() == false)
	{
		strcat_s (txt, sizeof(txt), " - freeware for non-commercial use - www.fsbus.de - D.Anderseck");
		FsWriteDirect (0x3380, strlen(txt), txt);
		short s = 10;
		FsWriteDirect (0x32FA, 2, &s);
		Sleep (1500);
	}
	else
	{
		extern char licinfo[];
		strcat_s (txt, sizeof(txt), " - ");
		strcat_s (txt, sizeof(txt), licinfo);
		strcat_s (txt, sizeof(txt), "          ");
		FsWriteDirect (0x3380, strlen(txt), txt);
		short s = 10;
		FsWriteDirect (0x32FA, 2, &s);
		Sleep (500);
	}
	return true;
}

BOOL FsDisconnect()
{
    FsuipcClose();
    free (QuickPoll);
    free (NormalPoll);
    free (LazyPoll);
	return TRUE;
}

BOOL FsReadDirect(int offset, int sz, void* dest)
{
    FsuipcRead(offset, sz, dest);
    FsuipcProcess();
	return TRUE;
}

BOOL FsWriteDirect(int offset, int sz, void* dest)
{
    FsuipcWrite(offset, sz, dest);
    FsuipcProcess();
	return TRUE;
}

BOOL FsSetPollTiming (int quick, int normal, int lazy)
{
    LazyInterval = lazy;
    NormalInterval = normal;
    QuickInterval = quick;
	return TRUE;
}

FSOBJECT* FsGetNextUpdated()
{
    if (NextUpd >= UpdCount)
    {
        NextUpd = UpdCount = 0;
        return NULL;
    }
    FSOBJECT* po = UpdObjects[NextUpd++];
    po->o.flags &= ~FLG_UPDATE;
    return po;
}

static DWORD        t0, t1;
static DWORD        tCount = 0;

DWORD FsExecute (int max)
{
    static DWORD dwt = 0;

    if (!FSUIPC_OK)
        return 0;

    assert (UpdCount == 0);

    if (NextPoll < PollCount)
    {
        // es sind noch abzuarbeitende Objekte in der Pollliste
        int cnt = PollCount - NextPoll;
        if (max && max < cnt)
            cnt = max;

        DWORD t = GetTickCount();
        if (dwt)
            t0 += t - dwt;
        dwt = t;

        Process (NextPoll, cnt);

        t = GetTickCount();
        t1 += t - dwt;
        tCount++;

        NextPoll += cnt;
        if (NextPoll < PollCount)
            return 0;
            // next Poll can occur immediate
        NextPoll = PollCount = 0;
        DWORD nt = nextquicktick;
        if (nt > nextnormaltick)
            nt = nextnormaltick;
        if (nt > nextlazytick)
            nt = nextlazytick;
        nt -= GetTickCount();
        if (nt <= 0)
            return 0;
        return nt;
    }
	else
	{
	    for (int i=0; i<nwriteitems; i++)
		{
			FSOBJECT* p = wrq[i];
			p->o.flags &= ~FLG_WRITEQUEUED;
			FsuipcWrite(p->offs, p->datasize, &p->uwr);
		}
		if (nwriteitems)
		{
			FsuipcProcess();
			nwriteitems = 0;
		}
	}
    // neue Liste der zu pollenden Objekte erstellen
    DWORD t = GetTickCount();
    if (nextlazytick <= t)
    {
        while (nextlazyidx < LazyPoll->Count && PollCount < MAXPOLL)
        {
            FSOBJECT* po = (FSOBJECT*)LazyPoll->Objects[nextlazyidx++];
            if (po->o.flags & FLG_DISABLED)
                continue;
            PollObjects[PollCount++] = po;
        }
        if (nextlazyidx >= LazyPoll->Count)
        {
            nextlazytick = t + LazyInterval;
            nextlazyidx = 0;
        }
    }
    if (nextnormaltick <= t)
    {
        while (nextnormalidx < NormalPoll->Count && PollCount < MAXPOLL)
        {
            FSOBJECT* po = (FSOBJECT*)NormalPoll->Objects[nextnormalidx++];
            if (po->o.flags & FLG_DISABLED)
                continue;
            PollObjects[PollCount++] = po;
        }
        if (nextnormalidx >= NormalPoll->Count)
        {
            nextnormaltick = t + NormalInterval;
            nextnormalidx = 0;
        }
    }
    if (nextquicktick <= t)
    {
        while (nextquickidx < QuickPoll->Count && PollCount < MAXPOLL)
        {
            FSOBJECT* po = (FSOBJECT*)QuickPoll->Objects[nextquickidx++];
            if (po->o.flags & FLG_DISABLED)
                continue;
            PollObjects[PollCount++] = po;
        }
        if (nextquickidx >= QuickPoll->Count)
        {
            nextquicktick = t + QuickInterval;
            nextquickidx = 0;
        }
    }
    if (PollCount)
        return 0;
    DWORD nt = nextquicktick;
    if (nt > nextnormaltick)
        nt = nextnormaltick;
    if (nt > nextlazytick)
        nt = nextlazytick;
    nt -= GetTickCount();
    if (nt <= 0)
        return 0;
    return nt;
}

//-----------------------------------------------------------------------
void Process(int nextix, int cnt)
{
    FSOBJECT* p;
    int i;

    NextPoll = nextix + cnt;

    for (i=0; i<nwriteitems; i++)
    {
        p = wrq[i];
        p->o.flags &= ~FLG_WRITEQUEUED;
        FsuipcWrite(p->offs, p->datasize, &p->uwr);
    }
    if (nwriteitems)
    {
        FsuipcProcess();
        nwriteitems = 0;
    }

    for (i=nextix; i<NextPoll; i++)
    {
        p = PollObjects[i];
        int sz = p->datasize;
        if (sz > sizeof(UVAR) || sz < 1)
            sz = 4;
        FsuipcRead(p->offs, sz, &p->urd);
    }
    FsuipcProcess();

    for (i=nextix; i<NextPoll; i++)
    {
        p = PollObjects[i];
        if (!(p->o.flags & FLG_UPDATE))
        {
			if (memcmp (&p->urd, &p->ucompare, p->datasize) || (p->o.flags & FLG_INVALID))
            {
                p->ucompare = p->urd;
                p->o.flags |= FLG_UPDATE;
                p->o.flags &= ~FLG_INVALID;
                UpdObjects[UpdCount++] = p;
            }
        }
    }
}

//---------------------------------------------------------------------------
bool FsuipcOpen()
{
    InitializeCriticalSection(&FSUIPC_csect);

	FSUIPC_Version = 0;
	FSUIPC_FS_Version = 0;
	FSUIPC_Lib_Version = LIB_VERSION;

	FSUIPC_m_hWnd = 0;       // FS6 window handle
	FSUIPC_m_msg = 0;        // id of registered window message
	FSUIPC_m_atom = 0;       // global atom containing name of file-mapping object
	FSUIPC_m_hMap = 0;       // handle of file-mapping object
	FSUIPC_m_pView = 0;      // pointer to view of file-mapping object
	FSUIPC_m_pNext = 0;
    FSUIPC_OK = false;
    FSUIPC_ConnectionType = 0;

	DWORD dwFSReq = SIM_FS2k2;
	char szName[MAX_PATH];
	static int nTry = 0;
	BOOL fWideFS = FALSE;
	int i = 0;
    FSUIPC_OK = false;

	// abort if already started
	if (FSUIPC_m_pView)
    {
    	Error ("Open fsuipc error");
		return false;
	}

	// Clear version information, so know when connected
	FSUIPC_Version = FSUIPC_FS_Version = 0;

	// Connect via FSUIPC, which is known to be FSUIPC's own
	// and isn't subject to user modificiation
	FSUIPC_m_hWnd = FindWindowEx(NULL, NULL, "UIPCMAIN", NULL);
    FSUIPC_ConnectionType = CT_FSUIPC;

	if (!FSUIPC_m_hWnd)
    {
    	// If there's no UIPCMAIN, we may be using WideClient
		// which only simulates FS98
		FSUIPC_m_hWnd = FindWindowEx(NULL, NULL, "FS98MAIN", NULL);
		fWideFS = TRUE;
		if (!FSUIPC_m_hWnd)
        {
        	Error ("no Flight Sim found");
			return false;
		}
        FSUIPC_ConnectionType = CT_WIDEFS;
	}
	// register the window message
	FSUIPC_m_msg = RegisterWindowMessage(FS6IPC_MSGNAME1);
	if (FSUIPC_m_msg == 0)
    {
    	Error ("Register Window error");
		return false;
	}

	// create the name of our file-mapping object
	nTry++; // Ensures a unique string is used in case user closes and reopens
	wsprintf(szName, FS6IPC_MSGNAME1 ":%X:%X", GetCurrentProcessId(), nTry);

	// stuff the name into a global atom
	FSUIPC_m_atom = GlobalAddAtom(szName);
	if (FSUIPC_m_atom == 0)
    {
    	Error ("Add Atom error");
		return false;
	}

	// create the file-mapping object
	FSUIPC_m_hMap = CreateFileMapping(
					(HANDLE)0xFFFFFFFF, // use system paging file
					NULL,               // security
					PAGE_READWRITE,     // protection
					0, MAX_SIZE+256,       // size
					szName);            // name

	if ((FSUIPC_m_hMap == 0) || (GetLastError() == ERROR_ALREADY_EXISTS))
    {
    	Error ("create filemap error");
		return false;
	}

	// get a view of the file-mapping object
	FSUIPC_m_pView = (BYTE*)MapViewOfFile(FSUIPC_m_hMap, FILE_MAP_WRITE, 0, 0, 0);
	if (FSUIPC_m_pView == NULL)
    {
    	Error ("map file error");
		return false;
	}

	// Okay, now determine FSUIPC version AND FS type
	FSUIPC_m_pNext = FSUIPC_m_pView;

	// Try up to 5 times with a 100mSec rest between each
	// Note that WideClient returns zeroes initially, whilst waiting
	// for the Server to get the data
	while ((i++ < 5) && ((FSUIPC_Version == 0) || (FSUIPC_FS_Version == 0)))
    {
        // Write license key
		FsuipcWrite(0x8001, 12, FsuipcLicense);

    	// Read FSUIPC version
		if (!FsuipcRead(0x3304, 4, &FSUIPC_Version))
        {
        	FsuipcClose();
			return false;
		}

		// and FS version and validity check pattern
		if (!FsuipcRead(0x3308, 2, &FSUIPC_FS_Version))
        {
        	FsuipcClose();
			return false;
		}

		// Write our Library version number to a special read-only offset
		// This is to assist diagnosis from FSUIPC logging
		// But only do this on first try
		if ((i < 2) && !FsuipcWrite(0x330a, 2, &FSUIPC_Lib_Version))
        {
        	FsuipcClose();
			return false;
		}

		// Actually send the requests and get the responses ("process")
		if (!FsuipcProcess())
        {
        	FsuipcClose();
			return false;
		}

		// Maybe running on WideClient, and need another try
		Sleep(100); // Give it a chance
	}

	// Only allow running on FSUIPC 1.998e or later
	// with correct check pattern 0xFADE
	if (FSUIPC_Version < 0x19980005)
    {
    	if (fWideFS)
       		Error ("fsuipc not running");
        else
        	Error ("fsuipc version error");
		FsuipcClose();
		return false;
	}
    FSUIPC_OK = true;
	return true;
}
//---------------------------------------------------------------------------
bool FsuipcClose()
{
	FSUIPC_m_hWnd = 0;
	FSUIPC_m_msg = 0;

	if (FSUIPC_m_atom)
	{
        GlobalDeleteAtom(FSUIPC_m_atom);
		FSUIPC_m_atom = 0;
	}

	if (FSUIPC_m_pView)
	{
        UnmapViewOfFile((LPVOID)FSUIPC_m_pView);
		FSUIPC_m_pView = 0;
	}

	if (FSUIPC_m_hMap)
	{	CloseHandle(FSUIPC_m_hMap);
		FSUIPC_m_hMap = 0;
	}
    FSUIPC_OK = false;

    DeleteCriticalSection(&FSUIPC_csect);

    return true;
}


//---------------------------------------------------------------------------
bool FsuipcProcess()
{
	DWORD dwError;
	DWORD *pdw;
	FS6IPC_READSTATEDATA_HDR *pHdrR;
	FS6IPC_WRITESTATEDATA_HDR *pHdrW;
	int i = 0;

	if (!FSUIPC_m_pView)
    {
    	Error ("FSUIPC not open");
        FSUIPC_OK = false;
		return false;
	}
	if (FSUIPC_m_pView == FSUIPC_m_pNext)
    {
		return true;
	}
    EnterCriticalSection(&FSUIPC_csect);

	ZeroMemory(FSUIPC_m_pNext, 4); // Terminator
	FSUIPC_m_pNext = FSUIPC_m_pView;

	// send the request (allow up to 9 tries)
	while ((++i < 10) && !SendMessageTimeout(
			FSUIPC_m_hWnd,       // FS6 window handle
			FSUIPC_m_msg,        // our registered message id
			FSUIPC_m_atom,       // wParam: name of file-mapping object
			0,            // lParam: offset of request into file-mapping obj
			SMTO_BLOCK,   // halt this thread until we get a response
			2000,			  // time out interval
			&dwError))    // return value
	{
    	Sleep(100); // Allow for things to happen
	}

	if (i >= 10)
    {  // Failed all tries?
        LeaveCriticalSection(&FSUIPC_csect);
    	if (GetLastError() == 0)
        	Error("fsuipc timeout");
        else
        	Error ("fsuipc Send Message Error");
        FSUIPC_OK = false;
		return false;
	}
	if (dwError != FS6IPC_MESSAGE_SUCCESS)
    {
        LeaveCriticalSection(&FSUIPC_csect);
    	Error ("fsuipc Data error");
        // FSUIPC didn't like something in the data!
        FSUIPC_OK = false;
		return false;
	}
	// Decode and store results of Read requests
	pdw = (DWORD *) FSUIPC_m_pView;

	while (*pdw)
    {
    	switch (*pdw)
        {
        case FS6IPC_READSTATEDATA_ID:
			pHdrR = (FS6IPC_READSTATEDATA_HDR *) pdw;
			FSUIPC_m_pNext += sizeof(FS6IPC_READSTATEDATA_HDR);
			if (pHdrR->pDest && pHdrR->nBytes)
				CopyMemory(pHdrR->pDest, FSUIPC_m_pNext, pHdrR->nBytes);
			FSUIPC_m_pNext += pHdrR->nBytes;
			break;
		case FS6IPC_WRITESTATEDATA_ID:
			// This is a write, so there's no returned data to store
			pHdrW = (FS6IPC_WRITESTATEDATA_HDR *) pdw;
			FSUIPC_m_pNext += sizeof(FS6IPC_WRITESTATEDATA_HDR) + pHdrW->nBytes;
			break;
		default:
			// Error! So terminate the scan
			*pdw = 0;
			break;
		}
		pdw = (DWORD *) FSUIPC_m_pNext;
	}

	FSUIPC_m_pNext = FSUIPC_m_pView;
    LeaveCriticalSection(&FSUIPC_csect);
	return true;
}

bool FsuipcProcessBuffer()
{
	DWORD dwError;

//	DWORD *pdw;
	int i = 0;

	// send the request (allow up to 9 tries)
	while ((++i < 10) && !SendMessageTimeout(
			FSUIPC_m_hWnd,       // FS6 window handle
			FSUIPC_m_msg,        // our registered message id
			FSUIPC_m_atom,       // wParam: name of file-mapping object
			0,            // lParam: offset of request into file-mapping obj
			SMTO_BLOCK,   // halt this thread until we get a response
			2000,			  // time out interval
			&dwError))    // return value
	{
    	Sleep(100); // Allow for things to happen
	}

	if (i >= 10)
    {  // Failed all tries?
    	if (GetLastError() == 0)
        	Error ("fsuipc timeout");
        else
        	Error ("fsuipc Send Message Error");
        FSUIPC_OK = false;
		return false;
	}
	if (dwError != FS6IPC_MESSAGE_SUCCESS)
    {
    	Error ("fsuipc Data error"); // FSUIPC didn't like something in the data!
        FSUIPC_OK = false;
		return false;
	}
	FSUIPC_m_pNext = FSUIPC_m_pView;
	return true;
}

//---------------------------------------------------------------------------
bool FsuipcRead(DWORD dwOffset, DWORD dwSize, void *pDest)
{
	FS6IPC_READSTATEDATA_HDR *pHdr = (FS6IPC_READSTATEDATA_HDR *) FSUIPC_m_pNext;

	// Check link is open
	if (!FSUIPC_m_pView)
    {
    	Error ("fsuipc not open");
        FSUIPC_OK = false;
		return false;
	}
    EnterCriticalSection(&FSUIPC_csect);
	// Check have space for this request (including terminator)
	if (((FSUIPC_m_pNext - FSUIPC_m_pView) + 4 + (dwSize + sizeof(FS6IPC_READSTATEDATA_HDR))) > MAX_SIZE)
	{
    	Error ("fsuipc request size error");
        LeaveCriticalSection(&FSUIPC_csect);
		return false;
	}

	// Initialise header for read request
	pHdr->dwId = FS6IPC_READSTATEDATA_ID;
	pHdr->dwOffset = dwOffset;
	pHdr->nBytes = dwSize;
	pHdr->pDest = (BYTE *) pDest;

	// Zero the reception area, so rubbish won't be returned
	if (dwSize) ZeroMemory(&FSUIPC_m_pNext[sizeof(FS6IPC_READSTATEDATA_HDR)], dwSize);

	// Update the pointer ready for more data
	FSUIPC_m_pNext += sizeof(FS6IPC_READSTATEDATA_HDR) + dwSize;
    LeaveCriticalSection(&FSUIPC_csect);
	return true;
}

//---------------------------------------------------------------------------
bool FsuipcWrite(DWORD dwOffset, DWORD dwSize, void *pSrce)
{
	FS6IPC_WRITESTATEDATA_HDR *pHdr = (FS6IPC_WRITESTATEDATA_HDR *) FSUIPC_m_pNext;

	// check link is open
	if (!FSUIPC_m_pView)
    {
    	Error ("fsuipc not open");
        FSUIPC_OK = false;
        return false;
	}
    EnterCriticalSection(&FSUIPC_csect);
	// Check have spce for this request (including terminator)
	if (((FSUIPC_m_pNext - FSUIPC_m_pView) + 4 + (dwSize + sizeof(FS6IPC_WRITESTATEDATA_HDR))) > MAX_SIZE)
	{
        LeaveCriticalSection(&FSUIPC_csect);
    	Error ("fsuipc request size error");
		return false;
	}

	// Initialise header for write request
	pHdr->dwId = FS6IPC_WRITESTATEDATA_ID;
	pHdr->dwOffset = dwOffset;
	pHdr->nBytes = dwSize;

	// Copy in the data to be written
	if (dwSize)
    	CopyMemory(&FSUIPC_m_pNext[sizeof(FS6IPC_WRITESTATEDATA_HDR)], pSrce, dwSize);

	// Update the pointer ready for more data
	FSUIPC_m_pNext += sizeof(FS6IPC_WRITESTATEDATA_HDR) + dwSize;
    LeaveCriticalSection(&FSUIPC_csect);

	return true;
}
//---------------------------------------------------------------------------

//#pragma package(smart_init)

