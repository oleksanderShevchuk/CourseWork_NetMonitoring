
#ifndef SINGLE_LOCK_H
#define SINGLE_LOCK_H

// Provide platform-specific implementation of locks in a simple interface
// Usage: Simply inherit this class, and then you can call Lock() and Unlock()
//        Code between Lock() and Unlock() cannot be executed simutaneously in different threads
class SingleLock
{
private:
    CRITICAL_SECTION _cs;

protected:
    void Lock();
    void Unlock();

public:
    SingleLock();
    ~SingleLock();
};

#endif
