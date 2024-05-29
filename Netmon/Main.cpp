
#include "stdafx.h"

#include "res/resource.h"
#include "../Lang/resource.h"

#include "utils/Utils.h"
#include "utils/SQLite.h"
#include "utils/ProcessModel.h"
#include "utils/ProcessView.h"
#include "utils/PortCache.h"
#include "utils/ProcessCache.h"
#include "utils/Language.h"
#include "utils/Profile.h"

#include "traffic-src/PcapSource.h"
#include "traffic-src/VirtualSource.h"

#include "DlgAbout.h"

#include "plugins/realtime/Realtime.h"
#include "plugins/month/Month.h"

#define WM_USER_TRAY            (WM_USER + 1)
#define WM_RECONNECT            (WM_USER + 2)
#define WM_CLEAR_DB_AND_RESTART (WM_USER + 3) // also defined in DlgPreference.h
#define WM_RESTART              (WM_USER + 4)

///----------------------------------------------------------------------------------------------//
///                                    Global Variables                                          //
///----------------------------------------------------------------------------------------------//
#pragma region Global Variables

HINSTANCE g_hInstance;

static HWND    g_hDlgMain;
static HWND    g_hCurPage;   // Current Child Dialog Box 
static HMENU   g_hTrayMenu;
static HMENU   g_hProcessMenu;

// Sidebar GDI objects
static HDC     g_hDcSidebarBg;
static HDC     g_hDcSidebarBuf;
static HBITMAP g_hBmpSidebarBg;
static HBITMAP g_hBmpSidebarBuf;

static HDC     g_hDcStart;
static HDC     g_hDcStartHover;
static HDC     g_hDcStop;
static HDC     g_hDcStopHover;

static HBITMAP g_hBmpStart;
static HBITMAP g_hBmpStartHover;
static HBITMAP g_hBmpStop;
static HBITMAP g_hBmpStopHover;

static enum enumHoverState
{
    Start, Stop, Neither
} g_enumHoverState;
// g_enumHoverState = Neither;

static int     g_iSidebarWidth;
static int     g_iSidebarHeight;

// Capture thread
HANDLE         g_hCaptureThread;
bool           g_bCapture = false;

// Adapter
int            g_nAdapters = 0;
int            g_iAdapter = 0;
TCHAR          g_szAdapterNames[16][256];

// Plugins
static std::vector<Plugin *> g_plugins;

// Language
static int     g_nLanguage;
static int     g_iCurLanguage;

// Profile
Profile        g_profile;

// View Setting
bool           g_bShowHidden;

// Splitter
static bool    g_bDragging = false;

// Options -h
static bool    g_bHideWindow = false;

#pragma endregion

///----------------------------------------------------------------------------------------------//
///                                    Capture Thread                                            //
///----------------------------------------------------------------------------------------------//
static DWORD WINAPI CaptureThread(LPVOID lpParam)
{
    // VirtualSource source;
    PcapSource source;
    PacketInfo pi;
    PacketInfoEx pie;

    PortCache pc;

    // Init Filter ------------------------------------------------------------
    if (!source.Initialize())
    {
        return 1;
    }

    // Find Devices -----------------------------------------------------------
    if (!source.EnumDevices())
    {
        return 2;
    }

    // Select a Device --------------------------------------------------------
    if (!source.SelectDevice(g_iAdapter))
    {
        return 3;
    }

    // Capture Packets --------------------------------------------------------
    while (g_bCapture)
    {
        int pid = -1;
        int processUID = -1;
        TCHAR processName[MAX_PATH] = TEXT("Unknown");
        TCHAR processFullPath[MAX_PATH] = TEXT("-");

        // - Get a Packet (Process UID or PID is not Provided Here)
        bool timeout = false;
        if (!source.Capture(&pi, &timeout))
        {
            if (timeout) // Timeout
            {
                if (!g_bCapture) // Stop when user clicks "Stop"
                {
                    break;
                }
                else // Just try again
                {
                    continue;
                }
            }
            else // Error
            {
                if (source.Reconnect(g_iAdapter))
                {
                    continue; // Auto-reconnect succeeded
                }
                else // Need manual reconnect
                {
                    PostMessage(g_hDlgMain, WM_RECONNECT, 0, 0);
                }
            }
        }

        // - Get PID
        if (pi.trasportProtocol == TRA_TCP )
        {
            pid = pc.GetTcpPortPid(pi.local_port);
            pid = ( pid == 0 ) ? -1 : pid;
        }
        else if (pi.trasportProtocol == TRA_UDP )
        {
            pid = pc.GetUdpPortPid(pi.local_port);
            pid = ( pid == 0 ) ? -1 : pid;
        }

        // - Get Process Name & Full Path
        if (pid != -1 )
        {
            ProcessCache::instance()->GetName(pid, processName, MAX_PATH);
            ProcessCache::instance()->GetFullPath(pid, processFullPath, MAX_PATH);

            if (processName[0] == TEXT('\0')) // Cannot get process name from the table
            {
                pid = -1;
                _tcscpy_s(processName, MAX_PATH, TEXT("Unknown"));
                _tcscpy_s(processFullPath, MAX_PATH, TEXT("-"));

                // Map from Port -> PID is successful, but pid does not exist, rebuild cache
                if (pi.trasportProtocol == TRA_TCP)
                {
                    pc.RebuildTcpTable();
                }
                else if (pi.trasportProtocol == TRA_UDP)
                {
                    pc.RebuildUdpTable();
                }
            }
        }
        // else
        //    it's likely to be an ICMP packet or something else, 
        //    processName is still "Unknown" and processFullPath is still "-"

        // - Get Process UID
        processUID = ProcessModel::GetProcessUid(processName);

        // - Insert Into Process Table
        if (processUID == -1)
        {
            processUID = Utils::InsertProcess(processName, processFullPath);
        }

        // - Fill PacketInfoEx
        memcpy(&pie, &pi, sizeof(pi));

        pie.pid = pid;
        pie.puid = processUID;

        _tcscpy_s(pie.name, MAX_PATH, processName);
        _tcscpy_s(pie.fullPath, MAX_PATH, processFullPath);

        // - Update Process List
        ProcessModel::OnPacket(&pie);

        // - Update Views
        for (unsigned int i = 0; i < g_plugins.size(); i++)
        {
            g_plugins[i]->InsertPacket(&pie);
        }

        // - Dump
#ifdef DUMP_PACKET
        {
            TCHAR msg[128];
            TCHAR *protocol = (pi.networkProtocol == NET_ARP) ? TEXT("ARP") : 
                              (pi.trasportProtocol == TRA_TCP) ? TEXT("TCP") :
                              (pi.trasportProtocol == TRA_UDP) ? TEXT("UDP") : 
                              (pi.trasportProtocol == TRA_ICMP) ? TEXT("ICMP") : 
                              (pi.trasportProtocol == TRA_IGMP) ? TEXT("IGMP") : TEXT("Other");
            TCHAR *dir = (pi.dir == DIR_UP) ? TEXT("Up") : 
                         (pi.dir == DIR_DOWN) ? TEXT("Down") : TEXT("");
            _stprintf_s(msg, _countof(msg), 
                TEXT("[Time = %d.%06d] [Size = %4d Bytes] [Port = %d, %d] %s %s\n"), 
                pi.time_s, pi.time_us, pi.size, pi.remote_port, pi.local_port, dir, protocol);

            OutputDebugString(msg);
        }
#endif
    }

    return 0;
}

///----------------------------------------------------------------------------------------------// 
///                                    Switch Language                                           //
///----------------------------------------------------------------------------------------------//
static void UpdateMenuLanguage()
{
    HMENU hMenuMain = GetMenu(g_hDlgMain);

    HMENU hMenuFile    = GetSubMenu(hMenuMain, 0);
    HMENU hMenuView    = GetSubMenu(hMenuMain, 1);
    /*HMENU hMenuOptions = GetSubMenu(hMenuMain, 2);*/
    HMENU hMenuHelp    = GetSubMenu(hMenuMain, 3);

    HMENU hMenuViewAdapter     = GetSubMenu(hMenuView, g_plugins.size() + 1);
    /*HMENU hMenuOptionsLanguage = GetSubMenu(hMenuOptions, 0);*/

    // Menu bar
    Utils::SetMenuString(hMenuMain, 0, TRUE, Language::GetString(IDS_MENU_FILE));
    Utils::SetMenuString(hMenuMain, 1, TRUE, Language::GetString(IDS_MENU_VIEW));
    //Utils::SetMenuString(hMenuMain, 2, TRUE, Language::GetString(IDS_MENU_OPTIONS));
    Utils::SetMenuString(hMenuMain, 3, TRUE, Language::GetString(IDS_MENU_HELP));

    // File
    Utils::SetMenuString(hMenuFile, 0, TRUE, Language::GetString(IDS_MENU_FILE_CAPTURE));
    Utils::SetMenuString(hMenuFile, 1, TRUE, Language::GetString(IDS_MENU_FILE_STOP));
    Utils::SetMenuString(hMenuFile, 3, TRUE, Language::GetString(IDS_MENU_FILE_EXIT));

    // View
    for (unsigned int i = 0; i < g_plugins.size(); i++)
    {
        Utils::SetMenuString(hMenuView, i, TRUE, g_plugins[i]->GetName());
    }

    Utils::SetMenuString(hMenuView, g_plugins.size() + 1, TRUE, 
        Language::GetString(IDS_MENU_VIEW_ADAPTER));
    Utils::SetMenuString(hMenuView, g_plugins.size() + 3, TRUE,
        Language::GetString(IDS_MENU_VIEW_SHOW_HIDDEN));

    //// Options
    //Utils::SetMenuString(hMenuOptions, 0, TRUE, Language::GetString(IDS_MENU_OPTIONS_LANGUAGE));
    //Utils::SetMenuString(hMenuOptions, 1, TRUE, Language::GetString(IDS_MENU_OPTIONS_PREFERENCES));

    // Help
    Utils::SetMenuString(hMenuHelp, 0, TRUE, Language::GetString(IDS_MENU_HELP_TOPIC));
    Utils::SetMenuString(hMenuHelp, 1, TRUE, Language::GetString(IDS_MENU_HELP_HOMEPAGE));
    Utils::SetMenuString(hMenuHelp, 3, TRUE, Language::GetString(IDS_MENU_HELP_ABOUT));

    // Tray
    Utils::SetMenuString(g_hTrayMenu, 0, TRUE, Language::GetString(IDS_MENU_TRAY_SHOW_WINDOW));
    Utils::SetMenuString(g_hTrayMenu, 1, TRUE, Language::GetString(IDS_MENU_TRAY_ABOUT));
    Utils::SetMenuString(g_hTrayMenu, 2, TRUE, Language::GetString(IDS_MENU_TRAY_EXIT));

    // Process
    Utils::SetMenuString(g_hProcessMenu, 0, TRUE, Language::GetString(IDS_MENU_PROCESS_SHOW));
    Utils::SetMenuString(g_hProcessMenu, 1, TRUE, Language::GetString(IDS_MENU_PROCESS_HIDE));

    // Refresh menu bar
    DrawMenuBar(g_hDlgMain);
}

static void UpdateTabLanguage()
{
    std::vector<const TCHAR *> names;
    for (unsigned int i = 0; i < g_plugins.size(); i++)
    {
        names.push_back(g_plugins[i]->GetName());
    }

    Utils::TabSetText(GetDlgItem(g_hDlgMain, IDT_VIEW), g_plugins.size(), &names[0]);
}

static void UpdateProcessListLanguage() // Process List
{
    Utils::ListViewSetColumnText(GetDlgItem(g_hDlgMain, IDL_PROCESS), 5, 
        Language::GetString(IDS_PLIST_UID), 
        Language::GetString(IDS_PLIST_PROCESS),
        Language::GetString(IDS_PLIST_TXRATE), 
        Language::GetString(IDS_PLIST_RXRATE),
        Language::GetString(IDS_PLIST_FULLPATH));
}

static void UpdateViewLanguage()
{
    ShowWindow(g_hCurPage, SW_HIDE);
    ShowWindow(g_hCurPage, SW_SHOW);
}

static void UpdateLanguage()
{
    UpdateMenuLanguage();
    UpdateTabLanguage();
    UpdateProcessListLanguage();
    UpdateViewLanguage();
}

///----------------------------------------------------------------------------------------------// 
///                                    Called by Message Handlers                                //
///----------------------------------------------------------------------------------------------//
static void InitDatabase()
{
    // Netmon.db consists of following tables:
    //
    // Process
    //    - UID          [Key] : Integer
    //    - Name               : Varchar(64)
    //
    // Note:
    //    - In SQLite 3.x, the "Integer" storage class may refer to data type:
    //      int8, int16, int24, int32, int48 or int64.
    //
    //    - There is only one table created by Netmon core.
    //      Other tables may be created by different plugins (views), and they are
    //      maintained by different plugins
    if (!SQLite::TableExist(TEXT("Process")))
    {
        SQLite::Exec(TEXT("Create Table Process(")
                     TEXT("    UID            Integer,")
                     TEXT("    Name           Varchar(260),")
                     TEXT("    FullPath       Varchar(260),")
                     TEXT("    ")
                     TEXT("    Primary Key (UID)")
                     TEXT(");"), true);

        // Add some init data
        Utils::InsertProcess(TEXT("Unknown"), TEXT("-"));
        Utils::InsertProcess(TEXT("System"), TEXT("-"));
        Utils::InsertProcess(TEXT("svchost.exe"), TEXT("-"));
    }
}

static void CreateViewMenuItems()
{
    HMENU hMenuMain = GetMenu(g_hDlgMain);
    HMENU hMenuView = GetSubMenu(hMenuMain, 1);

    // Get Device Names
    for (unsigned int i = 0; i < g_plugins.size(); i++)
    {
        const TCHAR *name = g_plugins[i]->GetName();

        // Create menu item
        if (i == 0)
        {
            ModifyMenu(hMenuView, 0, MF_BYPOSITION | MF_STRING, IDM_VIEW_FIRST + 0, name);
        }
        else
        {
            InsertMenu(hMenuView, i, MF_BYPOSITION | MF_STRING, IDM_VIEW_FIRST + i, name);
        }
    }
}

static void CreateAdapterMenuItems()
{
    HMENU hMenuMain = GetMenu(g_hDlgMain);
    HMENU hMenuView = GetSubMenu(hMenuMain, 1);
    HMENU hMenuAdapter = GetSubMenu(hMenuView, g_plugins.size() + 1);

    if (g_nAdapters == 0)
    {
        DeleteMenu(hMenuAdapter, 0, MF_BYPOSITION);
    }
    else
    {
        // Update menu
        for (int i = 0; i < g_nAdapters; i++)
        {
            if (i == 0 )
            {
                ModifyMenu(hMenuAdapter, 0, MF_BYPOSITION | MF_STRING, 
                    IDM_VIEW_ADAPTER_FIRST + 0, g_szAdapterNames[0]);
            }
            else
            {
                AppendMenu(hMenuAdapter, MF_STRING, 
                    IDM_VIEW_ADAPTER_FIRST + i, g_szAdapterNames[i]);
            }
        }

        // Check Default Adapter
        CheckMenuRadioItem(hMenuMain,
            IDM_VIEW_ADAPTER_FIRST, 
            IDM_VIEW_ADAPTER_FIRST + g_nAdapters - 1, 
            IDM_VIEW_ADAPTER_FIRST + g_iAdapter, MF_BYCOMMAND);
    }
}

//static void CreateLanguageMenuItems()
//{
//    HMENU hMenuMain = GetMenu(g_hDlgMain);
//    /*HMENU hMenuOptions = GetSubMenu(hMenuMain, 2);
//    HMENU hMenuLanguage = GetSubMenu(hMenuOptions, 0);*/
//
//    // Get Device Names
//    for (int i = 0; i < g_nLanguage; i++)
//    {
//        // Build menu item string
//        TCHAR szEnglishName[256];
//        TCHAR szNativeName[256];
//        TCHAR szMenuItem[256];
//        Language::GetName(i, szEnglishName, 256, szNativeName, 256);
//
//        if (_tcscmp(szEnglishName, szNativeName) == 0)
//        {
//            _stprintf_s(szMenuItem, 256, szEnglishName);
//        }
//        else
//        {
//            _stprintf_s(szMenuItem, 256, TEXT("%s (%s)"), szEnglishName, szNativeName);
//        }
//
//        // Create menu item
//        if (i == 0)
//        {
//            ModifyMenu(hMenuLanguage, 
//                0, MF_BYPOSITION | MF_STRING, IDM_OPTIONS_LANGUAGE_FIRST + 0, szMenuItem);
//        }
//        else
//        {
//            AppendMenu(hMenuLanguage, MF_STRING, IDM_OPTIONS_LANGUAGE_FIRST + i, szMenuItem);
//        }
//    }
//}


static void InitUI(HWND hWnd)
{
    NOTIFYICONDATA nti; 
    HMENU hMainMenu;
  /*  HMENU hViewMenu;*/
   /* HMENU hOptionsMenu;
    HMENU hLanguageMenu;*/
    HBRUSH hBrush;
    HDC hDc;

    // Set Window Size
    MoveWindow(hWnd, 100, 100, 721, 446, FALSE);

    // Load Icon
    HICON hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(ICO_MAIN));
    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    // Load Process Menu
    g_hProcessMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDM_PROCESS));
    g_hProcessMenu = GetSubMenu(g_hProcessMenu, 0);

    // Load Tray Icon Menu
    g_hTrayMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDM_TRAY)); 
    g_hTrayMenu = GetSubMenu(g_hTrayMenu, 0);

    // Create Tray Icon
    nti.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(ICO_MAIN)); 
    nti.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE; 
    nti.hWnd = hWnd; 
    nti.uID = 0;
    nti.uCallbackMessage = WM_USER_TRAY; 
    _tcscpy_s(nti.szTip, _countof(nti.szTip), TEXT("Netmon")); 

    Shell_NotifyIcon(NIM_ADD, &nti); 

    // Init main menu
    hMainMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDM_MAIN));
    SetMenu(hWnd, hMainMenu);

    EnableMenuItem(hMainMenu, IDM_FILE_CAPTURE, MF_ENABLED);
    EnableMenuItem(hMainMenu, IDM_FILE_STOP, MF_GRAYED);

   /* hViewMenu = GetSubMenu(hMainMenu, 1);*/
    /*hOptionsMenu = GetSubMenu(hMainMenu, 2);
    hLanguageMenu = GetSubMenu(hOptionsMenu, 0);*/

    //CreateLanguageMenuItems();
   /* CheckMenuRadioItem(hLanguageMenu, 
        0, g_nLanguage - 1, g_iCurLanguage, MF_BYPOSITION);*/

    CheckMenuItem(hMainMenu, IDM_VIEW_SHOW_HIDDEN, MF_BYCOMMAND | MF_CHECKED); // Hidden State
    g_bShowHidden = true;

    // Init Sidebar GDI Objects
    hDc = GetDC(hWnd);

   /* g_hDcSidebarBg = CreateCompatibleDC(hDc);
    g_hDcSidebarBuf = CreateCompatibleDC(hDc);*/
    
    //g_hBmpSidebarBg = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_SIDEBAR));
    //g_hBmpSidebarBuf = CreateCompatibleBitmap(hDc, 50, 2000); // 2000 pixels in height, 
    //                                                          // which is supposed to be enough
    //SelectObject(g_hDcSidebarBg, g_hBmpSidebarBg);
    //SelectObject(g_hDcSidebarBuf, g_hBmpSidebarBuf);

    g_hDcStart = CreateCompatibleDC(hDc);
    g_hDcStartHover = CreateCompatibleDC(hDc);
    g_hDcStop = CreateCompatibleDC(hDc);
    g_hDcStopHover = CreateCompatibleDC(hDc);

    g_hBmpStart = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_START));
    g_hBmpStartHover = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_START_HOVER));
    g_hBmpStop = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_STOP));
    g_hBmpStopHover = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_STOP_HOVER));

    SelectObject(g_hDcStart, g_hBmpStart);
    SelectObject(g_hDcStartHover, g_hBmpStartHover);
    SelectObject(g_hDcStop, g_hBmpStop);
    SelectObject(g_hDcStopHover, g_hBmpStopHover);

    SelectObject(g_hDcSidebarBuf, GetStockObject(NULL_PEN));

    hBrush = CreateSolidBrush(RGB(18, 98, 184));
    SelectObject(g_hDcSidebarBuf, hBrush);

    ReleaseDC(hWnd, hDc);
}

//static void DrawSidebar()
//{
//    HDC hDcSidebar = GetDC(g_hDlgMain);
//
//    // Paint to buffer
//    Rectangle(g_hDcSidebarBuf, 0, 0, g_iSidebarWidth, g_iSidebarHeight + 1);
//    BitBlt(g_hDcSidebarBuf, 0, g_iSidebarHeight - 446, 50, 446, g_hDcSidebarBg, 0, 0, SRCCOPY);
//
//    if (!g_bCapture && g_enumHoverState == Start)
//    {
//        BitBlt(g_hDcSidebarBuf, 15, g_iSidebarHeight - 60, 18, 18, g_hDcStartHover, 0, 0, SRCCOPY);
//        BitBlt(g_hDcSidebarBuf, 15, g_iSidebarHeight - 33, 18, 18, g_hDcStop, 0, 0, SRCCOPY);
//    }
//    else if (g_bCapture && g_enumHoverState == Stop)
//    {
//        BitBlt(g_hDcSidebarBuf, 15, g_iSidebarHeight - 60, 18, 18, g_hDcStart, 0, 0, SRCCOPY);
//        BitBlt(g_hDcSidebarBuf, 15, g_iSidebarHeight - 33, 18, 18, g_hDcStopHover, 0, 0, SRCCOPY);
//    }
//    else
//    {
//        BitBlt(g_hDcSidebarBuf, 15, g_iSidebarHeight - 60, 18, 18, g_hDcStart, 0, 0, SRCCOPY);
//        BitBlt(g_hDcSidebarBuf, 15, g_iSidebarHeight - 33, 18, 18, g_hDcStop, 0, 0, SRCCOPY);
//    }
//
//    // Write to screen
//    BitBlt(hDcSidebar, 0, 0, g_iSidebarWidth, g_iSidebarHeight + 1, g_hDcSidebarBuf, 0, 0, SRCCOPY);
//
//    ReleaseDC(g_hDlgMain, hDcSidebar);
//}

static void EnumDevices()
{
    PcapSource source;

    // Init Filter
    if (!source.Initialize())
    {
        MessageBox(g_hDlgMain, 
            TEXT("Cannot initizlize WinPcap library.\n")
            TEXT("Please make sure WinPcap version 4.1.2 is correctly installed."), 
            TEXT("Error"), MB_OK | MB_ICONWARNING);

        EnableMenuItem(GetMenu(g_hDlgMain), IDM_FILE_CAPTURE, MF_GRAYED);
        return;
    }

    // Find Devices
    g_nAdapters = source.EnumDevices();

    if (g_nAdapters <= 0)
    {
        MessageBox(g_hDlgMain, 
            TEXT("No network adapters has been found on this machine."), 
            TEXT("Error"), MB_OK | MB_ICONWARNING);

        EnableMenuItem(GetMenu(g_hDlgMain), IDM_FILE_CAPTURE, MF_GRAYED);
    }
    else // Save Device Names
    {
        for(int i = 0; i < g_nAdapters; i++)
        {
            if (i < _countof(g_szAdapterNames))
            {
                source.GetDeviceName(i, g_szAdapterNames[i], 256);
            }
        }
    }
}

static void ProfileInit(HWND hWnd)
{
    // Initialize
    g_profile.Init(TEXT("Netmon.ini"), TEXT("Netmnon Profile v2"));

    // Set profile defaults
    g_profile.RegisterDefault(TEXT("Adapter"), new ProfileStringItem(g_szAdapterNames[g_iAdapter]));
    g_profile.RegisterDefault(TEXT("AutoStart"), new ProfileStringItem(TEXT("")));
    g_profile.RegisterDefault(TEXT("AutoCapture"), new ProfileBoolItem(false));

    g_profile.RegisterDefault(TEXT("RtViewEnabled"), new ProfileBoolItem(true));
    g_profile.RegisterDefault(TEXT("MtViewEnabled"), new ProfileBoolItem(true));
    g_profile.RegisterDefault(TEXT("StViewEnabled"), new ProfileBoolItem(false));
    g_profile.RegisterDefault(TEXT("DtViewEnabled"), new ProfileBoolItem(false));

    g_profile.RegisterDefault(TEXT("HiddenProcess"), new ProfileIntListItem());
    g_profile.RegisterDefault(TEXT("ShowHidden"), new ProfileBoolItem(true));
    g_profile.RegisterDefault(TEXT("Language"), new ProfileStringItem(TEXT("English")));

    // Load Netmon.ini
    g_profile.Load();

    // Set Default Language
    const TCHAR *szLanguage = g_profile.GetString(TEXT("Language"))->value.data();

    for (int i = 0; i < g_nLanguage; i++)
    {
        TCHAR szEnglishName[64];
        TCHAR szNativeName[64];
        Language::GetName(i, szEnglishName, 64, szNativeName, 64);
        if (_tcscmp(szEnglishName, szLanguage) == 0)
        {
            Language::Select(g_iCurLanguage = i);

            // Update language menu radio button
            HMENU hOptionsMenu = GetSubMenu(GetMenu(hWnd), 2);
            HMENU hLanguageMenu = GetSubMenu(hOptionsMenu, 0);
            CheckMenuRadioItem(hLanguageMenu, 0, g_nLanguage - 1, g_iCurLanguage, MF_BYPOSITION);

            break;
        }
    }

    // Update Hidden State
    std::vector<int> hiddenProcesses = g_profile.GetIntList(TEXT("HiddenProcess"))->value;
    
    for (unsigned int i = 0; i < hiddenProcesses.size(); i++)
    {
        ProcessModel::HideProcess(hiddenProcesses[i]);
    }
    ProcessView::Update(true);

    // Select default adapter
    const TCHAR *szAdapter = g_profile.GetString(TEXT("Adapter"))->value.data();

    for(int i =  0; i < g_nAdapters; i++)
    {
        if (_tcscmp(szAdapter, g_szAdapterNames[i]) == 0 )
        {
            g_iAdapter = i;
            break;
        }
    }

    // If AutoCaptue = 1, start capture immediately
    bool autoCapture = g_profile.GetBool(TEXT("AutoCapture"))->value;
    if (g_nAdapters > 0 && autoCapture)
    {
        PostMessage(hWnd, WM_COMMAND, IDM_FILE_CAPTURE, 0);
    }

    // Set the "Show Hidden Process" option
    bool showHidden = g_profile.GetBool(TEXT("ShowHidden"))->value;

    if (!showHidden) // Hide processes when necessary
    {
        ProcessView::HideProcesses();
        CheckMenuItem(GetMenu(hWnd), IDM_VIEW_SHOW_HIDDEN, MF_BYCOMMAND | MF_UNCHECKED);
        g_bShowHidden = false;
    }
}

static void ResizeChildWindow(HWND hWnd)
{
    RECT stRect;
    GetWindowRect(GetDlgItem(hWnd, IDT_VIEW), &stRect);

    stRect.bottom -= stRect.top;
    stRect.right -= stRect.left;
    stRect.left = 0;
    stRect.top = 0;

    TabCtrl_AdjustRect(GetDlgItem(hWnd, IDT_VIEW), FALSE, &stRect);

    SetWindowPos(g_hCurPage, HWND_TOP, 
        stRect.left, stRect.top, stRect.right - stRect.left, stRect.bottom - stRect.top, 
        SWP_SHOWWINDOW);
}

static void Exit(HWND hWnd, bool restart)
{
    // Delete Tray Icon
    NOTIFYICONDATA nti; 

    nti.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(ICO_MAIN)); 
    nti.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE; 
    nti.hWnd = hWnd; 
    nti.uID = 0;
    nti.uCallbackMessage = WM_USER_TRAY; 
    _tcscpy_s(nti.szTip, _countof(nti.szTip), TEXT("Netmon")); 

    Shell_NotifyIcon(NIM_DELETE, &nti);

    // Delete GDI Objects
    DeleteDC(g_hDcSidebarBg);
    DeleteDC(g_hDcSidebarBuf);

    DeleteObject(g_hBmpSidebarBg);
    DeleteObject(g_hBmpSidebarBuf);

    DeleteDC(g_hDcStart);
    DeleteDC(g_hDcStartHover);
    DeleteDC(g_hDcStop);
    DeleteDC(g_hDcStopHover);

    DeleteObject(g_hBmpStart);
    DeleteObject(g_hBmpStartHover);
    DeleteObject(g_hBmpStop);
    DeleteObject(g_hBmpStopHover);

    // Close Plugins
    for (unsigned int i = 0; i < g_plugins.size(); i++)
    {
        delete g_plugins[i];
    }

    // End SQLite
    SQLite::Close();

    // Restart
    if (restart)
    {
        // Get full path name of Netmon.exe
        TCHAR szCurrentExe[MAX_PATH];
        GetModuleFileName(0, szCurrentExe, MAX_PATH);
    
        // Start Netmon (silent mode)
        Utils::StartProcess(szCurrentExe, TEXT("-s"), FALSE);
    }

    // Exit
    DestroyWindow(hWnd);
    PostQuitMessage(0);
}

static void StopTimer(HWND hWnd)
{
    // Normally, you do not need to stop timer. 
    // The timer is still on even if capture has been stopped.
    // However, when Netmon is going to be restarted, we have to make sure that
    // TimerProc will NEVER be executed again before cleaning up
    KillTimer(hWnd, 1);
    Sleep(1100);
}

///----------------------------------------------------------------------------------------------//
///                                    L2 Message Handlers                                       //
///----------------------------------------------------------------------------------------------//
//static void OnHomepage()
//{
//    ShellExecute(0, 0, TEXT("http://www.cnblogs.com/F-32/"), 0, 0, 0);
//}

//static void OnHelp()
//{
//    ShellExecute(0, TEXT("open"), TEXT("Netmon.chm"), 0, 0, SW_SHOW);
//}

static void OnAbout(HWND hWnd)
{
    DialogBoxParam(g_hInstance, TEXT("DLG_ABOUT"), g_hDlgMain, ProcDlgAbout, 0);
}

static void OnSelChanged(HWND hWnd, HWND hTab)
{
    // Get the Index of the Selected Tab.
    int i = TabCtrl_GetCurSel(hTab);
    int n = g_plugins.size(); // total number of tabs

    // Get dialog procedure ptr and template name
    DLGPROC proc = g_plugins[i]->GetDialogProc();
    const TCHAR *name = g_plugins[i]->GetTemplateName();

    // Update menu
    HMENU hViewMenu = GetSubMenu(GetMenu(hWnd), 1);
    CheckMenuRadioItem(hViewMenu, 0, n - 1, i, MF_BYPOSITION);

    // Destroy the Current Child Dialog
    if (g_hCurPage != NULL) 
    {
        SendMessage(g_hCurPage, WM_CLOSE, 0, 0);
    }

    // Create New Dialog
    g_hCurPage = CreateDialogParam(g_hInstance, name, hTab, proc, NULL);

    // Initialize the size of the new window
    ResizeChildWindow(hWnd);

    return;
}

static void OnViewSwitch(HWND hWnd, WPARAM wParam)
{
    int index = wParam - IDM_VIEW_FIRST;
    TabCtrl_SetCurSel(GetDlgItem(hWnd, IDT_VIEW), index);

    // Update menu
    HMENU hViewMenu = GetSubMenu(GetMenu(hWnd), 1);
    CheckMenuRadioItem(hViewMenu, 0, g_plugins.size() - 1, index, MF_BYPOSITION);

    // Update view
    OnSelChanged(hWnd, GetDlgItem(hWnd, IDT_VIEW));
}

static void OnAdapterSelected(HWND hWnd, WPARAM wParam)
{
    if (g_bCapture == true )
    {
    }
    else
    {
        g_iAdapter = wParam - IDM_VIEW_ADAPTER_FIRST;

        CheckMenuRadioItem(GetMenu(hWnd), 
            IDM_VIEW_ADAPTER_FIRST, 
            IDM_VIEW_ADAPTER_FIRST + g_nAdapters - 1, 
            IDM_VIEW_ADAPTER_FIRST + g_iAdapter, MF_BYCOMMAND);
    }
}

static void OnHiddenStateChanged(HWND hWnd)
{
    HMENU hMenu = GetMenu(hWnd);
    UINT uMenuState = LOWORD(GetMenuState(hMenu, IDM_VIEW_SHOW_HIDDEN, MF_BYCOMMAND));

    if (uMenuState & MF_CHECKED) // Visible -> Hidden
    {
        ProcessView::HideProcesses();
        CheckMenuItem(hMenu, IDM_VIEW_SHOW_HIDDEN, MF_BYCOMMAND | MF_UNCHECKED);
        g_profile.SetValue(TEXT("ShowHidden"), new ProfileBoolItem(false));
        g_bShowHidden = false;
    }
    else // Hidden ->Visible
    {
        ProcessView::ShowProcesses();
        CheckMenuItem(hMenu, IDM_VIEW_SHOW_HIDDEN, MF_BYCOMMAND | MF_CHECKED);
        g_profile.SetValue(TEXT("ShowHidden"), new ProfileBoolItem(true));
        g_bShowHidden = true;
    }
}

//static void OnLanguageSelected(HWND hWnd, WPARAM wParam)
//{
//    if (wParam - IDM_OPTIONS_LANGUAGE_FIRST != g_iCurLanguage )
//    {
//        // Update language
//        g_iCurLanguage = wParam - IDM_OPTIONS_LANGUAGE_FIRST;
//        Language::Select(g_iCurLanguage);
//        UpdateLanguage();
//
//        // Update Profile
//        TCHAR szEnglishName[64];
//        TCHAR szNativeName[64];
//        Language::GetName(g_iCurLanguage, szEnglishName, 64, szNativeName, 64);
//        g_profile.SetValue(TEXT("Language"), new ProfileStringItem(szEnglishName));
//
//        // Update language menu radio button
//        HMENU hOptionsMenu = GetSubMenu(GetMenu(hWnd), 2);
//        HMENU hLanguageMenu = GetSubMenu(hOptionsMenu, 0);
//        CheckMenuRadioItem(hLanguageMenu, 0, g_nLanguage - 1, g_iCurLanguage, MF_BYPOSITION);
//    }
//}
//deleted by Oleh
//static void OnPreferences(HWND hWnd)
//{
//    DialogBoxParam(g_hInstance, TEXT("DLG_PREFERENCES"), g_hDlgMain, ProcDlgPreferences, NULL);
//}

static void OnProcessChanged(HWND hWnd, LPARAM lParam)
{
    if(((NMHDR *)lParam)->code == LVN_ITEMCHANGED)
    {
        NMLISTVIEW *lpstListView = (NMLISTVIEW *)lParam;

        // Selection Changed
        if (lpstListView->iSubItem == 0 &&
            lpstListView->uNewState == (LVIS_FOCUSED | LVIS_SELECTED) &&
            lpstListView->uChanged == LVIF_STATE )
        {
            TCHAR szPUID[16];
            Utils::ListViewGetText(
                GetDlgItem(hWnd, IDL_PROCESS), lpstListView->iItem, 0, szPUID, 16);
            int puid = _tstoi(szPUID);

            for (unsigned int i = 0; i < g_plugins.size(); i++)
            {
                g_plugins[i]->SetProcess(puid);
            }
        }
    }
    else if(((NMHDR *)lParam)->code == NM_CLICK )
    {
        int index = ((NMITEMACTIVATE *)lParam)->iItem;

        if (index == -1 )
        {
            for (unsigned int i = 0; i < g_plugins.size(); i++)
            {
                g_plugins[i]->SetProcess(-1);
            }
        }
    }
}

static void OnCustomDraw(HWND hWnd, LPARAM lParam)
{
    if (g_bShowHidden) // Hidden processes are gray
    {
        NMLVCUSTOMDRAW *cd = (NMLVCUSTOMDRAW *)lParam;
        if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
        {
            SetDlgMsgResult(hWnd, WM_NOTIFY, CDRF_NOTIFYSUBITEMDRAW);
        }
        else if(cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
        {
            std::vector<bool> hiddenProcesses;
            ProcessModel::ExportHiddenState(hiddenProcesses);

            if (hiddenProcesses[cd->nmcd.dwItemSpec]) // Hidden
            {
                cd->clrText = RGB(192, 192, 192);
            }
            else // Visible
            {
                cd->clrText = RGB(0, 0, 0);
            }
            SetDlgMsgResult(hWnd, WM_NOTIFY, CDRF_NOTIFYSUBITEMDRAW);
        }
    }
    // else, do nothing
}

static void OnRightClick(HWND hWnd, LPARAM lParam)
{
    NMITEMACTIVATE *ia = (NMITEMACTIVATE *)lParam;
    int index = ia->iItem;
    if (index != -1 ) 
    {
        if (!ProcessView::IsHidden())
        {
            // Get Hidden State
            std::vector<bool> hidden;
            ProcessModel::ExportHiddenState(hidden);

            if ((unsigned int)index < hidden.size())
            {
                if (hidden[index]) // Hidden
                {
                    EnableMenuItem(g_hProcessMenu, IDM_PROCESS_SHOW, MF_ENABLED);
                    EnableMenuItem(g_hProcessMenu, IDM_PROCESS_HIDE, MF_GRAYED);
                }
                else // Visible
                {
                    EnableMenuItem(g_hProcessMenu, IDM_PROCESS_SHOW, MF_GRAYED);
                    EnableMenuItem(g_hProcessMenu, IDM_PROCESS_HIDE, MF_ENABLED);
                }
            }
        }
        else
        {
            EnableMenuItem(g_hProcessMenu, IDM_PROCESS_SHOW, MF_GRAYED);
            EnableMenuItem(g_hProcessMenu, IDM_PROCESS_HIDE, MF_ENABLED);
        }

        TrackPopupMenu(g_hProcessMenu, TPM_TOPALIGN | TPM_LEFTALIGN,  
            GET_X_LPARAM(GetMessagePos()), GET_Y_LPARAM(GetMessagePos()), 0, hWnd, NULL); 
    }
}

static void OnShowProcess(HWND hList)
{
    // Get Selected Index
    int index = Utils::ListViewGetSelectedItemIndex(hList);

    // Get PUID
    TCHAR buf[16];
    Utils::ListViewGetText(hList, index, 0, buf, 16);

    // Set Hidden State
    ProcessModel::ShowProcess(_tstoi(buf));
}

static void OnHideProcess(HWND hList)
{
    // Get Selected Index
    int index = Utils::ListViewGetSelectedItemIndex(hList);

    // Get PUID
    TCHAR buf[16];
    Utils::ListViewGetText(hList, index, 0, buf, 16);

    // Set Hidden State
    ProcessModel::HideProcess(_tstoi(buf));
    if (ProcessView::IsHidden())
    {
        for (unsigned int i = 0; i < g_plugins.size(); i++)
        {
            g_plugins[i]->SetProcess(-1);
        }
    }
}

static void OnShowWindow(HWND hWnd)
{
    ShowWindow(hWnd, SW_SHOWNORMAL);
}

static void OnCapture(HWND hWnd)
{
    // Start Thread
    g_bCapture = true;
    g_hCaptureThread = CreateThread(0, 0, CaptureThread, 0, 0, 0);

    EnableMenuItem(GetMenu(hWnd), IDM_FILE_CAPTURE, MF_GRAYED);
    EnableMenuItem(GetMenu(hWnd), IDM_FILE_STOP, MF_ENABLED);
}

static void OnStop(HWND hWnd)
{
    if (g_bCapture == true) // Stop Thread
    {
        g_bCapture = false;
        WaitForSingleObject(g_hCaptureThread, INFINITE);

        EnableMenuItem(GetMenu(hWnd), IDM_FILE_CAPTURE, MF_ENABLED);
        EnableMenuItem(GetMenu(hWnd), IDM_FILE_STOP, MF_GRAYED);
    }
}

static void OnExit(HWND hWnd)
{
    OnStop(hWnd);
    Exit(hWnd, false); // Will not restart
}

///----------------------------------------------------------------------------------------------// 
///                                    L1 Message Handlers                                       //
///----------------------------------------------------------------------------------------------//
static void WINAPI OnTimer(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    ProcessModel::OnTimer();

    // Save database every 30 minutes
    SYSTEMTIME time;
    GetLocalTime(&time);

    if (time.wSecond == 0 && (time.wMinute == 0 || time.wMinute == 30))
    {
        for (unsigned int i = 0; i < g_plugins.size(); i++)
        {
            g_plugins[i]->SaveDatabase();
        }
        SQLite::Flush();
    }
}

static void OnInitDialog(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    // Save hWnd
    g_hDlgMain = hWnd;

    // Initialize UI
    InitUI(hWnd);

    // Init Database
    TCHAR dbPath[MAX_PATH];
    Utils::GetFilePathInCurrentDir(dbPath, MAX_PATH, TEXT("Netmon.db"));
    SQLite::Open(dbPath);
    InitDatabase();

    // Init ListView (should call this after InitDatabase)
    ProcessView::Init(GetDlgItem(hWnd, IDL_PROCESS));

    // Enum Devices (should call this before ProfileInit)
    EnumDevices();

    // Init Profile (should call this before plugins loaded)
    ProfileInit(hWnd);

    // Init Plugins
    bool bRtViewEnabled = g_profile.GetBool(TEXT("RtViewEnabled"))->value;
    bool bMtViewEnabled = g_profile.GetBool(TEXT("MtViewEnabled"))->value;
    bool bStViewEnabled = g_profile.GetBool(TEXT("StViewEnabled"))->value;
    bool bDtViewEnabled = g_profile.GetBool(TEXT("DtViewEnabled"))->value;

    // We do not want all views disabled
    if (!bRtViewEnabled && !bMtViewEnabled && !bStViewEnabled && !bDtViewEnabled)
    {
        g_profile.SetValue(TEXT("RtViewEnabled"), new ProfileBoolItem(true));
        bRtViewEnabled = true;
    }

    if (bRtViewEnabled) g_plugins.push_back(new RealtimePlugin());
    if (bMtViewEnabled) g_plugins.push_back(new MonthPlugin());
    //if (bStViewEnabled) g_plugins.push_back(new StatisticsPlugin());
   /* if (bDtViewEnabled) g_plugins.push_back(new DetailPlugin());*/

    // Init Menu Items for Views (should call this after plugins loaded)
    CreateViewMenuItems();

    // Init Menu Items for Adapters (should call this after plugins loaded)
    CreateAdapterMenuItems();

    // Init Tab (The names will be updated in UpdateLanguage())
    std::vector<const TCHAR *> names(g_plugins.size(), TEXT(""));
    Utils::TabInit(GetDlgItem(hWnd, IDT_VIEW), g_plugins.size(), &names[0]);

    // Simulate Selection of the First Item. 
    OnSelChanged(hWnd, GetDlgItem(hWnd, IDT_VIEW));

    // Start the Timer that Updates Process List
    SetTimer(hWnd, 1, 1000, OnTimer);

    // Update language
    UpdateLanguage();

    // Show window if option "-h" is not present
    if (!g_bHideWindow)
    {
        ShowWindow(hWnd, SW_SHOW);
    }
}

static void OnClose(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    ShowWindow(hWnd, SW_HIDE); 
}

static void OnQueryEndSession(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    SetDlgMsgResult(hWnd, WM_QUERYENDSESSION, TRUE);
}

static void OnEndSession(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    OnExit(hWnd);
}

static void OnCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (wParam == IDM_FILE_EXIT || wParam == IDM_TRAY_EXIT)
    {
        OnExit(hWnd);
    }
    else if (wParam == IDM_TRAY_SHOW_WINDOW)
    {
        OnShowWindow(hWnd);
    }
    else if (wParam == IDM_FILE_CAPTURE)
    {
        OnCapture(hWnd);
    }
    else if (wParam == IDM_FILE_STOP)
    {
        OnStop(hWnd);
    }
    else if (wParam >= IDM_VIEW_FIRST && wParam < IDM_VIEW_ADAPTER_FIRST) // dyanmic
    {
        OnViewSwitch(hWnd, wParam);
    }
    else if (wParam >= IDM_VIEW_ADAPTER_FIRST && wParam < IDM_OPTIONS_LANGUAGE_FIRST) // dyanmic
    {
        OnAdapterSelected(hWnd, wParam);
    }
    else if (wParam == IDM_VIEW_SHOW_HIDDEN)
    {
        OnHiddenStateChanged(hWnd);
    }
    //else if (wParam >= IDM_OPTIONS_LANGUAGE_FIRST) // dynamic
    //{
    //    OnLanguageSelected(hWnd, wParam);
    //}
    //deleted by Oleh
    /*else if (wParam == IDM_OPTIONS_PREFERENCES)
    {
        OnPreferences(hWnd);
    }*/
    /*else if (wParam == IDM_HELP_HOMEPAGE)
    {
        OnHomepage();
    }
    else if (wParam == IDM_HELP_TOPIC)
    {
        OnHelp();
    }*/
    else if (wParam == IDM_HELP_ABOUT || wParam == IDM_TRAY_ABOUT)
    {
        OnAbout(hWnd);
    }
    else if (wParam == IDM_PROCESS_SHOW)
    {
        OnShowProcess(GetDlgItem(hWnd, IDL_PROCESS));
    }
    else if (wParam == IDM_PROCESS_HIDE)
    {
        OnHideProcess(GetDlgItem(hWnd, IDL_PROCESS));
    }
}

static void OnUserTray(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (lParam == WM_LBUTTONDBLCLK ) 
    {
        OnShowWindow(hWnd);
    }
    else if (lParam == WM_RBUTTONDOWN ) 
    {
        // Show Tray Icon Popup Menu
        POINT point;
        GetCursorPos(&point); 

        // Hide the menu when the user clicks outside of the menu
        SetForegroundWindow(hWnd);
        TrackPopupMenu(g_hTrayMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hWnd, NULL);
        PostMessage(hWnd, WM_NULL, 0, 0);
    }
}

static void OnReconnect(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    OnStop(hWnd);

    // Display a balloon on the tray icon
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = hWnd;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    nid.uID = 0;
    _tcscpy_s(nid.szInfoTitle, _countof(nid.szInfoTitle), 
        Language::GetString(IDS_TRAY_RECONNECT_TITLE));
    _tcscpy_s(nid.szInfo, _countof(nid.szInfo), 
        Language::GetString(IDS_TRAY_RECONNECT_DESC));
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

static void OnClearAndRestart(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    StopTimer(hWnd);
    OnStop(hWnd);

    for (unsigned int i = 0; i < g_plugins.size(); i++)
    {
        Plugin *p = g_plugins[i];

        RealtimePlugin   *rp = dynamic_cast<RealtimePlugin *>  (p);
        MonthPlugin      *mp = dynamic_cast<MonthPlugin *>     (p);
        //deleted by Oleh
        /*StatisticsPlugin *sp = dynamic_cast<StatisticsPlugin *>(p);*/
        /*DetailPlugin     *dp = dynamic_cast<DetailPlugin *>    (p);*/

        bool bRtViewEnabled = g_profile.GetBool(TEXT("RtViewEnabled"))->value;
        bool bMtViewEnabled = g_profile.GetBool(TEXT("MtViewEnabled"))->value;
        bool bStViewEnabled = g_profile.GetBool(TEXT("StViewEnabled"))->value;
        bool bDtViewEnabled = g_profile.GetBool(TEXT("DtViewEnabled"))->value;

        // If the plugin is now enabled, and it has been disabled in the preference dialog
        if (rp != NULL && !bRtViewEnabled) rp->ClearDatabase();
        if (mp != NULL && !bMtViewEnabled) mp->ClearDatabase();

       /* if (sp != NULL && !bStViewEnabled) sp->ClearDatabase();*/
       /* if (dp != NULL && !bDtViewEnabled) dp->ClearDatabase();*/
    }

    Exit(hWnd, true); // Will restart
}

static void OnRestart(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    StopTimer(hWnd);
    OnStop(hWnd);
    Exit(hWnd, true);
}

//static void OnPaint(HWND hWnd, WPARAM wParam, LPARAM lParam)
//{
//    PAINTSTRUCT stPS;
//    BeginPaint(hWnd, &stPS);
//
//   /* DrawSidebar();*/
//
//    EndPaint(hWnd, &stPS);
//}

static void OnSize(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    static bool firstRun = true;
    static int initWindowWidth = 0;
    static int initWindowHeight = 0;
    static int prevListHeight = 0;
    static int prevViewHeight = 0;

    // Resize Sidebar, ListView and Tab Control
    int clientWidth = lParam & 0xFFFF;
    int clientHeight = lParam >> 16;

    // The following events are observed on startup:
    //
    // OnSize: client_width = 839, client_height = 408, width = 855, height = 446 ...
    // OnSize: client_width = 839, client_height = 388, width = 855, height = 446
    //
    // The window size remains the same, but client rectangle becomes smaller
    // However, both events should be regarded as "first run"
    if (firstRun)
    {
        // Get current window size
        RECT rect;
        GetWindowRect(hWnd, &rect);

        // Save initial window size
        if (initWindowWidth == 0)
        {
            initWindowWidth = rect.right - rect.left;
            initWindowHeight = rect.bottom - rect.top;
        }
        else
        {
            if ((rect.right - rect.left) == initWindowWidth &&
                (rect.bottom - rect.top) == initWindowHeight)
            {
                // still first run
            }
            else
            {
                firstRun = false;
            }
        }
    }

    if (firstRun)
    {
        MoveWindow(GetDlgItem(hWnd, IDL_PROCESS),
            0 - 1, 0,
            clientWidth - 0 + 2,
            138,
            TRUE);
        MoveWindow(GetDlgItem(hWnd, IDT_VIEW), 
            0 + 6, 138 + 6,
            clientWidth - 0 - 12,
            clientHeight - 138 - 12,
            TRUE);

        prevListHeight = 138;
        prevViewHeight = clientHeight - 138 - 12;
    }
    else // Resize (keep the height of the tab control)
    {
        // Resize
        MoveWindow(GetDlgItem(hWnd, IDL_PROCESS), 
            1, 
            0,
            clientWidth - 2,
            clientHeight - prevViewHeight - 12,
            TRUE);
        MoveWindow(GetDlgItem(hWnd, IDT_VIEW),
            6,
            clientHeight - prevViewHeight - 6,
            clientWidth - 12,
            prevViewHeight,
            TRUE);

        prevListHeight = clientHeight - prevViewHeight - 12;
        prevViewHeight = prevViewHeight;
    }

   /* g_iSidebarWidth = 50;*/
  /*  g_iSidebarHeight = clientHeight;*/

    // Resize the Child Window in Tab Control
    ResizeChildWindow(hWnd);

    // Draw Sidebar
 /*   DrawSidebar();*/
}

static void OnGetMinMaxInfo(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    MINMAXINFO *stMMI = (MINMAXINFO *)lParam;

    stMMI->ptMaxSize.x = GetSystemMetrics(SM_CXFULLSCREEN);
    stMMI->ptMaxSize.y = GetSystemMetrics(SM_CYFULLSCREEN);

    stMMI->ptMaxPosition.x = 0;
    stMMI->ptMaxPosition.y = 0;

    stMMI->ptMinTrackSize.x = 855;
    stMMI->ptMinTrackSize.y = 446;

    stMMI->ptMaxTrackSize.x = GetSystemMetrics(SM_CXFULLSCREEN);
    stMMI->ptMaxTrackSize.y = GetSystemMetrics(SM_CXFULLSCREEN);
}

static void OnNotify(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (wParam == IDT_VIEW )
    {
        if (((NMHDR *)lParam)->code == TCN_SELCHANGE)
        {
            OnSelChanged(hWnd, GetDlgItem(hWnd, IDT_VIEW));
        }
    }
    else if (wParam == IDL_PROCESS)
    {
        if (((NMHDR *)lParam)->code == LVN_ITEMCHANGED ||
            ((NMHDR *)lParam)->code == NM_CLICK )
        {
            OnProcessChanged(hWnd, lParam);
        }
        else if (((NMHDR *)lParam)->code == NM_CUSTOMDRAW)
        {
            OnCustomDraw(hWnd, lParam);
        }
        else if (((NMHDR *)lParam)->code == NM_RCLICK)
        {
            OnRightClick(hWnd, lParam);
        }
    }
}

static void OnMouseMove(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    // Get Mouse Position
    int x = GET_X_LPARAM(lParam); 
    int y = GET_Y_LPARAM(lParam); 

    enum enumHoverState newState = Neither;
    if (x >= 15 && x < 33)
    {
        if (y >= g_iSidebarHeight - 60 && y < g_iSidebarHeight - 42)
        {
            newState = Start;
        }
        else if (y >= g_iSidebarHeight - 33 && y < g_iSidebarHeight - 15)
        {
            newState = Stop;
        }
    }

    //// Update Sidebar when necessary
    //if (g_nAdapters > 0)
    //{
    //    if (g_enumHoverState != newState)
    //    {
    //        g_enumHoverState = newState;
    //        DrawSidebar();
    //    }
    //}

    // Update Cursor
    RECT stListRect;
    GetWindowRect(GetDlgItem(hWnd, IDL_PROCESS), &stListRect);

    if (x > 0 && 
        y > stListRect.bottom - stListRect.top && 
        y < stListRect.bottom - stListRect.top + 10)
    {
        HCURSOR hCursor = LoadCursor(NULL, IDC_SIZENS);
        SetCursor(hCursor);
    }
    else
    {
        HCURSOR hCursor = LoadCursor(NULL, IDC_ARROW);
        SetCursor(hCursor);
    }

    // Drag
    if (g_bDragging)
    {
        // Move Window
        RECT stClientRect;
        GetClientRect(hWnd, &stClientRect);
        int clientWidth = stClientRect.right - stClientRect.left;
        int clientHeight = stClientRect.bottom - stClientRect.top;

        int listHeight = max(138, min(clientHeight - 230, y));
        int tabHeight = clientHeight - listHeight - 12;

        MoveWindow(GetDlgItem(hWnd, IDL_PROCESS), 
            1, 
            0, 
            clientWidth - 2, 
            listHeight, 
            TRUE);
        MoveWindow(GetDlgItem(hWnd, IDT_VIEW), 
            6, 
            listHeight + 6, 
            clientWidth - 12, 
            tabHeight, 
            TRUE);

        // Resize the Child Window in Tab Control
        RECT stRect;
        GetWindowRect(GetDlgItem(hWnd, IDT_VIEW), &stRect);

        stRect.bottom -= stRect.top;
        stRect.right -= stRect.left;
        stRect.left = 0;
        stRect.top = 0;

        TabCtrl_AdjustRect(GetDlgItem(hWnd, IDT_VIEW), FALSE, &stRect);

        SetWindowPos(g_hCurPage, HWND_TOP, 
            stRect.left, stRect.top, stRect.right - stRect.left, stRect.bottom - stRect.top, 
            SWP_SHOWWINDOW);
    }
}

//static void OnLButtonDown(HWND hWnd, WPARAM wParam, LPARAM lParam)
//{
//    // Get Mouse Position
//    int x = GET_X_LPARAM(lParam); 
//    int y = GET_Y_LPARAM(lParam); 
//
//    enum enumHoverState newState = Neither;
//    if (x >= 15 && x < 33)
//    {
//        if (y >= g_iSidebarHeight - 60 && y < g_iSidebarHeight - 42)
//        {
//            newState = Start;
//        }
//        else if (y >= g_iSidebarHeight - 33 && y < g_iSidebarHeight - 15)
//        {
//            newState = Stop;
//        }
//    }
//
//    if (g_nAdapters == 0)
//        return;
//
//    // Drag Start
//    RECT stListRect;
//    GetWindowRect(GetDlgItem(hWnd, IDL_PROCESS), &stListRect);
//
//    if (x > 0 && 
//        y > stListRect.bottom - stListRect.top && 
//        y < stListRect.bottom - stListRect.top + 10)
//    {
//        g_bDragging = true;
//        SetCapture(hWnd);
//    }
//}

//static void OnLButtonUp(HWND hWnd, WPARAM wParam, LPARAM lParam)
//{
//    // Get Mouse Position
//    int x = GET_X_LPARAM(lParam); 
//    int y = GET_Y_LPARAM(lParam); 
//
//    if (g_bDragging)
//    {
//        g_bDragging = false;
//        ReleaseCapture();
//    }
//}

///----------------------------------------------------------------------------------------------//
///                                    Main Dialog Proc                                          //
///----------------------------------------------------------------------------------------------//
static INT_PTR CALLBACK ProcDlgMain(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#define PROCESS_MSG(MSG, HANDLER) if(uMsg == MSG) { HANDLER(hWnd, wParam, lParam); return TRUE; }

  /*  PROCESS_MSG(WM_MOUSEMOVE,            OnMouseMove)*/
    /*PROCESS_MSG(WM_LBUTTONDOWN,          OnLButtonDown)
    PROCESS_MSG(WM_LBUTTONUP,            OnLButtonUp)*/
    PROCESS_MSG(WM_INITDIALOG,           OnInitDialog)      // Init
    PROCESS_MSG(WM_CLOSE,                OnClose)
    PROCESS_MSG(WM_QUERYENDSESSION,      OnQueryEndSession)
    PROCESS_MSG(WM_ENDSESSION,           OnEndSession)
    PROCESS_MSG(WM_COMMAND,              OnCommand)
    PROCESS_MSG(WM_USER_TRAY,            OnUserTray)        // Tray icon messages
    PROCESS_MSG(WM_RECONNECT,            OnReconnect)       // Resume from hibernation
   /* PROCESS_MSG(WM_PAINT,                OnPaint)*/
    PROCESS_MSG(WM_SIZE,                 OnSize)            // Resize controls
    PROCESS_MSG(WM_GETMINMAXINFO,        OnGetMinMaxInfo)   // Set Window's minimun size
    PROCESS_MSG(WM_NOTIFY,               OnNotify)
    PROCESS_MSG(WM_CLEAR_DB_AND_RESTART, OnClearAndRestart)
    PROCESS_MSG(WM_RESTART,              OnRestart)

#undef PROCESS_MSG

    return FALSE;
}

///----------------------------------------------------------------------------------------------//
///                                    WinMain Entry                                             //
///----------------------------------------------------------------------------------------------//
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
    MSG stMsg;

    g_hInstance = hInstance;

    // Single Instance (Create a Named-Pipe)
    HANDLE hPipe = 0;
    
    // Load languages
    g_nLanguage = Language::Load();
    if (g_nLanguage == 0 )
    {
        MessageBox(0, TEXT("Failed to load languages."), TEXT("Error"), MB_OK | MB_ICONWARNING);
        CloseHandle(hPipe);
        return 1;
    }
    else // Select English as default if available
    {
        TCHAR szEnglishName[64];
        TCHAR szNativeName[64];
        for (int i = 0; i < g_nLanguage; i++)
        {
            Language::GetName(i, szEnglishName, 64, szNativeName, 64);
            if (_tcscmp(szEnglishName, TEXT("English")) == 0)
            {
                g_iCurLanguage = i;
                break;
            }
        }
        Language::Select(g_iCurLanguage);
    }
    // Display the window
    CreateDialogParam(g_hInstance, TEXT("DLG_MAIN"), NULL, ProcDlgMain, 0);

    while (GetMessage(&stMsg, NULL, 0, 0) != 0)
    {
        TranslateMessage(&stMsg);
        DispatchMessage(&stMsg);
    }

    // Close the Named-Pipe
    CloseHandle(hPipe);

    // Exit
    return 0;
}
