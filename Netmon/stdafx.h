
#pragma once

#define WIN32_LEAN_AND_MEAN

#pragma region TCP/UDP Table Size Definition

#define  ANY_SIZE 1024

#pragma endregion

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <uxtheme.h>

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <assert.h>
#include <math.h>
#include <time.h>

// we use "tstring" that works with TCHARs
#include <string>
#include <xstring>

#ifdef _UNICODE
    #define tstring wstring
#else
    #define tstring string
#endif

#include <vector>
#include <map>

#include <tcpmib.h>

//#define DEBUG
//#define DEBUG_PID 4088
