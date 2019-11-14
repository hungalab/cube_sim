#include "busarbiter.h"
#include <cstddef>
#include <cassert>

BusArbiter::BusArbiter()
{
    bus_holder = nullptr;
}

bool BusArbiter::acquire_bus(DeviceExc *client)
{
    if (bus_holder == nullptr) {
        bus_holder = client;
        return true;
    } else if (client == bus_holder) {
        return true;
    } else {
        return false;
    }
}

void BusArbiter::release_bus(DeviceExc *client)
{
    // assert((bus_holder == client) 
    //     && "Invalid DeviceExc client passed to BusArbiter::release_bus()");
    if (bus_holder == client) {
        bus_holder = nullptr;
    }
}