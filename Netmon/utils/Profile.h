
#ifndef PROFILE_H
#define PROFILE_H

#include "ProfileItem.h"

// The Profile class is used to operate an ini file with only one section.
class Profile
{
private:
    TCHAR _filename[MAX_PATH];
    TCHAR _sectionName[MAX_PATH];

    std::map<std::tstring, ProfileValueItem *> _defaults;
    std::map<std::tstring, ProfileValueItem *> _values;
    std::vector<std::tstring> _keyList;

public:
    // Input some information of the profile
    void Init(const TCHAR *fileName, const TCHAR *sectionName);

    // Register default value
    void RegisterDefault(const TCHAR *option, ProfileValueItem *item);

    // Load
    void Load();

    // Get values
    ProfileBoolItem    *GetBool(const TCHAR *option);
    ProfileIntItem     *GetInt(const TCHAR *option);
    ProfileStringItem  *GetString(const TCHAR *option);
    ProfileIntListItem *GetIntList(const TCHAR *option);

    // Set value
    void SetValue(const TCHAR *option, ProfileValueItem *item);
};

#endif
