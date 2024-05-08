
#include "stdafx.h"
#include "SingleLock.h"

SingleLock::SingleLock()
{
    InitializeCriticalSection(&_cs);
}

SingleLock::~SingleLock()
{
    DeleteCriticalSection(&_cs);
}

void SingleLock::Lock()
{
    EnterCriticalSection(&_cs);
}

void SingleLock::Unlock()
{
    LeaveCriticalSection(&_cs);
}
