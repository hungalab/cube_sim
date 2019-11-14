#ifndef _BUSARBITER_H_
#define _BUSARBITER_H_

#include "deviceexc.h"

class BusArbiter {
public:
    BusArbiter();
    bool acquire_bus(DeviceExc *client);
    void release_bus(DeviceExc *client);
private:
    DeviceExc *bus_holder;
};
#endif /* _BUSARBITER_H_ */