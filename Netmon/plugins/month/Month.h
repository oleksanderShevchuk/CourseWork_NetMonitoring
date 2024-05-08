
#ifndef MONTH_PLUGIN_H
#define MONTH_PLUGIN_H

#include "../Plugin.h"
#include "MonthModel.h"
#include "MonthView.h"

class MonthPlugin : public Plugin
{
private:
    MonthModel *model;
    MonthView *view;

public:
    MonthPlugin();
    virtual ~MonthPlugin();

    virtual void InsertPacket(PacketInfoEx *pi);
    virtual void SetProcess(int puid);
    virtual void SaveDatabase();
    virtual void ClearDatabase();

    virtual const TCHAR * GetName();
    virtual const TCHAR * GetTemplateName();
    virtual DLGPROC GetDialogProc();
};

#endif
