
#ifndef REALTIME_PLUGIN_H
#define REALTIME_PLUGIN_H

#include "../Plugin.h"
#include "RealtimeModel.h"
#include "RealtimeView.h"

class RealtimePlugin : public Plugin
{
private:
    RealtimeModel *model;
    RealtimeView *view;

public:
    RealtimePlugin();
    virtual ~RealtimePlugin();

    virtual void InsertPacket(PacketInfoEx *pi);
    virtual void SetProcess(int puid);
    virtual void SaveDatabase();
    virtual void ClearDatabase();

    virtual const TCHAR * GetName();
    virtual const TCHAR * GetTemplateName();
    virtual DLGPROC GetDialogProc();
};

#endif
