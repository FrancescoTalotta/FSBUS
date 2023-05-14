//---------------------------------------------------------------------------
#include "stdafx.h"
#include "fsbusdll.h"
#pragma hdrstop

static STRINGLIST* timers = NULL;

void CreateTimer()
{
	timers = SLCreate();
}

void DestroyTimer()
{
	SLDestroy (timers);
    timers = NULL;
}

BOOL MkTimer (int oid, char* nm, void(* cb)(int oid, int val, double dbl), DWORD tm, int flg)
{
    TIMEROBJECT* po = CreateTimerObject(oid, tm, flg);
	if (!po)
		return FALSE;
    Objects[oid] = (DOBJECT*)po;
	po->o.name = "";
	if (nm)
		po->o.name = _strdup(nm);
	po->o.cb = cb;

    SLAddObject(timers, "", po);
	return TRUE;
}

DWORD NextTimerEvent()
{
    DWORD dw = 0xffffffff;
    for (int i=0; i<timers->Count; i++)
    {
        TIMEROBJECT* po = (TIMEROBJECT*)timers->Objects[i];
        DWORD n = po->nextevent;
        if (n && n < dw)
            dw = n;
    }
    return dw;
}

TIMEROBJECT* GetTimerObject()
{
    DWORD dw = GetTickCount();
    for (int i=0; i<timers->Count; i++)
    {
        TIMEROBJECT* po = (TIMEROBJECT*)timers->Objects[i];
        DWORD n = po->nextevent;
        if (n && n <= dw)
        {
            if (po->o.flags & FLG_ONESHOT)
                Disable(po->o.id);
            else
                Enable(po->o.id);
            return po;
        }
    }
    return NULL;
}
//-------------------------------------------------
//-------------------------------------------------
//-------------------------------------------------

TIMEROBJECT* CreateTimerObject(int oid, DWORD tm, int f)
{
    NewObjectIdOk(oid);
    TIMEROBJECT* po = (TIMEROBJECT*) malloc (sizeof(TIMEROBJECT));
    Objects[oid] = (DOBJECT*)po;

    po->o.id    = oid;
    po->o.type  = TIMEROBJ;
    po->o.flags = f ;
    po->o.tag = 0;

    SetTime(oid, tm);
    return po;

}

void DestroyTimerObject()
{
}

void SetTime(int oid, DWORD tm)
{
    DOBJECT* poo = GetValidObject(oid);
    if (poo->type != TIMEROBJ)
    {
        Error("object id is not a timer");
        return;
    }
    TIMEROBJECT *po = (TIMEROBJECT *)poo;

    po->time = tm;
    if ((po->o.flags & FLG_DISABLED) || (tm == 0))
        po->nextevent = 0;
    else
        po->nextevent = GetTickCount() + tm;
}

//---------------------------------------------------------------------------

//#pragma package(smart_init)
