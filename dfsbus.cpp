//---------------------------------------------------------------------------
#include "stdafx.h"
#include "fsbusdll.h"

#define MAXRID  80
#define MAXCID  99

static FSBUSOBJECT*		itemmap[MAXCID+1][MAXRID];
static HANDLE			hCom = NULL;
static int				seqno = -1;
static MEMORYSTREAM*	pmWrqInUse;
static MEMORYSTREAM*	pmWrq;
HANDLE                  hWriteCompleteEvent;
HANDLE                  hReadEvent;
OVERLAPPED              ovlrd;
OVERLAPPED              ovlwr;
unsigned char           rdbuf[128];

static int				cid;
static int				rid;
static int				val;
static FSBUSOBJECT*		pRdObject = NULL;
bool					bTransmitFsbusState = false;

/*
 * InterpolateCurve is a method of constructing a new data point
 * within the range of a discrete set of known data points.
 * The input value of a table entry must be sorted ascending while
 * the output value in the table slot is independent of the function.
 *
 * Example 1:
 *     the input values from 10-250 are interpolated to 0-16000
 *     CALTAB t[] = { {10, 16000},{ 250, 0} };
 *     int x = Calibrate (50, t, sizeof(t)/sizeof(CALTAB));
 *
 * Example 2:
 *     the input values from 10-220 are interpolated to 0-16000. A nonlinear
 *     curve is described by the input values in the table.
 *
 *     CALTAB t[] = { {10, 0}, {40, 3000}, {70, 5800}, {100, 8500}, {130, 11000},
 *               {160, 14000}, {190, 15000}, { 220, 16000} };
 *     int x = Calibrate (86, t, sizeof(t)/sizeof(CALTAB));
 *
 */

int  Calibrate (int val, CALTAB* t, int count)
{
    int i=0;
    for (; i<count; i++)
        if (val <= t[i].ival)
            break;
    if (i)
    {
        if (i >= count)
           return t[count-1].oval;
        i--;
    }
    else
        return t[0].oval;
    int idiff = t[i+1].ival - t[i].ival;
    int odiff = t[i+1].oval - t[i].oval;
    int res = (val-t[i].ival) * odiff / idiff;
    res += t[i].oval;
    return res;
}


//-------------------------------------------------------------------
void CreateFsbus()
{
    hCom = NULL;
    seqno = -1;
    memset (itemmap, 0, sizeof(itemmap));
    pmWrqInUse = MemoryStreamCreate();
    pmWrq = MemoryStreamCreate();
    hWriteCompleteEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
    hReadEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
}

//-------------------------------------------------------------------
void DestroyFsbus()
{
    FsbusClose();
    MemoryStreamDestroy(pmWrqInUse);
    MemoryStreamDestroy(pmWrq);
    CloseHandle(hWriteCompleteEvent);
    CloseHandle(hReadEvent);
}

//-------------------------------------------------------------------
BOOL FsbusOpen (LPSTR dev)
{
    if (hCom)
	{
        Error ("com interface still opened");
		return FALSE;
	}
	char name[256];
	sprintf (name, "\\\\.\\%s", dev);
	hCom = CreateFile(name,
                GENERIC_READ|GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                NULL);

    if (hCom == INVALID_HANDLE_VALUE)
	{
		DWORD dw = GetLastError();
        Error ("cannot open \"%s\" (%d)", dev, dw);
		return FALSE;
	}
    DCB dcb;
    dcb.DCBlength = sizeof(dcb);
    GetCommState( hCom, &dcb);
    dcb.BaudRate = CBR_19200;  	    // current baud rate
    dcb.ByteSize = 8;         		// number of bits/byte, 4-8
    dcb.Parity = NOPARITY; 			//0;           // 0-4=no,odd,even,mark,space
    dcb.StopBits =  0;  			//0;         // 0,1,2 = 1, 1.5, 2
    dcb.fBinary = 1;         	// binary mode, no EOF check
    dcb.fParity = FALSE;        	// enable parity checking
    dcb.fOutxCtsFlow = FALSE;   	// CTS output flow control
    dcb.fOutxDsrFlow = FALSE;   	// DSR output flow control
    dcb.fRtsControl = RTS_CONTROL_DISABLE;    // RTS flow control
    dcb.fDtrControl = DTR_CONTROL_DISABLE; // DTR flow control type
    dcb.fDsrSensitivity = FALSE;   	// DSR sensitivity
    dcb.fTXContinueOnXoff = FALSE; 	// XOFF continues Tx
    dcb.fOutX = FALSE;        		// XON/XOFF out flow control
    dcb.fInX = FALSE;         		// XON/XOFF in flow control
    dcb.fErrorChar = FALSE;   		// enable error replacement
    dcb.fNull = FALSE;        		// enable null stripping
    dcb.fAbortOnError = FALSE; 		// abort reads/writes on error

    if (SetCommState( hCom, &dcb) == FALSE)
    {
        CloseHandle(hCom);
        hCom = NULL;
        Error ("cannot setup \"%s\" interface", dev);
		return FALSE;
    }
    //EscapeCommFunction (hCom, SETRTS);  // 12V auf Pin 7
    //EscapeCommFunction (hCom, CLRDTR);  // -12V auf Pin 4

    COMMTIMEOUTS cto;
    cto.ReadIntervalTimeout = 0;
    cto.ReadTotalTimeoutMultiplier = 0;    // Wert * Anz Byte ergibt to
    cto.ReadTotalTimeoutConstant = 1;
    cto.WriteTotalTimeoutMultiplier = 0;
    cto.WriteTotalTimeoutConstant = 0;
    if (SetCommTimeouts(hCom, &cto) == FALSE)
    {
        CloseHandle(hCom);
        hCom = NULL;
        Error ("cannot setup \"%s\" timeouts", dev);
		return FALSE;
    }

    // start async read
    ovlrd.hEvent = hReadEvent;
    DWORD dw;
    if (!ReadFile (hCom, rdbuf, sizeof(rdbuf), &dw, &ovlrd))
        if (GetLastError() != ERROR_IO_PENDING)
        {
            CloseHandle(hCom);
            hCom = NULL;
            Error ("cannot read asynchron on \"%s\" ", dev);
			return FALSE;
        }
	return TRUE;
}

//-------------------------------------------------------------------
BOOL FsbusClose()
{
    if (hCom)
        CloseHandle(hCom);
    hCom = NULL;
	return TRUE;
}

//-------------------------------------------------------------------
BOOL FsbusWriteRaw(unsigned char* buf, int count)
{
    if (!hCom)
	{
        //Error("fsbus interface down");
		return FALSE;
	}
    DWORD dw;
    if (pmWrqInUse->Size == 0)
    {
        MemoryStreamWrite (pmWrqInUse, (char*)buf, count);
        ovlwr.hEvent = hWriteCompleteEvent;
        if (!WriteFile(hCom, pmWrqInUse->Memory, pmWrqInUse->Size, &dw, &ovlwr))
            if (GetLastError() != ERROR_IO_PENDING)
			{
                Error ("fsbus write error");
				return FALSE;
			}
    }
    else
    {
        // write still active, copy to backbuffer
        MemoryStreamWrite (pmWrq, (char*)buf, count);
    }
	return TRUE;
}
//-------------------------------------------------------------------
// wenn cid > 30, wird automatisch ein Byte1 eingefügt
BOOL _FsbusWriteExt(int cid, unsigned char* buf, int count)
{
	if (cid < 31)
		return FsbusWriteRaw(buf, count);

	unsigned char nbuf[32];
	nbuf[0] = buf[0] | 0x7c;
	nbuf[1] = cid - 31;
	for (int i=1; i<count; i++)
		nbuf[i+1] = buf[i];
	return FsbusWriteRaw(nbuf, count+1);
}


//-------------------------------------------------------------------
BOOL FsbusWriteFmt2(int cid, int rid, int val)
{
    unsigned char buf[3];
	int i = 1;
	int cid0 = cid;
	int cid1;

	if (cid > 30)
	{
		cid0 = 31;
		cid1 = cid - 31;
	}
    buf[0] = 0x80 | (cid0 << 2) | (val&0x01) | ((rid&0x80)>>6);
	if (cid0 == 31)
	    buf[i++] = cid1;
    buf[i++] = rid & 0x7f;
    return FsbusWriteRaw (buf, i);
}


//-------------------------------------------------------------------
BOOL FsbusWriteFmt3(int cid, int rid, int val)
{
    unsigned char buf[4];
	int i = 1;
	int cid0 = cid;
	int cid1;

	if (cid > 30)
	{
		cid0 = 31;
		cid1 = cid - 31;
	}
	buf[0] = 0x80 | (cid0 << 2) | (val&0x01) | ((rid&0x80)>>6);
	if (cid0 == 31)
	    buf[i++] = cid1;
    buf[i++] = rid & 0x7f;
    buf[i++] = (val & 0xfe) >> 1;
    return FsbusWriteRaw (buf, i);
}

//-------------------------------------------------------------------
BOOL FsbusWriteFmtVar(int cid, int rid, int v)
{
    unsigned char buf[32];
	int i = 1;
	int cid0 = cid;
	int cid1;

	if (cid > 30)
	{
		cid0 = 31;
		cid1 = cid - 31;
	}
	buf[0] = 0x80 | (cid0 << 2) | (v&0x01) | ((rid&0x80)>>6);
	if (cid0 == 31)
	    buf[i++] = cid1;
    v = v >> 1;
    buf[i++] = rid & 0x7f;
    for (;;)
    {
        buf[i] = v & 0x3f;
        v = v >> 6;
        if (v == 0)
        {
            buf[i++] |= 0x40;
            break;
        }
		i++;
    }
    return  FsbusWriteRaw(buf, i);
}

//-----------------------------------------------------
void OnAsyncWriteComplete()
{
    DWORD dw;
    MemoryStreamClear(pmWrqInUse);

    if (pmWrq->Size)
    {
        MEMORYSTREAM* pm = pmWrq;
        pmWrq = pmWrqInUse;
        pmWrqInUse = pm;
        ovlwr.hEvent = hWriteCompleteEvent;
        if (!WriteFile(hCom, pmWrqInUse->Memory, pmWrqInUse->Size, &dw, &ovlwr))
            if (GetLastError() != ERROR_IO_PENDING)
                Error ("fsbus write error");
    }
}

//-------------------------------------------------------------------
void OnAsyncRead()
{
    DWORD n;
    if (GetOverlappedResult(hReadEvent, &ovlrd, &n, FALSE) == TRUE)
        for (DWORD i=0; i<n; i++)
            OnChar (rdbuf[i]);
    else
	{
        Error ("GetOverlappedResult failed");
		return;
	}

    // das nächste Lesen starten
    ovlrd.hEvent = hReadEvent;
    DWORD dw;
    if (!ReadFile (hCom, rdbuf, sizeof(rdbuf), &dw, &ovlrd))
    {
        if (GetLastError() != ERROR_IO_PENDING)
        {
            Error ("cannot read com ");
			return;
        }
    }
}

// Zeichen von async read einzeln dekodieren
// dekodierte Objekte werden mit dem Datum gefüllt und
// dann die Event Funktion aufgerufen

void OnChar(unsigned char b)
{
    if (b & 0x80)
    {
        seqno = 0;
        cid = (b >> 2) & 0x1f;
        rid = (b & 0x02) << 6;
        val = b & 0x01;
        pRdObject = NULL;
        return;
    }
    switch (++seqno)
    {
    case 1:
        rid |= b;
        if (rid < 0 || rid >= MAXRID)
            return;
        if (cid < 0 || cid >= MAXCID)
            return;
        pRdObject = itemmap[cid][rid];    //Merge the data coming from PCB to the created object according to cid and rid
        break;

    case 2:
        val |= (b << 1);
        if (!pRdObject)
            break;
        break;

    case 3: 
        val |= (b << 8);
        break;
    }

    if (!(pRdObject->o.flags & FLG_DISABLED))
        OnFsbusInput (pRdObject, val);
}

//-----------------------------------------------------
BOOL Test (int id, char* name, int cid, int rid)
{
    if (id <= 0 || id >= MAXCONTAINEROBJECTS)
    {
        Error ("Object %s (%d) out of range", name, id);
        return FALSE;
    }
    if (Objects[id])
    {
        Error("Object %s (%d) still defined", name, id);
        return FALSE;
    }
    if (cid < 0 || cid > MAXCID)
    {
        Error ("Object %s (%d) CID:%d out of range",name, id, cid);
        return FALSE;
    }
    if (rid < 0 || rid >= MAXRID)
    {
        Error ("Object %s (%d) RID:%d out of range",name, id, rid);
        return FALSE;
    }
	return TRUE;
}

//-----------------------------------------------------
BOOL Test (int id, char* name, int cid)
{
    if (id <= 0 || id >= MAXCONTAINEROBJECTS)
    {
        Error ("Object %s (%d) out of range", name, id);
        return FALSE;
    }
    if (Objects[id])
    {
        Error("Object %s (%d) still defined", name, id);
        return FALSE;
    }
    if (cid < 0 || cid > MAXCID)
    {
        Error ("Object %s (%d) CID:%d out of range", name, id, cid);
        return FALSE;
    }
	return TRUE;
}

BOOL TestCR (int id, char* name, int cid, int rid)
{
    FSBUSOBJECT* po = itemmap[cid][rid];
    if (po)
    {
        Error ("Object %s (%d) CID:%d RID:%d occupied by another object",name, id, cid, rid);
		return FALSE;
	}
	return TRUE;
}

void FsbusInvalidate()
{
	bTransmitFsbusState = true;
	// the next Poll function will send all buttons and analog values to application 
}

BOOL MkFsbusObject(FSBUSTYPE ftp, int oid,char* nm,void(* cb)(int oid, int val, double dbl), int c, int r, int flg)
{
    if (!NewObjectIdOk(oid))
		return FALSE;
    FSBUSOBJECT* po = (FSBUSOBJECT*) malloc (sizeof(FSBUSOBJECT));

    po->o.id    = oid;
    po->o.type  = FSBUSOBJ;
    po->o.flags = flg & (FLG_DISABLED);
    po->o.tag = 0;
    po->o.u.i64 = 0;
	po->o.cb = cb;
	po->o.name = "";
	if (nm)
		po->o.name = _strdup(nm);

    po->fsbustype = ftp;
    po->cid = c;
    po->rid = r;
    po->LastCommaPos = -1;
    po->CommaIndex = 0;
    po->segcount = 6;
    po->segoffs = 0;
    po->bLeadzero = false;
	po->uoff.i32 = 0;
	po->bUsePowerOffValue = false;
	po->PowerTrigger = 0;

    switch (ftp)
    {
    case BTP_D_IN:
    case BTP_ROTARY:
    case BTP_A_IN:
        Test(oid,po->o.name,c,r);
        TestCR(oid,po->o.name,c,r);
        itemmap[c][r] = po;
        break;
    case BTP_D_OUT:
        Test(oid,po->o.name,c);
	    if (r < 0 || r > 255)
		{
			Error ("Object %s (%d) RID:%d out of range",po->o.name, oid, rid);
			return FALSE;
		}
        break;

	case BTP_DISPLAY:
        Test(oid,po->o.name,c,r);
        break;
    case BTP_A_OUT:
    case BTP_V_OUT:
        Test(oid,po->o.name,c);
	    if (r < 80 || r >= 88)
		{
			Error ("Object %s (%d) RID:%d out of range(80-87)",po->o.name, oid, rid);
			return FALSE;
		}
        break;
    }
    Objects[oid] = (DOBJECT*)po;
	return TRUE;
}

void FsbusPowerOffOptions (int oid, int pwrtype, int offval)
{
    DOBJECT* poo = GetValidObject(oid);
    if (poo->type != FSBUSOBJ)
	{
        Error ("%d is not an fsbus object", oid);
		return;
	}
    FSBUSOBJECT *po = (FSBUSOBJECT*)poo;
	po->uoff.i32 = offval;
	po->bUsePowerOffValue = true;
	po->PowerTrigger = pwrtype;
}

void SetPower (int pwrtype, int val)
{
    for (int i=0; i<MAXCONTAINEROBJECTS; i++)
	{
		DOBJECT* po = Objects[i];
        if (!po || (po->type != FSBUSOBJ))
			continue;
	    FSBUSOBJECT *pfsb = (FSBUSOBJECT *)po;
		if ((pfsb->PowerTrigger != pwrtype) || (pfsb->bUsePowerOffValue == false))
			continue;
		if (val==0)
			po->flags |= FLG_PWROFF;
		else
			po->flags &= ~FLG_PWROFF;

	    switch (pfsb->fsbustype)
		{
	    case BTP_D_OUT:
			WriteDigiOut(pfsb, po->u.i32);
			break;
		case BTP_DISPLAY:
			WriteDisplay(pfsb, po->u.i32);
			break;
		case BTP_A_OUT:
			WriteAnalogue(pfsb, po->u.i32);
			break;
		case BTP_V_OUT:
			WriteVarLength(pfsb, po->u.i32);
			break;
		}
	}
}

BOOL DisplayOptions (int oid, int SegCount, int SegOffs, bool LeadZero, int DecPoint)
{
    DOBJECT* poo = GetValidObject(oid);
    if (poo->type != FSBUSOBJ)
	{
        Error ("%d is not an fsbus object", oid);
		return FALSE;
	}
    FSBUSOBJECT *po = (FSBUSOBJECT *)poo;
    if (po->fsbustype != BTP_DISPLAY)
	{
        Error ("%d is not a display", oid);
		return FALSE;
	}
    po->segcount = SegCount;
    po->segoffs = SegOffs;
    po->bLeadzero = LeadZero;
    po->LastCommaPos = -1;
    po->CommaIndex = DecPoint;
    if (!WriteDisplay(po, po->o.u.i32))
		return FALSE;
	return TRUE;
}

void OnFsbusInput (FSBUSOBJECT *po, int v)
{
    switch (po->fsbustype)
    {
    case BTP_D_IN:
        OnDigiIn(po, v);
        break;
    case BTP_ROTARY:
        OnRotaryInput(po, v);
        break;
    case BTP_A_IN:
        OnAnalogueIn(po, v);
        break;
    }
}

void OnDigiIn(FSBUSOBJECT *po, int val)
{
    po->o.u.i32 = val;
    if (po->o.cb && ((po->o.flags & FLG_DISABLED) == 0))
        (*po->o.cb)(po->o.id, val, 0.0);
}

void OnAnalogueIn(FSBUSOBJECT *po, int val)
{
    po->o.u.i32 = val;
    if (po->o.cb && ((po->o.flags & FLG_DISABLED) == 0))
        (*po->o.cb)(po->o.id, val, 0.0);
}

void OnRotaryInput(FSBUSOBJECT *po, int val)
{
    char c = val & 0xff;
    po->o.u.i32 = c;
    if (po->o.cb && ((po->o.flags & FLG_DISABLED) == 0))
        (*po->o.cb)(po->o.id, c, 0.0);
}

BOOL FsbusWrite (int oid, int v)
{
    DOBJECT* poo = GetValidObject(oid);
	if (poo->flags & FLG_DISABLED)
		return TRUE;
	if (poo->type != FSBUSOBJ)
	{
        Error ("%d is not an fsbus object", oid);
		return FALSE;
	}
    FSBUSOBJECT *po = (FSBUSOBJECT*)poo;

    switch (po->fsbustype)
    {
    case BTP_D_IN:
    case BTP_ROTARY:
    case BTP_A_IN:
        Error ("%d has nothing to write to", oid);
		return FALSE;        
		break;
    case BTP_D_OUT:
		poo->u.i32 = v;
		WriteDigiOut(po, v);
        break;
    case BTP_DISPLAY:
		poo->u.i32 = v;
        WriteDisplay(po, v);
        break;
    case BTP_A_OUT:
		poo->u.i32 = v;
		WriteAnalogue(po, v);
        break;
    case BTP_V_OUT:
		poo->u.i32 = v;

		WriteVarLength(po, v);
        break;
    }
	return TRUE;
}

BOOL WriteDisplay(FSBUSOBJECT* po, int v)
{
    unsigned char buf[10];
    int i[6];
    int x;
#define SEG_MINUS   0x0a
#define SEG_S       0x0b
#define SEG_t       0x0c
#define SEG_d       0x0d
#define SEG_E       0x0e
#define SEG_SPC     0x0f

	if (po->o.flags & FLG_PWROFF)
		v = po->uoff.i32;
	
	int seg_empty = po->bLeadzero ? 0 : SEG_SPC;
    bool bminus = false;
    int sz = po->segcount;

    switch (v)
    {
    case DISPLAY_BLANK:     // -10000:    "     "
        for (x=0; x<6; x++)
            i[x] = SEG_SPC;
        break;
    case DISPLAY_BAR:       // -10001:    "-----"
        for (x=0; x<6; x++)
            i[x] = SEG_MINUS;
        break;
    case DISPLAY_STD:       // -10002:    " Std "
        i[0] = SEG_SPC;
        i[1] = SEG_SPC;
        i[2] = SEG_S;
        i[3] = SEG_t;
        i[4] = SEG_d;
        i[5] = SEG_SPC;
        break;
    default:
		// negative values (altitude, vs) change to absolute value
		// and bminus flag is set
        if (v < 0)
        {
            v = abs(v);
            bminus = true;
        }
	    // v von r nach l kopieren
	    x = 5;
		if (v == 0)
			i[x--] = 0;
		for (;  x>=0 && sz; x--, sz--)
		{
			if (v)
			{
				i[x] = v % 10;
				v = v / 10;
			}
			else
			{
				if (bminus)
				{
					i[x] = SEG_MINUS;
					bminus = false;
					seg_empty = SEG_SPC;
				}
				else
					i[x] = seg_empty;
			}
		}
		for (;  x>=0; x--)
			i[x] = SEG_SPC;
		break;
	}

    // i Buffer um Offset nach links schieben
    if (po->segoffs)
    {
        for (x=0; (x+po->segoffs) < 6; x++)
            i[x] = i[x+po->segoffs];
        for (; x < 6; x++)
            i[x] = SEG_SPC;
    }

    // Sendebuffer aus i[] generieren
    buf[0] = 0x80 | ((po->cid & 0x1f) << 2);
    buf[1] = ((i[5] & 0x0c) << 2) | i[4];
    buf[2] = ((i[5] & 0x03) << 4) | i[3];
    buf[3] = ((i[2] & 0x0c) << 2) | i[1];
    buf[4] = 0x40 | ((i[2] & 0x03) << 4) | i[0];

    if (!_FsbusWriteExt(po->cid, (unsigned char*)buf, 5))
		return FALSE;

    if (po->LastCommaPos != po->CommaIndex)
    {
        po->LastCommaPos = po->CommaIndex;
        buf[0] = 0x82 | ((po->cid & 0x1f) << 2) | (po->CommaIndex & 0x01);
        buf[1] = 132 & 0x7f;
        buf[2] = po->CommaIndex >> 1;
        if (!_FsbusWriteExt(po->cid, (unsigned char*)buf, 3))
			return FALSE;
    }
	return TRUE;
}


void WriteDigiOut(FSBUSOBJECT* po, int i32)
{
    // RID von 0-63
    int x = po->rid / 8;
    int r = po->rid % 8;
    int v = i32 ? 1 : 0;

    switch (x)
    {
    case 0: x = 88 + r; break;
    case 1: x = 96 + r; break;
    case 2: x = 104 + r; break;
    case 3: x = 112 + r; break;
    case 4: x = 200 + r; break;
    case 5: x = 208 + r; break;
    case 6: x = 216 + r; break;
    case 7: x = 224 + r; break;
    }

	if (po->o.flags & FLG_PWROFF)
		v = po->uoff.i32;

	FsbusWriteFmt2 (po->cid, x, v);
}

void WriteAnalogue(FSBUSOBJECT* po, int i32)
{
	if (po->o.flags & FLG_PWROFF)
		i32 = po->uoff.i32;
    FsbusWriteFmt3 (po->cid, po->rid, i32);
}

void WriteVarLength(FSBUSOBJECT* po, int i32)
{
	if (po->o.flags & FLG_PWROFF)
		i32 = po->uoff.i32;

    FsbusWriteFmtVar (po->cid, po->rid, i32);
}

//---------------------------------------------------------------------------
//#pragma package(smart_init)
