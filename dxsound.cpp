//---------------------------------------------------------------------------
#include "stdafx.h"
#include "fsbusdll.h"
#pragma hdrstop

static LPDIRECTSOUND       lpds = NULL;
static LPDIRECTSOUNDBUFFER dsbPrimary = NULL;
static HINSTANCE           hdxlib = NULL;

typedef HRESULT (WINAPI *pDirectSoundCreate)(LPCGUID , LPDIRECTSOUND* , LPUNKNOWN);
pDirectSoundCreate __DirectSoundCreate = NULL;


bool CreateSound()
{
    HRESULT hr;
    lpds = NULL;
    dsbPrimary = NULL;

	hdxlib = LoadLibrary( "dsound.dll");
    if (!hdxlib)
	{
        Error ("dsound.dll not available\r\n");
        return false;
    }

	__DirectSoundCreate = reinterpret_cast <pDirectSoundCreate>(GetProcAddress (hdxlib, "DirectSoundCreate"));
    if (__DirectSoundCreate == NULL) 
	{
	    Error ("DirectSoundCreate not found in dsound.dll\r\n");
        return false;
    }

	hr = __DirectSoundCreate ((LPCGUID)NULL, &lpds, (LPUNKNOWN)NULL);

    if (hr != DS_OK)
    {
        Error ("DirectSound create error: %s", GetSoundErrtext(hr));
        return false;
    }

    HWND hwnd = GetTopWindow(NULL);
    hr = lpds->SetCooperativeLevel (hwnd,DSSCL_NORMAL );//DSSCL_PRIORITY);

	if (hr != DS_OK)
    {
        lpds->Release();
		lpds = NULL;
        Error ("DirectSound SetCo error: %s", GetSoundErrtext(hr));
        return false;
    }
    DSBUFFERDESC bufdesc;
    memset (&bufdesc, 0, sizeof(DSBUFFERDESC));
    bufdesc.dwSize = sizeof(DSBUFFERDESC);
    bufdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    bufdesc.dwBufferBytes = 0;
    bufdesc.lpwfxFormat = NULL;
	hr = lpds->CreateSoundBuffer (&bufdesc, &dsbPrimary, NULL);

	if(hr != DS_OK)
    {
        lpds->Release();
        lpds = NULL;
        dsbPrimary = NULL;
        Error ("DirectSound create soundbuffer error: %s", GetSoundErrtext(hr));
        return false;
    }
    return true;
}

void DestroySound()
{
    if (dsbPrimary)
        dsbPrimary->Release();
    if (lpds)
        lpds->Release();
}

BOOL MkSound (int oid, char* nm, void(* cb)(int oid, int val, double dbl), char* soundfile)
{
    SOUNDOBJECT* po = CreateSoundObject(oid, 0);
	if (!po)
		return FALSE;
	po->o.cb = cb;
	po->o.name = "";
	if (nm)
		po->o.name = _strdup(nm);
    Objects[oid] = (DOBJECT*)po;
    if (!Load(oid, soundfile))
		return FALSE;
	return TRUE;

}

BOOL MkLoopSound (int oid, char* nm, void(* cb)(int oid, int val, double dbl), char* soundfile)
{
    SOUNDOBJECT* po = CreateSoundObject(oid, FLG_LOOP);
	if (!po)
		return FALSE;
	po->o.cb = cb;
	po->o.name = "";
	if (nm)
		po->o.name = _strdup(nm);
    Objects[oid] = (DOBJECT*)po;
    if (!Load(oid, soundfile))
		return FALSE;
	return TRUE;

}

char* GetSoundErrtext(HRESULT hr)
{
    switch (hr)
    {
        case DSERR_ALLOCATED:
            return "DSERR_ALLOCATED";
        case DSERR_INVALIDPARAM:
            return"DSERR_INVALIDPARAM";
        case DSERR_NOAGGREGATION:
            return"DSERR_NOAGGREGATION";
        case DSERR_NODRIVER:
            return"DSERR_NODRIVER";
        case DSERR_OUTOFMEMORY:
            return"DSERR_OUTOFMEMORY";
        case DSERR_UNINITIALIZED:
            return "DSERR_UNINITIALIZED";
        case DSERR_UNSUPPORTED:
            return "DSERR_UNSUPPORTED";
        default:
            return "";
    }
}

//----------------------------------------------------
//----------------------------------------------------
SOUNDOBJECT* CreateSoundObject(int oid, int f)
{
    NewObjectIdOk(oid);
    SOUNDOBJECT* po = (SOUNDOBJECT*) malloc (sizeof(SOUNDOBJECT));
    Objects[oid] = (DOBJECT*)po;

    po->o.id    = oid;
    po->o.type  = SOUNDOBJ;
    po->o.flags = f & (FLG_DISABLED | FLG_LOOP);
    po->o.tag = 0;

    po->dsb = NULL;
    return po;
}

void DestroySoundObject(SOUNDOBJECT* po)
{
    if (po->dsb)
    {
        po->dsb->Stop();
        po->dsb->Release();
    }
}

BOOL Load (int oid, char *file)
{
    DOBJECT* poo = GetValidObject(oid);
    if (poo->type != SOUNDOBJ)
    {
        Error("object id is not a sound");
        return FALSE;
    }
    SOUNDOBJECT *po = (SOUNDOBJECT*)poo;

    HMMIO wavefile;
    wavefile = mmioOpen (file, 0, MMIO_READ | MMIO_ALLOCBUF);
    if (wavefile == NULL)
    {
        Error ("cannot open file \"%s\"", file);
        return FALSE;
    }
    MMCKINFO parent;
    memset (&parent, 0, sizeof(MMCKINFO));
    parent.fccType = mmioFOURCC ('W', 'A', 'V', 'E');
    mmioDescend (wavefile, &parent, 0, MMIO_FINDRIFF);
    MMCKINFO child;
    memset (&child, 0, sizeof(MMCKINFO));
    child.fccType = mmioFOURCC ('f', 'm', 't', ' ');
    mmioDescend (wavefile, &child, &parent, 0);
    WAVEFORMATEX wavefmt;
    mmioRead (wavefile, (char*)&wavefmt, sizeof(wavefmt));

    if(wavefmt.wFormatTag != WAVE_FORMAT_PCM)
    {
        Error ("%s: waveformat unsupported", file);
        return FALSE;
    }
    mmioAscend (wavefile, &child, 0);
    child.ckid = mmioFOURCC ('d', 'a', 't', 'a');
    mmioDescend (wavefile, &child, &parent, MMIO_FINDCHUNK);

    DSBUFFERDESC bufdesc;
    memset (&bufdesc, 0, sizeof(DSBUFFERDESC));
    bufdesc.dwSize = sizeof(DSBUFFERDESC);
    bufdesc.dwFlags = DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_GLOBALFOCUS;
    bufdesc.dwBufferBytes = child.cksize;
    bufdesc.lpwfxFormat = &wavefmt;

    if((lpds->CreateSoundBuffer (&bufdesc, &po->dsb, NULL)) != DS_OK)
    {
        po->dsb = NULL;
        Error ("Create Soundbuffer");
        return FALSE;
    }
    void *write1 = 0, *write2 = 0;
    unsigned long length1,length2;
    po->dsb->Lock (0, child.cksize, &write1, &length1, &write2, &length2, 0);
    if(write1 > 0)
        mmioRead (wavefile, (char*)write1, length1);
    if (write2 > 0)
        mmioRead (wavefile, (char*)write2, length2);
    po->dsb->Unlock (write1, length1, write2, length2);
    mmioClose (wavefile, 0);
	return TRUE;
}

void Play(int oid)
{

    DOBJECT* poo = GetValidObject(oid);
    if (poo->type != SOUNDOBJ)
    {
        Error("object id is not a sound");
        return;
    }
    SOUNDOBJECT *po = (SOUNDOBJECT*)poo;
	if (po->dsb)
		po->dsb->Play (0, 0, (po->o.flags & FLG_LOOP) ? DSBPLAY_LOOPING: 0);
}

void Stop(int oid)
{
    DOBJECT* poo = GetValidObject(oid);
    if (poo->type != SOUNDOBJ)
    {
        Error("object id is not a sound");
        return;
    }
    SOUNDOBJECT *po = (SOUNDOBJECT*)poo;
	if (po->dsb)
	    po->dsb->Stop ();
}

void Vol(int oid, int vol)
{
    vol = vol < 0 ? 0 : (vol>100 ? 100 : vol);
    DOBJECT* poo = GetValidObject(oid);
    if (poo->type != SOUNDOBJ)
    {
        Error("object id is not a sound");
        return;
    }
    SOUNDOBJECT *po = (SOUNDOBJECT*)poo;
	if (po->dsb)
	    po->dsb->SetVolume((100-vol) * -50);
}

void Pan(int oid, int pan)
{
    pan = pan < -100 ? -100 : (pan>100 ? 100 : pan);
    DOBJECT* poo = GetValidObject(oid);
    if (poo->type != SOUNDOBJ)
    {
        Error("object id is not a sound");
        return;
    }
    SOUNDOBJECT *po = (SOUNDOBJECT*)poo;
	if (po->dsb)
	    po->dsb->SetPan(pan * 100);
}

void Rewind(int oid)
{
    DOBJECT* poo = GetValidObject(oid);
    if (poo->type != SOUNDOBJ)
    {
        Error("object id is not a sound");
        return;
    }
    SOUNDOBJECT *po = (SOUNDOBJECT*)poo;
	if (po->dsb)
	{
		po->dsb->Stop ();
		po->dsb->SetCurrentPosition (0);
	}
}

//---------------------------------------------------------------------------
//#pragma package(smart_init)
