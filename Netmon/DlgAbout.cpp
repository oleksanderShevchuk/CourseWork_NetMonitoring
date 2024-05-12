
#include "stdafx.h"
#include "DlgAbout.h"

#include "res/resource.h"

#include "utils/Utils.h"
#include "utils/Language.h"

///----------------------------------------------------------------------------------------------//
///                                    Global Variables                                          //
///----------------------------------------------------------------------------------------------//
extern HINSTANCE g_hInstance;
static WNDPROC   g_lpOldProcEdit;

///----------------------------------------------------------------------------------------------//
///                                    The WNDPROC That Makes An Edit Control Readonly           //
///----------------------------------------------------------------------------------------------//
static LRESULT CALLBACK MyProcEdit(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg != WM_CHAR && uMsg != WM_PASTE)
    {
        return CallWindowProc(g_lpOldProcEdit, hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

///----------------------------------------------------------------------------------------------// 
///                                    L1 Message Handlers                                       //
///----------------------------------------------------------------------------------------------//
static void OnInitDialog(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    // Load Icon
    HICON hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(ICO_MAIN));
    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    // Size Dialog
    SetWindowPos(hWnd, HWND_TOP, 140, 140, 569, 320, 0);

    // Init the Read-only Edit Control
    g_lpOldProcEdit =
        (WNDPROC)SetWindowLong(GetDlgItem(hWnd, IDE_THIRD_PARTY), GWL_WNDPROC, (LONG)MyProcEdit);

    // Get Client Rectangle
    RECT stClientRect;

    GetClientRect(hWnd, &stClientRect);

    int x1 = stClientRect.left + 143;
    int y1 = stClientRect.top;
    int x2 = stClientRect.right;
    int y2 = stClientRect.bottom;

    // Move Controls
    SetWindowPos(GetDlgItem(hWnd, IDE_THIRD_PARTY),
        HWND_TOP, x1 + 22, 35, x2 - x1 - 32, y2 - y1 - 80, 0);
    SetWindowPos(GetDlgItem(hWnd, IDB_CLOSE),
        HWND_TOP, x2 - 90, y2 - 34, 80, 24, 0);

    // Set Bitmap
    SendMessage(GetDlgItem(hWnd, IDS_SIDEBAR), STM_SETIMAGE,
        IMAGE_BITMAP, (LPARAM)LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_ABOUT_SIDEBAR)));

    // Set close button text
    SetDlgItemText(hWnd, IDB_CLOSE, Language::GetString(IDS_ABOUT_CLOSE));

    // Set Third Party Infomation
    SetDlgItemText(hWnd, IDE_THIRD_PARTY,
        TEXT("   About Network Monitoring Application\r\n")
        TEXT("\r\n")
        TEXT("   Version: 1.0\r\n")
        TEXT("   Developer: Shevchuk Oleksandr\r\n")//Oleg Vozniuk
        TEXT("\r\n")
        // Description
        TEXT("   Description:\r\n")
        TEXT("   Network Monitoring is a powerful and convenient tool for\r\n")
        TEXT("   network monitoring.With it, you can observe the state \r\n")
        TEXT("   of your network in real - time, receive event \r\n")
        TEXT("   notifications, and analyze traffic.\r\n")
        TEXT("\r\n")
        // Key Features
        TEXT("   Key Features:\r\n")
        TEXT("   1. Real-Time Monitoring: Monitor your network's activity\r\n")
        TEXT("   in real-time. Track data transfer speeds, connections, and\r\n")
        TEXT("   other metrics.\r\n")
        TEXT("   2. Event Notifications: Receive notifications about important\r\n")
        TEXT("   events in your network, such as connection losses, attacks,\r\n")
        TEXT("   or hardware failures.\r\n")
        TEXT("   3. Traffic Analysis: Study your network traffic to identify\r\n")
        TEXT("   bottlenecks, pinpoint resource overutilization, and detect\r\n")
        TEXT("   anomalies.\r\n")
        TEXT("   4. Customizable Configuration: Tailor the application to \r\n")
        TEXT("   your needs by setting monitoring and notification parameters.\r\n")
        TEXT("   \r\n")
        // How to Use
        TEXT("   How to Use:\r\n")
        TEXT("   Simply download the application to your device, enable \r\n")
        TEXT("   monitoring, and receive up-to-date information about your \r\n")
        TEXT("   network anytime.\r\n")
        TEXT("   \r\n")
        // Support
        TEXT("   Support:\r\n")
        TEXT("   If you have any questions or suggestions regarding the\r\n")
        TEXT("   Network Monitor application, please contact us.\r\n")
        TEXT("\r\n")
        TEXT("   Thank you for choosing Network Monitor!"));
}

static void OnPaint(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT stPS;
    BeginPaint(hWnd, &stPS);

    // Get Client Rectangle
    RECT stClientRect;
    GetClientRect(hWnd, &stClientRect);

    int x1 = stClientRect.left + 143;
    int y1 = stClientRect.top - 65;
    int x2 = stClientRect.right;
    int y2 = stClientRect.bottom;

    // Create fonts
    HFONT hFontDefault;
    HFONT hFontTitle  = Utils::MyCreateFont(TEXT("MS Shell Dlg 2"), 14, 0, true);
    HFONT hFontTitle2 = Utils::MyCreateFont(TEXT("MS Shell Dlg 2"), 14, 0, true);
    HFONT hFontText   = Utils::MyCreateFont(TEXT("MS Shell Dlg 2"), 14, 0, false);

    // Get version string
    TCHAR szVersionNumber[64];
    TCHAR szVersion[64];
    const TCHAR *szVersionFormat = Language::GetString(IDS_ABOUT_NETMON);

    Utils::GetVersionString(szVersionNumber, 64);
    _stprintf_s(szVersion, 64, szVersionFormat, szVersionNumber);
    
    // Clear Background
    Rectangle(stPS.hdc, x1 - 1, y1 - 1, x2 + 1, y2 + 1);

    // Draw Text

    // - Netmon 1.2.1
    //SetTextAlign(stPS.hdc, TA_LEFT | TA_BOTTOM);
    //SetTextColor(stPS.hdc, RGB(0x17, 0x14, 0xA3));
    hFontDefault = (HFONT) SelectObject(stPS.hdc, hFontTitle);
    //TextOut(stPS.hdc, x1 + 10, y1 + 24, szVersion, _tcslen(szVersion));

    // - Line
    //MoveToEx(stPS.hdc, x1 + 10, y1 + 27, 0);
    //LineTo(stPS.hdc, x2 - 10, y1 + 27);

    // - Copyright (C) ...
    // - All rights ...
    //SetTextAlign(stPS.hdc, TA_LEFT | TA_TOP);
    //SetTextColor(stPS.hdc, RGB(0x00, 0x00, 0x00));
    //SelectObject(stPS.hdc, hFontText);

    //TextOut(stPS.hdc, x1 + 22, y1 + 34, Language::GetString(IDS_ABOUT_COPYRIGHT),  
      //  _tcslen(Language::GetString(IDS_ABOUT_COPYRIGHT)));
    //TextOut(stPS.hdc, x1 + 22, y1 + 48, Language::GetString(IDS_ABOUT_ALL_RIGHTS), 
      //  _tcslen(Language::GetString(IDS_ABOUT_ALL_RIGHTS)));

    // - Third parties
    SetTextAlign(stPS.hdc, TA_LEFT | TA_BOTTOM);
    SetTextColor(stPS.hdc, RGB(0x17, 0x14, 0xA3));
    SelectObject(stPS.hdc, hFontTitle2);
    TextOut(stPS.hdc, x1 + 10, y1 + 87, Language::GetString(IDS_ABOUT_THIRD_PARTIES), 
        _tcslen(Language::GetString(IDS_ABOUT_THIRD_PARTIES)));

    // - Line
    MoveToEx(stPS.hdc, x1 + 10, y1 + 90, 0);
    LineTo(stPS.hdc, x2 - 10, y1 + 90);

    SelectObject(stPS.hdc, hFontDefault);

    DeleteObject(hFontTitle);
    DeleteObject(hFontTitle2);
    DeleteObject(hFontText);

    EndPaint(hWnd, &stPS);
}

static void OnClose(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    EndDialog(hWnd, 0);
}

static void OnCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (wParam == IDB_CLOSE )
    {
        EndDialog(hWnd, 0);
    }
}

///----------------------------------------------------------------------------------------------//
///                                    About Dialog Proc                                         //
///----------------------------------------------------------------------------------------------//
INT_PTR CALLBACK ProcDlgAbout(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#define PROCESS_MSG(MSG, HANDLER) if(uMsg == MSG) { HANDLER(hWnd, wParam, lParam); return TRUE; }

    PROCESS_MSG(WM_INITDIALOG, OnInitDialog)
    PROCESS_MSG(WM_CLOSE,      OnClose)
    PROCESS_MSG(WM_COMMAND,    OnCommand)
    PROCESS_MSG(WM_PAINT,      OnPaint)

#undef PROCESS_MSG

    return FALSE;
}
