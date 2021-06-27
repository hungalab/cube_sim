/*  A simple memory module accessed through stacked chip network
    Copyright (c) 2021 Amano laboratory, Keio University.
        Author: Takuya Kojima

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

#include "remoteram.h"

RemoteRam::RemoteRam(uint32 node_ID, Router* upperRouter, int mem_size)
	: CubeAccelerator(node_ID, upperRouter)
{
	mem = new MemoryModule(mem_size, 0);
}

RemoteRam::~RemoteRam()
{
	delete mem;
}

void RemoteRam::setup()
{
	localBus->map_at_local_address(mem, 0x0);
}