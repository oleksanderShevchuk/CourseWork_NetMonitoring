
#ifndef MONTH_MODEL_H
#define MONTH_MODEL_H

#include "../abstract/Model.h"
#include "../../utils/SQLite.h"
#include "../../utils/Date.h"

class MonthModel : public Model
{
public:
    // Item Definition
    // 1. The Item of a Process for a Month, 512 Bytes -------
    typedef struct tagMonthItem
    {
        __int64 dayTx[31]; // In Bytes
        __int64 dayRx[31];

        __int64 sumTx;
        __int64 sumRx;

        struct tagMonthItem()
        {
            RtlZeroMemory(dayTx, sizeof(dayTx));
            RtlZeroMemory(dayRx, sizeof(dayRx));

            sumTx = 0;
            sumRx = 0;
        }
    } MonthItem;

    // 2. The Item of a Process ------------------------------
    typedef std::map<ShortDate, MonthItem> MtModelItem;

private:
    std::map<int, MtModelItem> _items;
    bool _clear_flag;

private:
    void InitDatabase();
    void ReadDatabase();
    static void ReadDatabaseCallback(SQLiteRow *row, void *context);

public:
    MonthModel();
    ~MonthModel();

    // Modify the Model
    void InsertPacket(PacketInfoEx *pi);

    // Export Model Info
    void Export(int process, const ShortDate &date, MonthItem &item);

    // Save Database (in case of crash)
    void SaveDatabase();

    // Clear Database
    void ClearDatabase();

    ShortDate GetFirstMonth(int puid);
    ShortDate GetLastMonth(int puid);
    ShortDate GetClosestMonth(int puid, const ShortDate &target);
};

#endif
