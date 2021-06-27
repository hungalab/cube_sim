/*  Bus arbiter for connecting multiple bus masters
    Copyright (c) 2021 Amano laboratory, Keio University.
        Author: Takuya Kojima, Takeharu Ikezoe

    This file is part of CubeSim, a cycle accurate simulator for 3-D stacked system.

    CubeSim is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    CubeSim is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CubeSim.  If not, see <https://www.gnu.org/licenses/>.
*/


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