
#ifndef PCAP_SOURCE_H
#define PCAP_SOURCE_H

#include "TrafficSource.h"
#include "pcap/pcap.h"

class PcapSource : public TrafficSource
{
private:
    // Pcap Function Pointer Definition
    typedef pcap_t *(* pcap_open_live_proc)(const char *, int, int, int, char *);
    typedef void    (* pcap_close_proc)(pcap_t *);
    typedef int     (* pcap_next_ex_proc)(pcap_t *, struct pcap_pkthdr **, const u_char **);
    typedef int     (* pcap_findalldevs_proc)(pcap_if_t **, char *);
    typedef void    (* pcap_freealldevs_proc)(pcap_if_t *);

private: // Pcap DLL Module Handle and Function Pointers
    HMODULE              _hWinPcap;
    pcap_open_live_proc   pcap_open_live;
    pcap_close_proc       pcap_close;
    pcap_findalldevs_proc pcap_findalldevs;
    pcap_freealldevs_proc pcap_freealldevs;
    pcap_next_ex_proc     pcap_next_ex;

private:
    pcap_t       *_fp;
    pcap_if_t    *_devices;
    int           _numDevices;
    unsigned char _macAddr[6];

public:
    virtual bool Initialize();
    virtual ~PcapSource();
    virtual int EnumDevices();
    virtual void GetDeviceName(int index, TCHAR *buf, int cchLen);
    virtual bool SelectDevice(int index);
    virtual bool Capture(PacketInfo *pi, bool *timeout);

    bool Reconnect(int index);
};

#endif