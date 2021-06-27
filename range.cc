/* Mapping ranges, the building blocks of the physical memory system, modified for morery access latencys.
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

#include "range.h"
#include "accesstypes.h"
#include "error.h"
#include <cassert>

/* Returns true if ADDR is mapped by this Range object; false otherwise. */
bool
Range::incorporates(uint32 addr)
{
	uint32 base = getBase ();
	return (addr >= base) && (addr < (base + getExtent()));
}

bool
Range::overlaps(Range *r)
{
	assert(r);
	
	uint32 end = getBase() + getExtent();
	uint32 r_end = r->getBase() + r->getExtent();

	/* Key: --- = this, +++ = r, *** = overlap */
	/* [---[****]+++++++] */
	if (getBase() <= r->getBase() && end > r->getBase())
		return true;
	/* [+++++[***]------] */
	else if (r->getBase() <= getBase() && r_end > getBase())
		return true;
	/* [+++++[****]+++++] */
	else if (getBase() >= r->getBase() && end <= r_end)
		return true;
	/* [---[********]---] */
	else if (r->getBase() >= getBase() && r_end <= end)
		return true;

	/* If we got here, we've determined the ranges don't overlap. */
	return false;
}

uint32
Range::fetch_word(uint32 offset, int mode, DeviceExc *client)
{
	read_count++;
	return ((uint32 *)address)[offset / 4];
}

uint16
Range::fetch_halfword(uint32 offset, DeviceExc *client)
{
	read_count++;
	return ((uint16 *)address)[offset / 2];
}

uint8
Range::fetch_byte(uint32 offset, DeviceExc *client)
{
	read_count++;
	return ((uint8 *)address)[offset];
}

void
Range::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	write_count++;
	uint32 *werd;
	/* calculate address */
	werd = ((uint32 *) address) + (offset / 4);
	/* store word */
	*werd = data;
}

void
Range::store_halfword(uint32 offset, uint16 data, DeviceExc *client)
{
	write_count++;
	uint16 *halfwerd;
	/* calculate address */
	halfwerd = ((uint16 *) address) + (offset / 2);
	/* store halfword */
	*halfwerd = data;
}

void
Range::store_byte(uint32 offset, uint8 data, DeviceExc *client)
{
	write_count++;
	uint8 *byte;
	byte = ((uint8 *) address) + offset;
	/* store halfword */
	*byte = data;
}

void Range::report_profile()
{
	fprintf(stderr, "\tRead Count:\t%d\n", read_count);
	fprintf(stderr, "\tWrite Count:\t%d\n", write_count);
}