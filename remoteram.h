/*  Headers for the stacked memory class
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

#ifndef _REMOTERAM_H_
#define _REMOTERAM_H_

#include <stdio.h>
#include "accelerator.h"
#include "memorymodule.h"

class CubeAccelerator;
class MemoryModule;

class RemoteRam : public CubeAccelerator {
private:
	MemoryModule *mem;

public:
	RemoteRam(uint32 node_ID, Router* upperRouter, int mem_size);
	~RemoteRam();

	void setup();
	void core_step() {};
	void core_reset() {};

	const char *accelerator_name() { return "RemoteRam"; }

	virtual void send_commnad(uint32 cmd, uint32 arg) {};
	virtual bool isTriggered() { return false; };
	virtual uint32 get_dbg_data() { return 0; };

};


#endif //_REMOTERAM_H_
