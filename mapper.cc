/* Physical memory system for the virtual machine.
   Copyright 2001, 2003 Brian R. Gaeke.

This file is part of VMIPS.

VMIPS is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

VMIPS is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with VMIPS; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "cpu.h"
#include "devicemap.h"
#include "error.h"
#include "excnames.h"
#include "mapper.h"
#include "memorymodule.h"
#include "rommodule.h"
#include "options.h"
#include "range.h"
#include "vmips.h"
#include <cassert>

Mapper::Mapper () :
	last_used_mapping (NULL)
{

	opt_bigendian = machine->opt->option("bigendian")->flag;
	byteswapped = (((opt_bigendian) && (!machine->host_bigendian))
			   || ((!opt_bigendian) && machine->host_bigendian));
}

/* Deconstruction. Deallocate the range list. */
Mapper::~Mapper()
{
	for (Ranges::iterator i = ranges.begin(); i != ranges.end(); i++)
		delete *i;
}

/* For now, it always returns true */
bool Mapper::ready(uint32 addr, int32 mode, DeviceExc *client)
{
	return true;
}

void Mapper::requstWord(uint32 addr, int32 mode, DeviceExc *client)
{
	/* to be implemented */
}

void Mapper::step()
{

}

/* Add range R to the mapping. R must not overlap with any existing
 * ranges in the mapping. Return 0 if R added sucessfully or -1 if
 * R overlapped with an existing range.
 */
int
Mapper::add_range(Range *r)
{
	assert (r && "Null range object passed to Mapper::add_range()");

	/* Check to make sure the range is non-overlapping. */
	for (Ranges::iterator i = ranges.begin(); i != ranges.end(); i++) {
		if (r->overlaps(*i)) {
			error("Attempt to map two VMIPS components to the "
			       "same memory area: (base %x extent %x) and "
			       "(base %x extent %x).", r->getBase(),
			       r->getExtent(), (*i)->getBase(),
			       (*i)->getExtent());
			return -1;
		}
	}

	/* Once we're satisfied that it doesn't overlap, add it to the list. */
	ranges.push_back(r);
	return 0;
}

/* Returns the first mapping in the rangelist, starting at the beginning,
 * which maps the address P, or NULL if no such mapping exists. This
 * function uses the LAST_USED_MAPPING instance variable as a cache to
 * speed a succession of accesses to the same area of memory.
 */
Range *
Mapper::find_mapping_range(uint32 p)
{
	if (last_used_mapping && last_used_mapping->incorporates(p))
		return last_used_mapping;

	for (Ranges::iterator i = ranges.begin(), e = ranges.end(); i != e;
			++i) {
		if ((*i)->incorporates(p)) {
			last_used_mapping = *i;
			return *i;
		}
	}

	return NULL;
}

/* If the host processor is byte-swapped with respect to the target
 * we are emulating, we will need to swap data bytes around when we
 * do loads and stores. These functions implement the swapping.
 *
 * The mips_to_host_word(), etc. methods invoke the swap_word() methods
 * if the host processor is the opposite endianness from the target.
 */

/* Convert word W from big-endian to little-endian, or vice-versa,
 * and return the result of the conversion.
 */
uint32
Mapper::swap_word(uint32 w)
{
	return ((w & 0x0ff) << 24) | (((w >> 8) & 0x0ff) << 16) |
		(((w >> 16) & 0x0ff) << 8) | ((w >> 24) & 0x0ff);
}

/* Convert halfword H from big-endian to little-endian, or vice-versa,
 * and return the result of the conversion.
 */
uint16
Mapper::swap_halfword(uint16 h)
{
	return ((h & 0x0ff) << 8) | ((h >> 8) & 0x0ff);
}

/* Convert word W from target processor byte-order to host processor
 * byte-order and return the result of the conversion.
 */
uint32
Mapper::mips_to_host_word(uint32 w)
{
	if (byteswapped)
		w = swap_word (w);
	return w;
}

/* Convert word W from host processor byte-order to target processor
 * byte-order and return the result of the conversion.
 */
uint32
Mapper::host_to_mips_word(uint32 w)
{
	if (byteswapped)
		w = swap_word (w);
	return w;
}

/* Convert halfword H from target processor byte-order to host processor
 * byte-order and return the result of the conversion.
 */
uint16
Mapper::mips_to_host_halfword(uint16 h)
{
	if (byteswapped)
		h = swap_halfword(h);
	return h;
}

/* Convert halfword H from host processor byte-order to target processor
 * byte-order and return the result of the conversion.
 */
uint16
Mapper::host_to_mips_halfword(uint16 h)
{
	if (byteswapped)
		h = swap_halfword(h);
	return h;
}

void
Mapper::bus_error (DeviceExc *client, int32 mode, uint32 addr,
                   int32 width, uint32 data)
{
	last_berr_info.valid = true;
	last_berr_info.client = client;
	last_berr_info.mode = mode;
	last_berr_info.addr = addr;
	last_berr_info.width = width;
	last_berr_info.data = data;
	if (machine->opt->option("dbemsg")->flag) {
		fprintf (stderr, "%s %s %s physical address 0x%x caused bus error",
			(mode == DATASTORE) ? "store" : "load",
			(width == 4) ? "word" : ((width == 2) ? "halfword" : "byte"),
			(mode == DATASTORE) ? "to" : "from",
			addr);
		if (mode == DATASTORE)
			fprintf (stderr, ", data = 0x%x", data);
		fprintf (stderr, "\n");
	}
	client->exception((mode == INSTFETCH ? IBE : DBE), mode);
}


uint32
Mapper::fetch_word(uint32 addr, int32 mode, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset;
	uint32 result, oaddr = addr;

	if (addr % 4 != 0) {
		client->exception(AdEL,mode);
		return 0xffffffff;
	}

	l = find_mapping_range(addr);
	if (!l) {
		bus_error (client, mode, addr, 4);
		return 0xffffffff;
	}
	offset = oaddr - l->getBase();
	if (!l->canRead(offset)) {
		/* Reads from write-only ranges return ones */
		return 0xffffffff;
	}

	return host_to_mips_word(l->fetch_word(offset, mode, client));
}

/* Fetch a halfword from the physical memory from physical address ADDR.
 * CACHEABLE is true if this access should be routed through the cache,
 * false otherwise.
 * 
 * The routine returns either the specified halfword, if it is mapped
 * and the address is correctly aligned, or else a halfword consisting
 * of all ones is returned.
 *
 * Halfwords are returned in the endianness of the target processor;
 * since devices are implemented as Ranges, devices should return halfwords
 * in the host endianness.
 * 
 * This routine may trigger exception DBE in the client processor,
 * if the address is unmapped.
 * This routine may trigger exception AdEL in the client
 * processor, if the address is unaligned.
 */
uint16
Mapper::fetch_halfword(uint32 addr, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset;
	uint32 result, oaddr = addr;

	if (addr % 2 != 0) {
		client->exception(AdEL,DATALOAD);
		return 0xffff;
	}

	l = find_mapping_range(addr);
	if (!l) {
		bus_error (client, DATALOAD, addr, 2);
		return 0xffff;
	}
	offset = oaddr - l->getBase();
	if (!l->canRead(offset)) {
		/* Reads from write-only ranges return ones */
		return 0xffff;
	}

	return host_to_mips_halfword(l->fetch_halfword(offset, client));
}

/* Fetch a byte from the physical memory from physical address ADDR.
 * CACHEABLE is true if this access should be routed through the cache,
 * false otherwise.
 * 
 * The routine returns either the specified byte, if it is mapped,
 * or else a byte consisting of all ones is returned.
 * 
 * This routine may trigger exception DBE in the client processor,
 * if the address is unmapped.
 */
uint8
Mapper::fetch_byte(uint32 addr, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset;
	uint32 result, oaddr = addr;

	l = find_mapping_range(addr);
	if (!l) {
		bus_error (client, DATALOAD, addr, 1);
		return 0xff;
	}
	offset = oaddr - l->getBase();
	if (!l->canRead(offset)) {
		/* Reads from write-only ranges return ones */
		return 0xff;
	}

	return l->fetch_byte(offset, client);
}


/* Store a word's-worth of DATA to physical address ADDR.
 * CACHEABLE is true if this access should be routed through the cache,
 * false otherwise.
 * 
 * This routine may trigger exception AdES in the client processor,
 * if the address is unaligned.
 * This routine may trigger exception DBE in the client processor,
 * if the address is unmapped.
 */
void
Mapper::store_word(uint32 addr, uint32 data, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset;

	if (addr % 4 != 0) {
		client->exception(AdES,DATASTORE);
		return;
	}

	l = find_mapping_range(addr);
	if (!l) {
		bus_error (client, DATASTORE, addr, 4, data);
		return;
	}
	offset = addr - l->getBase();
	if (!l->canWrite(offset)) {
		fprintf(stderr, "Attempt to write read-only memory: 0x%08x\n",
			addr);
		return;
	}

	l->store_word(addr - l->getBase(), mips_to_host_word(data), client);

}

/* Store half a word's-worth of DATA to physical address ADDR.
 * CACHEABLE is true if this access should be routed through the cache,
 * false otherwise.
 * 
 * This routine may trigger exception AdES in the client processor,
 * if the address is unaligned.
 * This routine may trigger exception DBE in the client processor,
 * if the address is unmapped.
 */
void
Mapper::store_halfword(uint32 addr, uint16 data, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset;

	if (addr % 2 != 0) {
		client->exception(AdES,DATASTORE);
		return;
	}

	l = find_mapping_range(addr);
	if (!l) {
		bus_error (client, DATASTORE, addr, 2, data);
		return;
	}
	offset = addr - l->getBase();
	if (!l->canWrite(offset)) {
		fprintf(stderr, "Attempt to write read-only memory: 0x%08x\n",
			addr);
		return;
	}

	l->store_halfword(addr - l->getBase(), mips_to_host_halfword(data), client);

}

/* Store a byte of DATA to physical address ADDR.
 * CACHEABLE is true if this access should be routed through the cache,
 * false otherwise.
 * 
 * This routine may trigger exception DBE in the client processor,
 * if the address is unmapped.
 */
void
Mapper::store_byte(uint32 addr, uint8 data, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset;

	l = find_mapping_range(addr);
	if (!l) {
		bus_error (client, DATASTORE, addr, 1, data);
		return;
	}
	offset = addr - l->getBase();
	if (!l->canWrite(offset)) {
		fprintf(stderr, "Attempt to write read-only memory: 0x%08x\n",
			addr);
		return;
	}

	l->store_byte(addr - l->getBase(), data, client);

}

/* Print a hex dump of the first 8 words on top of the stack to the
 * filehandle pointed to by F. The physical address that corresponds to the
 * stack pointer is STACKPHYS. The stack is assumed to grow down in memory;
 * that is, the addresses which are dumped are STACKPHYS, STACKPHYS - 4,
 * STACKPHYS - 8, ...
 */
void
Mapper::dump_stack(FILE *f, uint32 stackphys)
{
	Range *l;

	fprintf(f, "Stack: ");
	if ((l = find_mapping_range(stackphys)) == NULL) {
		fprintf(f, "(points to hole in address space)");
	} else {
		if (!dynamic_cast<MemoryModule *> (l)) {
			fprintf(f, "(points to non-RAM address space)");
		} else {
			for (int i = 0; i > -8; i--) {
				uint32 data =
					((uint32 *) l->
					 getAddress())[(stackphys - l->getBase()) / 4 + i];
				if (byteswapped)
					data = swap_word (data);
				fprintf(f, "%08x ", data);
			}
		}
	}
	fprintf(f, "\n");
}

/* Print a hex dump of the first word of memory at physical address
 * ADDR to the filehandle pointed to by F.
 */
void
Mapper::dump_mem(FILE *f, uint32 phys)
{
	Range *l;

	if ((l = find_mapping_range(phys)) == NULL) {
		fprintf(f, "(points to hole in address space)");
	} else {
		if (!(dynamic_cast<MemoryModule *> (l) || dynamic_cast<ROMModule *>(l))) {
			fprintf(f, "(points to non-memory address space)");
		} else {
			uint32 data =
				((uint32 *) l->
				 getAddress())[(phys - l->getBase()) / 4];
			if (byteswapped)
				data = swap_word (data);
			fprintf(f, "%08x ", data);
		}
	}
}
