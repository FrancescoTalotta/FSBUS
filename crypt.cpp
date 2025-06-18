//---------------------------------------------------------------------------
#include "stdafx.h"

#include "fsbusdll.h"

char licinfo[256] = "";


bool CheckLicense()
{
	char buf[256];
	char normaltext[256] = "";
	char activation[256] = "";

	if (GetModuleFileName(NULL, buf, sizeof(buf)) <= 0)
		return false;
	for (char* p= &buf[strlen(buf)]-1; p > buf; p--)
		if (*p == '\\')
		{
			p[1] = 0; 
			break;
		}
	strcat_s(buf, sizeof(buf), "fsbus.ini");
	char *file = _strdup (buf);
	bool bret = false;

	GetPrivateProfileString("LICENSE","NAME","",licinfo,sizeof(licinfo),file);
	strcpy_s (normaltext, sizeof(normaltext), licinfo);
	NormalText (normaltext);
	GetPrivateProfileString("LICENSE","ACTIVATION","",activation,sizeof(activation),file);
	U_KEY key;
	if (Ascii2Bin (activation, &key) == true)
	{
		unsigned int textcs = Checksum (normaltext);
		if (textcs == key.s_buf.checksum)
			bret = true;
	}

	free (file);
	return bret;
}

unsigned int Checksum (char* s)
{
	unsigned int cs = 0x51feb85d;
	int shft = 0;

    for (;*s; s++)
    {
        cs ^= *s << shft;
        shft += 8;
		if (shft > 24)
			shft = 0;
    }
	return cs;
}

bool Ascii2Bin (char* ascii, U_KEY* key)
{
	int i=0;
	int bc = 0;
	int cnt = sizeof(key->buf);
	unsigned char* pdest = key->buf;

	*pdest = 0;
	memset (key->buf, 0, sizeof(key->buf));
	for (; *ascii && cnt; ascii++)
	{
		for (; *ascii && *ascii<=' '; ascii++);
		*pdest = *pdest << 4;

		char c = tolower(*ascii);
		if (c >= '0' && c <= '9')
			*pdest |= (c - '0');
		else
			if (c >= 'a' && c <= 'f')
				*pdest |= (c - 'a') + 10;
			else
				return false;


		if (bc & 1)
		{
			pdest++;
			*pdest = 0;
			cnt--;
		}
		bc++;
	}
	return true;
}


void NormalText (char *p)
{
	int substcount = 0;
	char *pdest = p;

	for (; *p; p++)
	{
		if (*p < 0)
		{
			*pdest++ = (++substcount) + '0';
			if (substcount > 9)
				substcount = -1;
		}
		else
			if (*p > ' ')
				*pdest++ = *p;
	}
	*pdest = 0;
}



