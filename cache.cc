/*  Write back cache for more practical simulation
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


#include "cache.h"
#include "excnames.h"
#include "mapper.h"

//#define CACHE_DEBUG

Cache::Cache(Mapper* mem, unsigned int block_count_, unsigned int block_size_, unsigned int way_size_) :
	physmem(mem),
	block_count(block_count_),
	block_size(block_size_),
	way_size(way_size_)
{
	//block_size: byte size
	blocks = new Entry*[way_size];
	word_size = block_size / 4;
	for (int i = 0; i < way_size; i++) {
		blocks[i] = new Entry[block_count];
		for (int j = 0; j < block_count; j++) {
			blocks[i][j].data = new uint32[word_size];
			blocks[i][j].dirty = false;
			blocks[i][j].valid = false;
			blocks[i][j].last_access = machine->num_cycles;
		}
	}

	// analyze bit format
	offset_len = int(std::log2(block_size));
	index_len = int(std::log2(block_count));

	// init cache profile
	cache_hit_counts = 0;
	cache_miss_counts = 0;
	cache_wb_counts = 0;
	isisolated = false;

	// init cache status
	status = next_status = CACHE_IDLE;
	last_state_update_time = 0;
}

Cache::~Cache()
{
	delete [] blocks;
}

void Cache::step()
{
	if (machine->num_cycles > last_state_update_time) {
		status = next_status;
		last_state_update_time = machine->num_cycles;
	}

	if (status == CACHE_WB || status == CACHE_OP_WB) {
		cache_wb();
	} else if (status == CACHE_FETCH) {
		cache_fetch();
	}
}

void Cache::reset_stat()
{
	// If exception occurs while cache working, cache must be reset
	if (status != CACHE_IDLE) {
		status = CACHE_IDLE;
		physmem->release_bus(cache_op_state->client);
		delete cache_op_state;
	}
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

bool Cache::ready(uint32 addr)
{
	// just return whether the cache block is available
	uint32 index, way, offset;
	return cache_hit(addr, index, way, offset);
}

bool Cache::cache_hit(uint32 addr, uint32 &index, uint32 &way, uint32 &offset)
{
	// in case of cache hit index, way, offset will be set
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

void Cache::request_block(uint32 addr, int mode, DeviceExc* client)
{
	int least_recent_used_way = 0;
	bool find = false;

	uint32 tag, index, way, offset;
	addr_separete(addr, tag, index, offset);

	//if cache is working or bus is busy, request is ignored
	if (status == CACHE_IDLE && physmem->acquire_bus(client)) {
		//find free block or LRU block
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
		if (!find) { // in case of no free block
			way = least_recent_used_way;
			//check if WB is needed
			if (!isisolated && mode != INSTFETCH && blocks[way][index].dirty) {
				next_status = CACHE_WB;
#if defined(CACHE_DEBUG)
				fprintf(stderr, "WB cache block way(%d) index(%d) is used for addr(0x%x)\n", way, index, addr);
#endif
			} else {
				next_status = CACHE_FETCH;
			}
		} else {
			next_status = CACHE_FETCH;
		}

#if defined(CACHE_DEBUG)
		if (next_status == CACHE_FETCH) {
			fprintf(stderr, "cache block way(%d) index(%d) is used for addr(0x%x)\n", way, index, addr);
		}
#endif
		// regist replaced cache block & status
		cache_op_state = new CacheOpState{word_size, addr, way, index, mode, false, client};
		cache_miss_counts++;
	}
}


void Cache::cache_wb()
{
	uint32 wb_addr = calc_addr(cache_op_state->way, cache_op_state->index);
	wb_addr += (word_size - cache_op_state->counter) * 4;
	unsigned int way = cache_op_state->way;
	unsigned int index = cache_op_state->index;

	// request for access at first
	if (cache_op_state->counter == word_size) {
		for (int i = 0; i < word_size; i++) {
			physmem->request_word(wb_addr + (4 * i), DATASTORE, cache_op_state->client);
		}
	}

	// if access is ready, write back the data
	if (physmem->ready(wb_addr, DATASTORE, cache_op_state->client)) {
		physmem->store_word(wb_addr,
			physmem->host_to_mips_word(blocks[way][index].data[word_size - cache_op_state->counter]),
			cache_op_state->client);

		if (--cache_op_state->counter == 0) {
			//finish write back
			if (status == CACHE_OP_WB) {
				//no need to fetch block
				blocks[way][index].dirty = false;
				if (cache_op_state->last_invalidate) {
					blocks[way][index].valid = false;
				}
				next_status = CACHE_IDLE;
				delete cache_op_state;
				physmem->release_bus(cache_op_state->client);
			} else {
				next_status = CACHE_FETCH;
				cache_op_state->counter = word_size;
			}
			cache_wb_counts++;
		}
	}

}

void Cache::cache_fetch()
{
	unsigned int tag, way, index, offset;
	uint32 fetch_addr = (cache_op_state->requested_addr & ~((1 << offset_len) - 1))
							+ ((word_size - cache_op_state->counter) * 4);

	way = cache_op_state->way;
	index = cache_op_state->index;

	int mode = cache_op_state->mode == INSTFETCH ? INSTFETCH : DATALOAD;

	// request for access at first
	if (cache_op_state->counter == word_size) {
		for (int i = 0; i < word_size; i++) {
			physmem->request_word(fetch_addr + (4 * i), mode, cache_op_state->client);
		}
	}

	// if access is ready, fetch the data
	if (physmem->ready(fetch_addr, mode, cache_op_state->client)) {
		blocks[way][index].data[word_size - cache_op_state->counter] =
			physmem->host_to_mips_word(physmem->fetch_word(fetch_addr, mode, cache_op_state->client));

		if (--cache_op_state->counter == 0) {
			// finish cache fetch
			next_status = CACHE_IDLE;
			addr_separete(fetch_addr, tag, index, offset);
			blocks[way][index].tag = tag;
			blocks[way][index].valid = true;
			blocks[way][index].dirty = false;
			delete cache_op_state;
			physmem->release_bus(cache_op_state->client);
		}
	}
}

bool Cache::exec_cache_op(uint16 opcode, uint32 addr, DeviceExc* client)
{
	uint32 tag, index, way, offset;
	uint32 least_recent_used_way = 0;
	int mode = DATALOAD;
	bool last_invalidate = false;
	bool stall = false;
	bool find;

	//if it causes cpu stall, return false
	if (status == CACHE_IDLE && physmem->acquire_bus(client)) {
		switch (opcode) {
			//invalidate by index
			case ICACHE_OP_IDX_INV:
				mode = INSTFETCH;
			case DCACHE_OP_IDX_INV:
				fprintf(stderr, "cache index invalidate not implemented\n");
				break;
			//write back & invalidate by index
			case DCACHE_OP_IDX_WB:
				fprintf(stderr, "cache index writeback not implemented\n");
				break;
			case DCACHE_OP_IDX_WBINV:
				fprintf(stderr, "cache index writeback, invalidate not implemented\n");
				break;
			//load tag by index
			case ICACHE_OP_IDX_LTAG:
				mode = INSTFETCH;
			case DCACHE_OP_IDX_LTAG:
				fprintf(stderr, "cache tag load not implemented\n");
				break;
			//store tag by inedx
			case ICACHE_OP_IDX_STAG:
				mode = INSTFETCH;
			case DCACHE_OP_IDX_STAG:
				fprintf(stderr, "cache tag store not implemented\n");
				break;
			//force write back by index
			case DCACHE_OP_IDX_FWB:
				fprintf(stderr, "cache index force writeback not implemented\n");
				break;
			//force write back & invalidate by index
			case DCACHE_OP_IDX_FWBINV:
				fprintf(stderr, "cache index force writeback, invalidate not implemented\n");
				break;
			//setline
			case DCACHE_OP_SETLINE:

				if (!cache_hit(addr, index, way, offset)) {
					addr_separete(addr, tag, index, offset);
					//find free block or oldest block
					for (way = 0, find = false; way < way_size; way++) {
						if (!blocks[way][index].valid) {
							find = true;
							break;
						}
						if (blocks[least_recent_used_way][index].last_access >
								blocks[way][index].last_access) {
							//update
							least_recent_used_way = way;
						}
					}

					if (!find) {
						// block replace
						way = least_recent_used_way;
					}
					if (blocks[way][index].dirty) {
						next_status = CACHE_OP_WB;
					} else {
						blocks[way][index].valid = true;
						//overwrite tag
						blocks[way][index].tag = tag;
						blocks[way][index].dirty = false;
					}
				}
				break;
			//invalidate by cache hit
			case ICACHE_OP_HIT_INV:
				mode = INSTFETCH;
			case DCACHE_OP_HIT_INV:
				if (cache_hit(addr, index, way, offset)) {
					blocks[way][index].valid = false;
				}
				break;
			//(force) write back (&invalidate) by cache hit
			case DCACHE_OP_HIT_WBINV:
			case DCACHE_OP_HIT_FWBINV:
				last_invalidate = true;
			case DCACHE_OP_HIT_WB:
			case DCACHE_OP_HIT_FWB:
				if (cache_hit(addr, index, way, offset)) {
					if (blocks[way][index].dirty) {
						next_status = CACHE_OP_WB;
					} else if (opcode == DCACHE_OP_HIT_FWB || opcode == DCACHE_OP_HIT_INV) {
						next_status = CACHE_OP_WB;
					}
				}
				break;
			//change
			case DCACHE_OP_CHANGE:
				fprintf(stderr, "cache change not implemented\n");
				break;
			//reverse change
			case DCACHE_OP_RCHANGE:
				fprintf(stderr, "cache reverse change not implemented\n");
				break;
		}
		if (next_status == CACHE_OP_WB) {
			cache_op_state = new CacheOpState{word_size, addr, way, index, mode, last_invalidate, client};
			stall = true;
		}
	} else {
		//cache is working
		stall = true;
	}

	return !stall;
}

uint32 Cache::fetch_word(uint32 addr, int32 mode, DeviceExc *client)
{
	uint32 offset;
	uint32 way, index;
	Cache::Entry *entry = NULL;

	if (addr % 4 != 0) {
		client->exception(AdEL,mode);
		return 0xffffffff;
	}

	if (!cache_hit(addr, index, way, offset)) {
		return 0xffffffff;
	} else {
		cache_hit_counts++;
	}

	//get data
	entry = &blocks[way][index];
	if (!entry->valid) {
		return 0xffffffff;
	}
	if (isisolated) {
		printf("Isolated word read returned 0x%x\n", entry->data[offset>>2]);
	}
	entry->last_access = machine->num_cycles;
	return entry->data[offset>>2];
}

uint16 Cache::fetch_halfword(uint32 addr, DeviceExc *client)
{

	uint32 offset, index, way;
	Cache::Entry *entry = NULL;

	if (addr % 2 != 0) {
		client->exception(AdEL,DATALOAD);
		return 0xffff;
	}


	if (!cache_hit(addr, index, way, offset)) {
		return 0xffff;
	} else {
		cache_hit_counts++;
	}

	entry = &blocks[way][index];
	if (!entry->valid) {
		return 0xffff;
	}
	//addr check
	uint32 n;
	n = (offset >> 1) & 0x1;
	if (physmem->byteswapped)
		n = 1 - n;

	if (isisolated) {
		printf("Isolated word read returned 0x%x\n", ((uint16 *)(&entry->data[offset>>2]))[n]);
	}
	entry->last_access = machine->num_cycles;
	return ((uint16 *)(&entry->data[offset>>2]))[n];

}

uint8 Cache::fetch_byte(uint32 addr, DeviceExc *client)
{
	uint32 offset, index, way;
	Cache::Entry *entry = NULL;

	if (!cache_hit(addr, index, way, offset)) {
		return 0xff;
	} else {
		cache_hit_counts++;
	}

	entry = &blocks[way][index];
	if (!entry->valid) {
		return 0xff;
	}

	uint32 n;
	n = (offset & 0x3);
	if (physmem->byteswapped)
	   n = 3 - n;

	if (isisolated) {
		printf("Isolated word read returned 0x%x\n", ((uint8 *)(&entry->data[offset>>2]))[n]);
	}
	entry->last_access = machine->num_cycles;
	return ((uint8 *)(&entry->data[offset>>2]))[n];
}

void Cache::store_word(uint32 addr, uint32 data, DeviceExc *client)
{
	uint32 offset, way, index;
	Cache::Entry *entry = NULL;

	if (addr % 4 != 0) {
		client->exception(AdES,DATASTORE);
		return;
	}

	if (!cache_hit(addr, index, way, offset)) {
		return;
	} else {
		cache_hit_counts++;
	}

	entry = &blocks[way][index];
	if (!entry->valid) {
		return;
	}
	if (isisolated) {
		printf("Write(%d) w/isolated cache 0x%x -> 0x%x\n", 4, data, addr);
	}
	entry->data[offset>>2] = data;
	entry->dirty = true;
	entry->last_access = machine->num_cycles;
	return;

}

void Cache::store_halfword(uint32 addr, uint16 data,  DeviceExc *client)
{
	uint32 offset, way, index;
	Cache::Entry *entry = NULL;

	if (addr % 2 != 0) {
		client->exception(AdES,DATASTORE);
		return;
	}

	if (!cache_hit(addr, index, way, offset)) {
		return ;
	} else {
		cache_hit_counts++;
	}

	entry = &blocks[way][index];
	if (!entry->valid) {
		return;
	}
	if (isisolated) {
		printf("Write(%d) w/isolated cache 0x%x -> 0x%x\n", 4, data, addr);
	}
	uint32 n;
	n = (offset >> 1) & 0x1;
	if (physmem->byteswapped)
	    n = 1 - n;
	((uint16 *)(&entry->data[offset>>2]))[n] = (uint16)data;
	entry->dirty = true;
	entry->last_access = machine->num_cycles;
	return;
}

void Cache::store_byte(uint32 addr, uint8 data, DeviceExc *client)
{
	uint32 offset, way, index;
	Cache::Entry *entry = NULL;

	if (!cache_hit(addr, index, way, offset)) {
		return ;
	} else {
		cache_hit_counts++;
	}

	entry = &blocks[way][index];
	if (!entry->valid) {
		return;
	}
	if (isisolated) {
		printf("Write(%d) w/isolated cache 0x%x -> 0x%x\n", 4, data, addr);
	}
	uint32 n;
	n = offset & 0x3;
	if (physmem->byteswapped)
	    n = 3 - n;
	((uint8 *)(&entry->data[offset>>2]))[n] = (uint8)data;
	entry->dirty = true;
	entry->last_access = machine->num_cycles;
	return;
}

void Cache::report_prof()
{
	uint32 cache_access = cache_miss_counts + cache_hit_counts;
	fprintf(stderr, "\tAccess Count %d\n", cache_access);
	fprintf(stderr, "\tCache Miss Ratio %.5f%%\n",
		(double)cache_miss_counts / (double)cache_access * 100.0);
	fprintf(stderr, "\twrite back ratio %.5f%%\n",
		(double)cache_wb_counts / (double)cache_miss_counts * 100.0);

}
