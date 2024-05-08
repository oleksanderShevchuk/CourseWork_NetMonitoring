
#ifndef REALTIME_MODEL_H
#define REALTIME_MODEL_H

#include "../abstract/Model.h"

enum ZoomFactor
{
    ZOOM_1S,
    ZOOM_10S,
    ZOOM_60S
};

class RealtimeModel : public Model
{
private:
    // Item Definition
    typedef struct tagRtModelItem
    {
        std::vector<int> rate_tx_1s;
        std::vector<int> rate_tx_10s;
        std::vector<int> rate_tx_60s;

        std::vector<int> rate_rx_1s;
        std::vector<int> rate_rx_10s;
        std::vector<int> rate_rx_60s;

        int removed_1s;
        int removed_10s;
        int removed_60s;

        struct tagRtModelItem()
        {
            rate_tx_1s.reserve(3600 * 10);
            rate_tx_10s.reserve(360 * 10);
            rate_tx_60s.reserve(60 * 10);

            rate_rx_1s.reserve(3600 * 10);
            rate_rx_10s.reserve(360 * 10);
            rate_rx_60s.reserve(60 * 10);

            removed_1s = 0;
            removed_10s = 0;
            removed_60s = 0;
        }
    } RtModelItem;

    // Items
    std::map<int, RtModelItem> _items;

    // Others
    int _startTime;

private:
    void Fill();

public:
    RealtimeModel();

    // Modify the Model
    void InsertPacket(PacketInfoEx *pi);

    // Export Model Info
    void Export(
        int process, enum ZoomFactor zoom, std::vector<int> &txRate, std::vector<int> &rxRate);
};

#endif
