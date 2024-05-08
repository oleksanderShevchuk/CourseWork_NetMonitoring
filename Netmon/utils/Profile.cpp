
#include "stdafx.h"
#include "Profile.h"
#include "Utils.h"

// Init
void Profile::Init(const TCHAR *filename, const TCHAR *sectionName)
{
    // Get full path
    TCHAR fullPath[MAX_PATH];
    Utils::GetFilePathInCurrentDir(fullPath, MAX_PATH, filename);

    // Save
    _tcscpy_s(_filename, MAX_PATH, fullPath);
    _tcscpy_s(_sectionName, MAX_PATH, sectionName);
}

// Register default value
void Profile::RegisterDefault(const TCHAR *option, ProfileValueItem *item)
{
    _defaults[option] = item;
    _keyList.push_back(option);
}

// Load
void Profile::Load()
{
    // For each registered default value
    for (unsigned int i = 0; i < _keyList.size(); i++)
    {
        const TCHAR *key = _keyList[i].data();
        ProfileValueItem *defaultValue = _defaults[key];

        TCHAR value[4096];
        if (GetPrivateProfileString(_sectionName, key, NULL, value, 4096, _filename)) // item exist
        {
            _values[key] = defaultValue->Parse(value); // save to map
        }
        else // item does not exist
        {
            // write default value to file
            WritePrivateProfileString(_sectionName, key, defaultValue->ToString().data(), _filename);

            // save to map
            _values[key] = defaultValue->Parse(defaultValue->ToString());
        }
    }
}

// Get values
ProfileBoolItem *Profile::GetBool(const TCHAR *option)
{
    if (_values.count(option) == 0)
        return NULL;
    return dynamic_cast<ProfileBoolItem *>(_values[option]);
}

ProfileIntItem *Profile::GetInt(const TCHAR *option)
{
    if (_values.count(option) == 0)
        return NULL;
    return dynamic_cast<ProfileIntItem *>(_values[option]);
}

ProfileStringItem *Profile::GetString(const TCHAR *option)
{
    if (_values.count(option) == 0)
        return NULL;
    return dynamic_cast<ProfileStringItem *>(_values[option]);
}

ProfileIntListItem *Profile::GetIntList(const TCHAR *option)
{
    if (_values.count(option) == 0)
        return NULL;
    return dynamic_cast<ProfileIntListItem *>(_values[option]);
}

// Set value
void Profile::SetValue(const TCHAR *option, ProfileValueItem *item)
{
    if (_values.count(option) == 0)
    {
        TCHAR msg[256];
        _stprintf_s(msg, 256, TEXT("Invalid profile key \"%s\""), option);
        MessageBox(NULL, msg, TEXT("Warning"), MB_OK | MB_ICONWARNING);
    }
    else
    {
        // Type check (skipped)
        // if (typeid(*_values[option]) != typeid(*item)) ...

        // Delete previous object
        delete _values[option];

        // Update map
        _values[option] = item;

        // Update file
        WritePrivateProfileString(_sectionName, option, item->ToString().data(), _filename);
    }
}
