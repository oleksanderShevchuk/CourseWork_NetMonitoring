
#ifndef VIRTUAL_SOURCE_H
#define VIRTUAL_SOURCE_H

#include "TrafficSource.h"

class VirtualSource : public TrafficSource
{
public:
    bool Initialize();
    virtual ~VirtualSource();

    virtual int EnumDevices();
    virtual void GetDeviceName(int index, TCHAR *buf, int cchLen);
    virtual bool SelectDevice(int index);

    virtual bool Capture(PacketInfo *pi, bool *timeout);

    bool Reconnect(int index) { return true; };
};

#endif