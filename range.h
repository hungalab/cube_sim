/* Definitions to support mapping ranges.
    Original work Copyright 2001, 2003 Brian R. Gaeke.
    Modified work Copyright (c) 2021 Amano laboratory, Keio University.
        Modifier: Takuya Kojima

    This file is part of CubeSim, a cycle accurate simulator for 3-D stacked system.
    It is derived from a source code of VMIPS project under GPLv2.

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

#ifndef _RANGE_H_
#define _RANGE_H_

#include "accesstypes.h"
#include "types.h"
#include <sys/types.h>
#include <stdio.h>

class DeviceExc;

/* Base class for managing a range of mapped memory. Memory-mapped
 * devices (class DeviceMap) derive from this.
 */
class Range { 
protected:
	uint32 base;        // first physical address represented
	uint32 extent;      // number of bytes of memory provided
	void *address;      // host machine pointer to start of memory
	int perms;          // MEM_READ, MEM_WRITE, ... in accesstypes.h
	// for profile
	int read_count;
	int write_count;

public:
	Range(uint32 _base, uint32 _extent, caddr_t _address, int _perms) :
		base(_base), extent(_extent), address(_address), perms(_perms),
		read_count(0), write_count(0) { }
	virtual ~Range() { }
	
	bool incorporates(uint32 addr);
	bool overlaps(Range *r);
	uint32 getBase () const { return base; }
	uint32 getExtent () const { return extent; }
	void *getAddress () const { return address; }
	int getPerms () const { return perms; }
	void setBase (uint32 newBase) { base = newBase; }
	void setPerms (int newPerms) { perms = newPerms; }

	virtual bool canRead (uint32 offset) { return perms & MEM_READ; }
	virtual bool canWrite (uint32 offset) { return perms & MEM_WRITE; }

	virtual uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
	virtual uint16 fetch_halfword(uint32 offset, DeviceExc *client);
	virtual uint8 fetch_byte(uint32 offset, DeviceExc *client);
	virtual void store_word(uint32 offset, uint32 data, DeviceExc *client);
	virtual void store_halfword(uint32 offset, uint16 data,
		DeviceExc *client);
	virtual void store_byte(uint32 offset, uint8 data, DeviceExc *client);
	virtual bool ready(uint32 offset, int32 mode, DeviceExc *client) { return true; } ;
	virtual int extra_latency() { return 0; };

	void report_profile();
};


#endif /* _RANGE_H_ */
