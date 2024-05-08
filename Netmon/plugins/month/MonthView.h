
#ifndef MONTH_VIEW_H
#define MONTH_VIEW_H

#include "../abstract/View.h"
#include "MonthModel.h"

class MonthView : public View
{
protected:
    // Settings
    static ShortDate _curMonth;

    // GDI Objects
    static HDC     _hdcTarget;
    static HDC     _hdcBuf;
    static HBITMAP _hbmpBuf;
    static HFONT   _hFontDays;
    static HFONT   _hFontDesc;
    static HPEN    _hPenVertical;

    static HDC     _hdcPage;
    static HBITMAP _hbmpPageUpLight;
    static HBITMAP _hbmpPageUpDark;
    static HBITMAP _hbmpPageDownLight;
    static HBITMAP _hbmpPageDownDark;

    // Window Handle
    static HWND _hWnd;

    // Model Object
    static MonthModel *_model;

private:
    static void DrawGraph();
    static void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

public:
    MonthView(MonthModel *model);
    ~MonthView();

public:
    static void SetProcess(int puid);
    static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif
