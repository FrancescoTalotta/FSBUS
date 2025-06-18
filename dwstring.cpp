//-------------------------------------------------

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <memory.h>
#include <malloc.h>
#include <assert.h>

#include "dwstring.h"

MEMORYSTREAM* MemoryStreamCreate()
{
    MEMORYSTREAM* m = (MEMORYSTREAM*) malloc (sizeof(MEMORYSTREAM));
    if (!m)
        Error ("not enough memory");
    m->allo = 512;
    m->Memory = malloc (m->allo);
    if (!m->Memory)
        Error ("not enough memory");
    m->Size = 0;
    m->Position = 0;
    return m;
}
void MemoryStreamDestroy(MEMORYSTREAM* m)
{
    if (m->Memory)
        free (m->Memory);
    free (m);
}

void MemoryStreamClear(MEMORYSTREAM* m)
{
    m->Size = 0;
    m->Position = 0;
}

void MemoryStreamWrite (MEMORYSTREAM* m, char* buf, int cnt)
{
    int nsz = m->Size + cnt;
    if (nsz > m->allo)
    {
        if ((m->allo*2) >= nsz)
            nsz = m->allo * 2;

        char* p = (char*)malloc (nsz);
        memcpy (p, m->Memory, m->Position);
        free(m->Memory);
        m->Memory = p;
        m->allo = nsz;
    }
    memcpy ((char *)m->Memory + m->Size, buf, cnt);
    m->Size += cnt;
}


//-------------------------------------------------------------
STRINGLIST*  SLCreate()
{
    STRINGLIST* psl = (STRINGLIST*)malloc(sizeof(STRINGLIST));
    if (!psl)
        Error ("not enough memory");
	psl->Count = 0;
	psl->Allo = 0;
	psl->Strings = NULL;
	psl->Objects = NULL;
    return psl;
}

void SLDestroy(STRINGLIST* sl)
{
	SLClear(sl);
    if (sl->Strings)
        free (sl->Strings);
    if (sl->Objects)
        free (sl->Objects);
    free (sl);
}

void SLClear(STRINGLIST* sl)
{
	for (int i=0; i<sl->Count; i++)
    {
        free (sl->Strings[i]);
        sl->Strings[i] = NULL;
        sl->Objects[i] = NULL;
    }
	sl->Count = 0;
}

bool SLDelete(STRINGLIST* sl, int ix)
{
	if (ix < 0 || ix >= sl->Count)
		return false;
	if (sl->Strings[ix])
		free (sl->Strings[ix]);

	for (int i=ix+1; i<sl->Allo; i++)
	{
		sl->Strings[i-1] = sl->Strings[i];
		sl->Objects[i-1] = sl->Objects[i];
	}
	sl->Strings[sl->Allo-1] = NULL;
	sl->Objects[sl->Allo-1] = NULL;
	return true;
}

int SLAddObject(STRINGLIST* sl, char* p, void* obj)
{
    int i = SLAdd(sl, p);
    sl->Objects[i] = obj;
    return i;
}

int SLAdd(STRINGLIST* sl, char* p)
{
	int i;

	if (sl->Count >= sl->Allo)
	{
		sl->Allo = (!sl->Allo) ? 16 : (sl->Allo * 2);
		char** pn = (char**)malloc (sl->Allo * sizeof(char*));
        if (!pn)
            Error ("not enough memory");

		if (sl->Count)
		{
			memcpy (pn, sl->Strings, sizeof(char*) * sl->Count);
			free (sl->Strings);
		}
		sl->Strings = pn;
		for (i = sl->Count; i<sl->Allo; i++)
			sl->Strings[i] = NULL;

		char** po = (char**)malloc (sl->Allo * sizeof(char*));
        if (!po)
            Error ("not enough memory");

		if (sl->Objects)
		{
			memcpy (po, sl->Objects, sizeof(void*) * sl->Count);
			free(sl->Objects);
		}
		sl->Objects = (void**)po;
		for (i = sl->Count; i<sl->Allo; i++)
			sl->Objects[i] = NULL;
	}
	sl->Objects[sl->Count] = NULL;
	char *ds = _strdup(p);

	sl->Strings[sl->Count++] = ds;
	return sl->Count-1;
}

//---------------------------------------------------------------------------

//#pragma package(smart_init)
