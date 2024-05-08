
#ifndef PLUGIN_H
#define PLUGIN_H

#include "../utils/Packet.h"
#include "../utils/Language.h"

class Plugin
{
public:
    virtual ~Plugin() {};
    virtual void InsertPacket(PacketInfoEx *pi) = 0;
    virtual void SetProcess(int puid) = 0;
    virtual void SaveDatabase() = 0;
    virtual void ClearDatabase() = 0;

    virtual const TCHAR * GetName() = 0;
    virtual const TCHAR * GetTemplateName() = 0;
    virtual DLGPROC GetDialogProc() = 0;
};

#endif
