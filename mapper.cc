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
#include <cmath>

#define CACHE_DEBUG

Cache::Cache(unsigned int block_count_, unsigned int block_size_, unsigned int way_size_) :
	block_count(block_count_),
	block_size(block_size_),
	way_size(way_size_)
{
	//block_size: byte size
	blocks = new Entry*[way_size];
	for (int i = 0; i < way_size; i++) {
		blocks[i] = new Entry[block_count];
		for (int j = 0; j < block_count; j++) {
			blocks[i][j].data = new uint32[block_size / 4];
			blocks[i][j].dirty = false;
			blocks[i][j].valid = false;
			blocks[i][j].last_access = machine->num_instrs;
		}
	}

	offset_len = int(std::log2(block_size));
	index_len = int(std::log2(block_count));
}

Cache::~Cache()
{
	delete [] blocks;
}

void Cache::addr_separete(uint32 addr, uint32 &tag, uint32 &index, uint32 &offset)
{
	tag = addr >> (offset_len + index_len);
	index = (addr & ((1 << (index_len + offset_len)) - 1)) >> offset_len;
	offset = (addr & ((1 << offset_len) - 1));
}

uint32 Cache::calc_addr(uint32 way, uint32 index)
{
	return (blocks[way][index].tag << (offset_len + index_len)) + (index << offset_len);
}

bool Cache::cache_hit(uint32 addr, uint32 &index, uint32 &way, uint32 &offset)
{
	uint32 tag;
	addr_separete(addr, tag, index, offset);
	for (way = 0; way < way_size; way++) {
		if (blocks[way][index].tag == tag && blocks[way][index].valid) {
#if defined(CACHE_DEBUG)
			printf("Cache Hit addr(0x%x)\n", addr);
#endif
			return true;
		}
	}
#if defined(CACHE_DEBUG)
	printf("Cache Miss addr(0x%x)\n", addr);
#endif
	return false;
}

void Cache::cache_fetch(uint32 addr, Mapper* physmem, int mode, DeviceExc *client, uint32 &index, uint32 &way, uint32 &offset)
{
	int least_recent_used_way = 0;
	uint32 tag;

	addr_separete(addr, tag, index, offset);
	bool find = false;
	Range *l = NULL;

	//find free block and LRU block
	for (way = 0; way < way_size; way++) {
		if (blocks[least_recent_used_way][index].last_access >
				blocks[way][index].last_access) {
			//update
			least_recent_used_way = way;
		}
		if (!blocks[way][index].valid) {
			find = true;
			break;
		}
	}

	if (!find) {
		way = least_recent_used_way;
		//check if WB is needed
		if (!physmem->isIsolated() && mode != INSTFETCH && blocks[least_recent_used_way][index].dirty) {
			//cache WB
#if defined(CACHE_DEBUG)
			printf("WB cache block way(%d) index(%d) is used for addr(0x%x)\n", way, index, addr);
#endif
			uint32 wb_addr = calc_addr(way, index);
			uint32 wb_offset = 0;
			for (int i = 0; i < block_size / 4; i++, wb_addr += 4) {
				l = physmem->find_mapping_range(wb_addr);
				if (!l) {
					physmem->bus_error(client, mode, wb_addr, 4);
					return;
				}
				wb_offset = wb_addr - l->getBase();
				l->store_word(wb_offset, physmem->host_to_mips_word(blocks[way][index].data[i]), client);
			}
		}
	}

#if defined(CACHE_DEBUG)
	printf("cache block way(%d) index(%d) is used for addr(0x%x)\n", way, index, addr);
#endif

	blocks[way][index].tag = tag;
	blocks[way][index].valid = true;
	blocks[way][index].dirty = false;

	if (physmem->isIsolated() && mode != INSTFETCH) {
		// no need to fetch
		return;
	}

	//set line
	l = NULL;
	uint32 fetch_offset;
	for (int i = 0; i < block_size / 4; i++) {
		l = physmem->find_mapping_range(addr + 4 * i);
		if (!l) {
			physmem->bus_error(client, mode, addr + 4 * i, 4);
			blocks[way][index].valid = false;
			return;
		}
		fetch_offset = addr + 4 * i - l->getBase();
		blocks[way][index].data[i] = physmem->host_to_mips_word(l->fetch_word(fetch_offset, mode, client));
	}


}

Mapper::Mapper () :
	last_used_mapping (NULL),
	caches_isolated (false),
	caches_swapped (false)
{
     /* Caches are 2way-set associative, physically indexed, physically tagged,
	 * with 1-word lines. */
	icache = new Cache(64, 64, 2);	/* 64Byte * 64Block * 2way = 8KB*/
	dcache = new Cache(64, 64, 2);
	opt_bigendian = machine->opt->option("bigendian")->flag;
	byteswapped = (((opt_bigendian) && (!machine->host_bigendian))
			   || ((!opt_bigendian) && machine->host_bigendian));
}

/* Deconstruction. Deallocate the range list. */
Mapper::~Mapper()
{
	delete icache;
	delete dcache;
	for (Ranges::iterator i = ranges.begin(); i != ranges.end(); i++)
		delete *i;
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

/* Set the cache control bits to the given values.
 */
void Mapper::cache_set_control_bits(bool isolated, bool swapped)
{
#if defined(CACHE_DEBUG)
	if (caches_isolated != isolated) {
	    printf("Isolated -> %d\n", isolated);
	}
	if (caches_swapped != swapped) {
	    printf("Swapped -> %d\n", swapped);
	}
#endif
	caches_isolated = isolated;
	caches_swapped = swapped;
}

/* Test a specific cache entry for a hit; return whether we hit.
 */
// bool Mapper::cache_use_entry(const Cache::Entry *const entry,
// 	uint32 tag, int32 mode) const
// {
// 	return (caches_isolated && (mode != INSTFETCH)) ||
// 	    (entry->valid && entry->tag == tag);
// }

/* Test cache for a hit; return whether we hit.
 */
// bool Mapper::cache_read(bool cacheable, int32 mode, uint32 &offset, uint32 &addr,
// 	Cache::Entry *&entry)
// {
// 	if (cacheable) {
// 		Cache *cache;
// 		if (caches_swapped) {
// 			cache = (mode == INSTFETCH) ? dcache : icache;
// 		} else {
// 			cache = (mode == INSTFETCH) ? icache : dcache;
// 		}

// 		// hit check
// 		bool hit = false;
// 		uint32 tag, index;
// 		int way;
// 		cache->addr_separete(addr, tag, index, offset);
// 		for (way = 0; way < cache->way_size; way++) {
// 			if (cache->blocks[way][index].tag == tag && cache->blocks[way][index].valid) {
// 				hit = true;
// 				break;
// 			}
// 		}

// 		if (hit) {
// 			entry = &cache->blocks[way][index];
// 		} else {
// 			//cache miss
// 			entry = NULL;
// 		}

// 		if (cache_use_entry(entry, tag, mode)) {
// #if defined(CACHE_DEBUG)
// 		    	if (caches_isolated) {
// 				printf("Read w/isolated cache 0x%x\n", addr);
// 			}
// #endif
// 		        return true;
// 		}

// 	}
// 	return false;
// }

/* Read data from a specific cache entry.
 */
uint32 Mapper::cache_get_data_from_entry(const Cache::Entry *const entry,
		int size, uint32 offset)
{
	uint32 result;
	uint32 n;
	switch (size) {
		case 4: result = entry->data[offset>>2]; break;
		case 2: n = (offset >> 1) & 0x1;
			if (byteswapped)
			    n = 1 - n;
			result = ((uint16 *)(&entry->data[offset>>2]))[n];
			break;
		case 1: n = (offset & 0x3);
			if (byteswapped)
			    n = 3 - n;
			result = ((uint8 *)(&entry->data[offset>>2]))[n];
			break;
		default: assert(0); result = 0xffffffff; break;
	}
	return result;
}

/* Write data to a specific cache entry.
 */
void Mapper::cache_set_data_into_entry(Cache::Entry *const entry,
		int size, uint32 offset, uint32 data)
{
	uint32 n;
	switch (size) {
		case 4: entry->data[offset>>2] = data; break;
		case 2: n = (offset >> 1) & 0x1;
			if (byteswapped)
			    n = 1 - n;
			((uint16 *)(&entry->data[offset>>2]))[n] = (uint16)data;
			break;
		case 1: n = offset & 0x3;
			if (byteswapped)
			    n = 3 - n;
			((uint8 *)(&entry->data[offset>>2]))[n] = (uint8)data;
			break;
		default: assert(0); break;
	}
	entry->dirty = true;
}

/* Write data to cache, and then to main memory.
 * 0. If isolated, no write through
 *    - If partial word, then invalidate, otherwise valid
 * 1. If full word, write through and set tag+valid bit regardless of
 *    current contents
 * 2. If partial word, 
 *    - fill cache entry with new address
 *    - do partial-word update
 *    - write through
 */
// void Mapper::cache_write(int size, uint32 addr, uint32 data, Range *l,
// 	DeviceExc *client)
// {
// 	Cache *cache;
// 	if (caches_swapped) {
// 		cache = icache;
// 	} else {
// 		cache = dcache;
// 	}
// 	uint32 tag = addr>>2;
// 	Cache::Entry *entry = &cache->entries[tag & cache->mask];

// 	if (caches_isolated) {
// #if defined(CACHE_DEBUG)
// 	        printf("Write(%d) w/isolated cache 0x%x -> 0x%x\n", size, data, addr);
// #endif

// 		if (size == 4) {
// 		        /* Caches isolated; write to cache only. */
// 			cache_set_data_into_entry(entry,size,addr,data);
// 		} else {
// 			/* Partial-word store to isolated cache causes
// 			   invalidation. */
// 			entry->valid = 0;
// 		}
// 		return;	/* Don't write to memory. */
// 	}
// 	if (size != 4 && !cache_use_entry(entry, tag, DATASTORE)) {
// 	        /* Partial-word store to cache entry that is not already valid.
// 		   This triggers read-modify-write behavior. */
// 	        uint32 word_addr = addr & ~0x3;	/* Refill whole word. */
// 	        uint32 word_offset = word_addr - l->getBase();

// 	    	/* Fill cache entry with word containing addressed byte or
// 		   halfword. */
// 		cache_do_fill(entry, tag, l, word_offset, DATASTORE, client, 4,
// 			word_addr);
// 	}
// 	/* Update data in cache. */
// 	cache_set_data_into_entry(entry,size,addr,data);
// 	entry->valid = true;
// 	entry->tag = tag;
// 	/* Write word from cache to memory. */
// 	l->store_word(addr - l->getBase(), mips_to_host_word(entry->data), client);
// }

/* Refill a cache entry.
 */
// uint32 Mapper::cache_do_fill(Cache::Entry *const entry, uint32 addr, int32 mode, DeviceExc *client,
// 		int32 size)
// {
// 	if (!caches_isolated || mode==INSTFETCH) {
// 		for (int i = 0; i < size; i++) {
// 			entry->data[i] = host_to_mips_word(l->fetch_word(offset, mode,
// 				    client));
// 		}
// 	}
// 	return cache_get_data_from_entry(entry,size,addr);
// }

/* Fetch a word from the physical memory from physical address
 * ADDR. MODE is INSTFETCH if this is an instruction fetch; DATALOAD
 * otherwise. CACHEABLE is true if this access should be routed through
 * the cache, false otherwise.  This routine is shared between instruction
 * fetches and word-wide data fetches.
 * 
 * The routine returns either the specified word, if it is mapped and
 * the address is correctly aligned, or else a word consisting of all
 * ones is returned.
 *
 * Words are returned in the endianness of the target processor; since devices
 * are implemented as Ranges, devices should return words in the host
 * endianness.
 * 
 * This routine may trigger exceptions IBE and/or DBE in the client
 * processor, if the address is unmapped.
 * This routine may trigger exception AdEL in the client
 * processor, if the address is unaligned.
 */
uint32
Mapper::fetch_word(uint32 addr, int32 mode, bool cacheable, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset;
	uint32 way, index;
	uint32 result, oaddr = addr;
	Cache::Entry *entry = NULL;

	if (addr % 4 != 0) {
		client->exception(AdEL,mode);
		return 0xffffffff;
	}

	if (cacheable) {
		Cache *cache;
		if (caches_swapped) {
			cache = (mode == INSTFETCH) ? dcache : icache;
		} else {
			cache = (mode == INSTFETCH) ? icache : dcache;
		}
		if (!cache->cache_hit(addr, index, way, offset)) {
			//cache miss
			cache->cache_fetch(addr, this, mode, client, index, way, offset);
		}
		entry = &cache->blocks[way][index];
		if (!entry->valid) {
			return 0xffffffff;
		}
		uint32 x;
		x = cache_get_data_from_entry(entry, 4, offset);
		if (caches_isolated) {
			printf("Isolated word read returned 0x%x\n", x);
		}
		return x;
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
Mapper::fetch_halfword(uint32 addr, bool cacheable, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset, index, way;
	uint32 result, oaddr = addr;
	Cache::Entry *entry = NULL;

	if (addr % 2 != 0) {
		client->exception(AdEL,DATALOAD);
		return 0xffff;
	}

	if (cacheable) {
		Cache *cache;
		if (caches_swapped) {
			cache = icache;
		} else {
			cache = dcache;
		}
		if (!cache->cache_hit(addr, index, way, offset)) {
			//cache miss
			cache->cache_fetch(addr, this, DATALOAD, client, index, way, offset);
		}
		entry = &cache->blocks[way][index];
		if (!entry->valid) {
			return 0xffff;
		}
		uint32 x;
		x = cache_get_data_from_entry(entry, 2, offset);
		if (caches_isolated) {
			printf("Isolated word read returned 0x%x\n", x);
		}
		return x;
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
Mapper::fetch_byte(uint32 addr, bool cacheable, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset, index, way;
	uint32 result, oaddr = addr;
	Cache::Entry *entry = NULL;

	if (cacheable) {
		Cache *cache;
		if (caches_swapped) {
			cache = icache;
		} else {
			cache = dcache;
		}
		if (!cache->cache_hit(addr, index, way, offset)) {
			//cache miss
			cache->cache_fetch(addr, this, DATALOAD, client, index, way, offset);
		}
		entry = &cache->blocks[way][index];
		if (!entry->valid) {
			return 0xff;
		}
		uint32 x;
		x = cache_get_data_from_entry(entry, 4, offset);
		if (caches_isolated) {
			printf("Isolated word read returned 0x%x\n", x);
		}
		return x;
	}

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
Mapper::store_word(uint32 addr, uint32 data, bool cacheable, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset, way, index;
	Cache::Entry *entry = NULL;

	printf("load byte from 0x%x\n", addr);
	if (addr % 4 != 0) {
		client->exception(AdES,DATASTORE);
		return;
	}

	if (cacheable) {
		Cache *cache;
		if (caches_swapped) {
			cache = icache;
		} else {
			cache = dcache;
		}
		if (!cache->cache_hit(addr, index, way, offset)) {
			//cache miss
			cache->cache_fetch(addr, this, DATASTORE, client, index, way, offset);
		}
		entry = &cache->blocks[way][index];
		if (!entry->valid) {
			return;
		}
		if (caches_isolated) {
			printf("Write(%d) w/isolated cache 0x%x -> 0x%x\n", 4, data, addr);
		}
		cache_set_data_into_entry(entry, 4, offset, data);
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
Mapper::store_halfword(uint32 addr, uint16 data, bool cacheable, DeviceExc
	*client)
{
	Range *l = NULL;
	uint32 offset, way, index;
	Cache::Entry *entry = NULL;

	if (addr % 2 != 0) {
		client->exception(AdES,DATASTORE);
		return;
	}

	if (cacheable) {
		Cache *cache;
		if (caches_swapped) {
			cache = icache;
		} else {
			cache = dcache;
		}
		if (!cache->cache_hit(addr, index, way, offset)) {
			//cache miss
			cache->cache_fetch(addr, this, DATASTORE, client, index, way, offset);
		}
		entry = &cache->blocks[way][index];
		if (!entry->valid) {
			return;
		}
		if (caches_isolated) {
			printf("Write(%d) w/isolated cache 0x%x -> 0x%x\n", 2, data, addr);
		}
		cache_set_data_into_entry(entry, 2, offset, data);
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
Mapper::store_byte(uint32 addr, uint8 data, bool cacheable, DeviceExc *client)
{
	Range *l = NULL;
	uint32 offset, way, index;
	Cache::Entry *entry = NULL;

	if (cacheable) {
		Cache *cache;
		if (caches_swapped) {
			cache = icache;
		} else {
			cache = dcache;
		}
		if (!cache->cache_hit(addr, index, way, offset)) {
			//cache miss
			cache->cache_fetch(addr, this, DATASTORE, client, index, way, offset);
		}
		entry = &cache->blocks[way][index];
		if (!entry->valid) {
			return;
		}
		if (caches_isolated) {
			printf("Write(%d) w/isolated cache 0x%x -> 0x%x\n", 1, data, addr);
		}
		cache_set_data_into_entry(entry, 1, offset, data);
		return;
	}

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
