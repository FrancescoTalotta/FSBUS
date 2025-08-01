//---------------------------------------------------------------------------
#include "stdafx.h"
#include "fsbusdll.h"
#pragma hdrstop

int ParseSysSequence (const char* p, MEMORYSTREAM* pm);
int MakeVK(const char* s);

//---------------------------------------------------------------------------
void Keyboard(const char* p)
{

    for (; *p; p++)
    {
        bool bUpper = false;
        bool bChr = false;
        unsigned char c;

        switch (*p)
        {
        case '�':   c = 0xde; break;
        case '�':   c = 0xc0; break;
        case '�':   c = 0xba; break;
        case '�':   c = 0xdb; break;
        case '#':   c = 0xbf; break;
        case ',':   c = 0xbc; break;
        case '.':   c = 0xbe; break;
        case '-':   c = 0xbd; break;
        case (unsigned char)'�':   c = 0xdd; break;
        case '<':   c = 0xe2; break;
        case '+':   c = 0xbb; break;
        default:
            bChr = isalpha(*p) ? TRUE : FALSE;
            c = bChr ? toupper(*p) : *p;
            break;

        }
        if (bChr && isupper(*p))
            bUpper = true;
        if (bUpper)
            keybd_event(VK_SHIFT, 0, 0, 0);
        keybd_event(c, 0, 0, 0);
        keybd_event(c, 0, KEYEVENTF_KEYUP, 0);
        if (bUpper)
            keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
    }
}

void ExtKeyboard(const char* p)
{
    MEMORYSTREAM* ms = MemoryStreamCreate();
    ParseSysSequence ((const char *)p, ms);

    unsigned short* sys = (unsigned short*)ms->Memory;
    int count = ms->Size;
    if (!count)
    {
        delete ms;
        return;
    }

    HWND h = GetActiveWindow();
    // If the function succeeds, handle of active window associated with the thread
    // that calls the function. If the calling thread does not have an active window,
    // the return value is NULL.
    int hscreenpx = GetSystemMetrics(SM_CXSCREEN);
    int vscreenpx = GetSystemMetrics(SM_CYSCREEN);
    int flag;
    count /= sizeof(unsigned short);

    for (int i=0; i<count; )
    {
        switch (sys[i])
        {
        case 'D':
            flag = sys[i+1];
            Sleep (flag);
            i += 2;
            break;
        case 'K':
            flag = sys[i+1] == '-' ? KEYEVENTF_KEYUP : 0;
            keybd_event((unsigned char)sys[i+2], 0, flag, 0);
            i += 3;
            break;
        case 'L':
            flag = MOUSEEVENTF_ABSOLUTE;
            flag |= sys[i+1] == '-' ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN;
            mouse_event(MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE, MulDiv (0xffff,sys[i+2],hscreenpx),
                            MulDiv (0xffff, sys[i+3], vscreenpx), 0, 0);
            mouse_event(flag, MulDiv (0xffff,sys[i+2],hscreenpx),
                            MulDiv (0xffff, sys[i+3], vscreenpx), 0, 0);
            i += 4;
            break;
        case 'R':
            mouse_event(MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE, MulDiv (0xffff,sys[i+2],hscreenpx),
                            MulDiv (0xffff, sys[i+3], vscreenpx), 0, 0);
            flag = MOUSEEVENTF_ABSOLUTE;
            flag |= sys[i+1] == '-' ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN;
            mouse_event(flag, MulDiv (0xffff,sys[i+2],hscreenpx),
                            MulDiv (0xffff, sys[i+3], vscreenpx), 0, 0);
            i += 4;
            break;
        default:
            i += 1;
            break;
        }
    }
    delete ms;
}

//---------------------------------------------------------------------------
// 0,1 Trenner = |
// Sequenztrenner = ;
// Parametertrenner = ,

int ParseSysSequence (const char* p, MEMORYSTREAM* pm)
{
    char buf[128];
    int ibuf = 0;
    enum {TYPE,KDOWNUP,KVAL,MS,KEY,VAL,DELAY, WINDOW} state = TYPE;
    unsigned short i;
    MemoryStreamClear(pm);

    for (; *p; p++)
    {
        switch (state)
        {
        case TYPE:
            switch (toupper(*p))
            {
            case 'L': case 'R':
                i = toupper(*p);
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                state = MS;
                break;
            case 'K':
                i = 'K';
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                state = KDOWNUP;
                break;
            case 'D':
                i = 'D';
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                state = DELAY;
                break;
            case 'W':
                i = 'W';
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                state = WINDOW;
                break;
            default:
                if (*p > ' ')
                    return -1;
                break;
            }
            continue;

        case WINDOW:
            switch (*p)
            {
            case ';':
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                state = TYPE;
                break;
            default:
                i = (unsigned short)*p;
                break;
            }
            break;

        case KDOWNUP:
            switch (*p)
            {
            case '-':
            case '+':
                i = *p;
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                state = KEY;
                break;
            default:
                return -1;
            }
            break;

        case DELAY:
            switch (*p)
            {
            case ';':
                i = atoi(buf);
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                state = TYPE;
                ibuf = 0;
                break;
            default:
                buf[ibuf++] = *p;
                buf[ibuf] = 0;
                break;
            }
            break;

        case KEY:
            switch (*p)
            {
            case ';':
                i = MakeVK (buf);
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                state = TYPE;
                ibuf = 0;
                break;
            default:
                buf[ibuf++] = *p;
                buf[ibuf] = 0;
                if (p[1] == 0)
                {
                    i = MakeVK (buf);
                    MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                    state = TYPE;
                    ibuf = 0;
                }
                break;
            }
            break;

        case MS:
            switch (*p)
            {
            case '-':
            case '+':
                i = *p;
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                state = VAL;
                break;
            default:
                return false;
            }
            break;

        case VAL:
            switch (*p)
            {
            case ',':
                i = atoi(buf);
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                ibuf = 0;
                break;
            case ';':
                i = atoi(buf);
                MemoryStreamWrite (pm, (char *)&i, sizeof(i));
                state = TYPE;
                ibuf = 0;
                break;

            default:
                buf[ibuf++] = *p;
                buf[ibuf] = 0;
                break;
            }
            break;
        } // END SWITCH TYPE
    } // END WHILE
    if (ibuf)
    {
        i = atoi(buf);
        MemoryStreamWrite (pm, (char *)&i, sizeof(i));
    }
    return 0;
}

//---------------------------------------------------------------------------
int MakeVK(const char* s)
{
    if (strlen(s) == 0)
        return 0;
    if (strlen(s) == 1)
        return (int)s[0];

    static struct
    {
        LPSTR   name;
        int     vk;
    } vktab[] = {
	{"VK_CANCEL", VK_CANCEL}, {"VK_BACK", VK_BACK},	{"VK_TAB", VK_TAB},
	{"VK_CLEAR", VK_CLEAR},	{"VK_RETURN", VK_RETURN},{"VK_SHIFT", VK_SHIFT},
	{"VK_CONTROL", VK_CONTROL},	{"VK_MENU", VK_MENU},	{"VK_PAUSE", VK_PAUSE},
	{"VK_CAPITAL", VK_CAPITAL},	{"VK_KANA",VK_KANA },	{"VK_HANGUL",VK_HANGUL },
	{"VK_JUNJA", VK_JUNJA},	{"VK_FINAL", VK_FINAL},	{"VK_HANJA", VK_HANJA},
	{"VK_KANJI", VK_KANJI},	{"VK_ESCAPE", VK_ESCAPE},	{"VK_CONVERT", VK_CONVERT},
	{"VK_NONCONVERT", VK_NONCONVERT},	{"VK_ACCEPT", VK_ACCEPT},{"VK_MODECHANGE", VK_MODECHANGE},
	{"VK_SPACE", VK_SPACE}, {"VK_PRIOR", VK_PRIOR},	{"VK_NEXT", VK_NEXT},
	{"VK_END", VK_END},{"VK_HOME", VK_HOME},{"VK_LEFT", VK_LEFT},{"VK_UP", VK_UP},
	{"VK_RIGHT", VK_RIGHT},{"VK_DOWN", VK_DOWN},{"VK_SELECT", VK_SELECT},
	{"VK_PRINT", VK_PRINT},{"VK_EXECUTE", VK_EXECUTE},{"VK_SNAPSHOT", VK_SNAPSHOT},
	{"VK_INSERT", VK_INSERT},{"VK_DELETE", VK_DELETE},{"VK_HELP", VK_HELP},
	{"VK_LWIN", VK_LWIN},{"VK_RWIN", VK_RWIN},{"VK_APPS", VK_APPS},
#	ifdef VK_SLEEP
	{"VK_SLEEP", VK_SLEEP},
#	endif
	{"VK_NUMPAD0", VK_NUMPAD0},
	{"VK_NUMPAD1", VK_NUMPAD1},
    {"VK_NUMPAD2", VK_NUMPAD2},
	{"VK_NUMPAD3", VK_NUMPAD3},
	{"VK_NUMPAD4", VK_NUMPAD4},
	{"VK_NUMPAD5", VK_NUMPAD5},
	{"VK_NUMPAD6", VK_NUMPAD6},
	{"VK_NUMPAD7", VK_NUMPAD7},
	{"VK_NUMPAD8", VK_NUMPAD8},
	{"VK_NUMPAD9", VK_NUMPAD9},
	{"VK_MULTIPLY", VK_MULTIPLY},
	{"VK_ADD", VK_ADD},	{"VK_SEPARATOR", VK_SEPARATOR},	{"VK_SUBTRACT", VK_SUBTRACT},
	{"VK_DECIMAL", VK_DECIMAL},	{"VK_DIVIDE", VK_DIVIDE},	{"VK_F1", VK_F1},
	{"VK_F2", VK_F2},{"VK_F3", VK_F3},{"VK_F4", VK_F4},{"VK_F5", VK_F5},
	{"VK_F6", VK_F6},{"VK_F7", VK_F7},{"VK_F8", VK_F8},{"VK_F9", VK_F9},
	{"VK_F10", VK_F10},{"VK_F11", VK_F11},{"VK_F12", VK_F12},{"VK_F13", VK_F13},
	{"VK_F14", VK_F14},{"VK_F15", VK_F15},{"VK_F16", VK_F16},{"VK_F17", VK_F17},
	{"VK_F18", VK_F18},{"VK_F19", VK_F19},{"VK_F20", VK_F20},{"VK_F21", VK_F21},
	{"VK_F22", VK_F22},{"VK_F23", VK_F23},{"VK_F24", VK_F24},{"VK_NUMLOCK", VK_NUMLOCK},
	{"VK_SCROLL", VK_SCROLL},
#	ifdef 	VK_OEM_NEC_EQUAL
	{"VK_OEM_NEC_EQUAL", VK_OEM_NEC_EQUAL},
#endif	
	{"VK_LSHIFT", VK_LSHIFT},
	{"VK_RSHIFT", VK_RSHIFT},{"VK_LCONTROL", VK_LCONTROL},{"VK_RCONTROL", VK_RCONTROL},
	{"VK_LMENU", VK_LMENU},{"VK_RMENU", VK_RMENU},
#	ifdef	VK_BROWSER_BACK
	{"VK_BROWSER_BACK", VK_BROWSER_BACK},
	{"VK_BROWSER_FORWARD", VK_BROWSER_FORWARD},
	{"VK_BROWSER_REFRESH", VK_BROWSER_REFRESH},
	{"VK_BROWSER_STOP", VK_BROWSER_STOP},
	{"VK_BROWSER_SEARCH", VK_BROWSER_SEARCH},
	{"VK_BROWSER_FAVORITES", VK_BROWSER_FAVORITES},
	{"VK_BROWSER_HOME", VK_BROWSER_HOME},
	{"VK_VOLUME_MUTE", VK_VOLUME_MUTE},
	{"VK_VOLUME_DOWN", VK_VOLUME_DOWN},
	{"VK_VOLUME_UP", VK_VOLUME_UP},
	{"VK_MEDIA_NEXT_TRACK", VK_MEDIA_NEXT_TRACK},
	{"VK_MEDIA_PREV_TRACK", VK_MEDIA_PREV_TRACK},
	{"VK_MEDIA_STOP", VK_MEDIA_STOP},
	{"VK_MEDIA_PLAY_PAUSE", VK_MEDIA_PLAY_PAUSE},
	{"VK_LAUNCH_MAIL", VK_LAUNCH_MAIL},
	{"VK_LAUNCH_MEDIA_SELECT", VK_LAUNCH_MEDIA_SELECT},
	{"VK_LAUNCH_APP1", VK_LAUNCH_APP1},
	{"VK_LAUNCH_APP2", VK_LAUNCH_APP2},
	{"VK_OEM_1", VK_OEM_1},
	{"VK_OEM_PLUS", VK_OEM_PLUS},
	{"VK_OEM_COMMA", VK_OEM_COMMA},
	{"VK_OEM_MINUS", VK_OEM_MINUS},
	{"VK_OEM_PERIOD", VK_OEM_PERIOD},{"VK_OEM_2", VK_OEM_2},
	{"VK_OEM_3", VK_OEM_3},{"VK_OEM_4", VK_OEM_4},{"VK_OEM_5", VK_OEM_5},
	{"VK_OEM_6", VK_OEM_6},{"VK_OEM_7", VK_OEM_7},{"VK_OEM_8", VK_OEM_8},
	{"VK_OEM_AX", VK_OEM_AX},	
	{"VK_OEM_102", VK_OEM_102},
	{"VK_ICO_HELP", VK_ICO_HELP},
	{"VK_ICO_00", VK_ICO_00},
	{"VK_PROCESSKEY", VK_PROCESSKEY},
	{"VK_ICO_CLEAR", VK_ICO_CLEAR},
	{"VK_PACKET",VK_PACKET },
	{"VK_OEM_RESET", VK_OEM_RESET},
	{"VK_OEM_JUMP", VK_OEM_JUMP},
	{"VK_OEM_PA1", VK_OEM_PA1},{"VK_OEM_PA2", VK_OEM_PA2},{"VK_OEM_PA3", VK_OEM_PA3},
	{"VK_OEM_WSCTRL", VK_OEM_WSCTRL},{"VK_OEM_CUSEL", VK_OEM_CUSEL},{"VK_OEM_ATTN", VK_OEM_ATTN},
	{"VK_OEM_FINISH", VK_OEM_FINISH},{"VK_OEM_COPY", VK_OEM_COPY},{"VK_OEM_AUTO", VK_OEM_AUTO},
	{"VK_OEM_ENLW", VK_OEM_ENLW},{"VK_OEM_BACKTAB", VK_OEM_BACKTAB},
#	endif	
	{"VK_ATTN", VK_ATTN},
	{"VK_CRSEL", VK_CRSEL},{"VK_EXSEL", VK_EXSEL},{"VK_EREOF", VK_EREOF},
	{"VK_PLAY", VK_PLAY},{"VK_ZOOM", VK_ZOOM},{"VK_NONAME", VK_NONAME},
	{"VK_PA1", VK_PA1},{"VK_OEM_CLEAR", VK_OEM_CLEAR},
    {NULL, 0}  };

    for (int i=0; vktab[i].name; i++)
        if (strcmp(s, vktab[i].name) == 0)
            return vktab[i].vk;
    return 0;
}

//#pragma package(smart_init)
