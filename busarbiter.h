/*  Headers for the bus arbiter class
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


#ifndef _BUSARBITER_H_
#define _BUSARBITER_H_

#include "vmips.h"
#include "deviceexc.h"

class BusArbiter {
public:
    BusArbiter();
    bool acquire_bus(DeviceExc *client);
    void release_bus(DeviceExc *client);
private:
    int32 last_released_cycle;
    DeviceExc *bus_holder;
};
#endif /* _BUSARBITER_H_ */