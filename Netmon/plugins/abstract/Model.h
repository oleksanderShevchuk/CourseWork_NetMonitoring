
#ifndef MODEL_H
#define MODEL_H

#include "../../utils/SingleLock.h"
#include "../../utils/Packet.h"

// Base class of all concrete models
class Model : public SingleLock
{
protected:
    static const int PROCESS_ALL;
};

#endif
