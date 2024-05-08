
#ifndef REALTIME_VIEW_H
#define REALTIME_VIEW_H

#include "../abstract/View.h"
#include "RealtimeModel.h"

class RealtimeView : public View
{
private:
    // Settings
    static enum ZoomFactor _zoomFactor;

    static enum SmoothFactor
    {
        SMOOTH_1X,
        SMOOTH_2X,
        SMOOTH_4X
    } _smoothFactor;

    // GDI Objects
    static HDC     _hdcTarget;
    static HDC     _hdcBuf;
    static HBITMAP _hbmpBuf;

    static HFONT   _hEnglishFont;
    static HFONT   _hShellDlgFont;
    static HFONT   _hProcessFont;

    // Window Handle
    static HWND _hWnd;

    // Model object
    static RealtimeModel *_model;

private:
    static void DrawGraph();
    static void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

public:
    RealtimeView(RealtimeModel *model);
    ~RealtimeView();

public:
    static void SetProcess(int puid);
    static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif
