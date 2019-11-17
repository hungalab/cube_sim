#include "busarbiter.h"
#include <cstddef>

BusArbiter::BusArbiter()
{
    last_released_cycle = 0;
    bus_holder = nullptr;
}

bool BusArbiter::acquire_bus(DeviceExc *client)
{
    if (last_released_cycle == machine->num_cycles) {
        return false;
    } else if (bus_holder == nullptr) {
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
    if (bus_holder == client) {
        bus_holder = nullptr;
        last_released_cycle = machine->num_cycles;
    }
}