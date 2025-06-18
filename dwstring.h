//---------------------------------------------------------------------------

#ifndef dwstringH
#define dwstringH


extern "C" {

typedef struct
{
  void*     Memory;
  int       Size;
  int       allo;
  int       Position;
} MEMORYSTREAM;

MEMORYSTREAM* MemoryStreamCreate();
void MemoryStreamDestroy(MEMORYSTREAM* m);
void MemoryStreamClear(MEMORYSTREAM* m);
void MemoryStreamWrite (MEMORYSTREAM* m, char* buf, int cnt);


typedef struct
{
	char**		Strings;
	void**     	Objects;
	int        	Count;
	int        	Allo;
} STRINGLIST;

STRINGLIST*     SLCreate();
void            SLDestroy(STRINGLIST* sl);
int         	SLAdd(STRINGLIST* sl, char* p);
int             SLAddObject(STRINGLIST* sl, char* p, void* obj);
bool        	SLDelete(STRINGLIST* sl, int ix);
void        	SLClear(STRINGLIST* sl);

extern void     Error (LPSTR fmt, ...);

}

#endif
