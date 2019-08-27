#include "cache.h"
#include "excnames.h"
#include "mapper.h"

//#define CACHE_DEBUG

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
			blocks[i][j].last_access = machine->num_cycles;
		}
	}

	offset_len = int(std::log2(block_size));
	index_len = int(std::log2(block_count));
	cache_hit_counts = 0;
	cache_miss_counts = 0;
	cache_wb_counts = 0;
	isisolated = false;
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

void Cache::cache_wb(Mapper* physmem, int mode, DeviceExc *client, uint32 index, uint32 way)
{
	uint32 wb_addr = calc_addr(way, index);
	uint32 wb_offset = 0;
	Range *l = NULL;
	for (int i = 0; i < block_size / 4; i++, wb_addr += 4) {
		l = physmem->find_mapping_range(wb_addr);
		if (!l) {
			physmem->bus_error(client, mode, wb_addr, 4);
			return;
		}
		wb_offset = wb_addr - l->getBase();
		l->store_word(wb_offset, physmem->host_to_mips_word(blocks[way][index].data[i]), client);
	}
	cache_wb_counts++;
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
		if (!isisolated && mode != INSTFETCH && blocks[way][index].dirty) {
			//cache WB
#if defined(CACHE_DEBUG)
			printf("WB cache block way(%d) index(%d) is used for addr(0x%x)\n", way, index, addr);
#endif
			cache_wb(physmem, mode, client, index, way);
		}
	}

#if defined(CACHE_DEBUG)
	printf("cache block way(%d) index(%d) is used for addr(0x%x)\n", way, index, addr);
#endif

	blocks[way][index].tag = tag;
	blocks[way][index].valid = true;
	blocks[way][index].dirty = false;

	if (isisolated && mode != INSTFETCH) {
		// no need to fetch
		return;
	}

	//set line
	l = NULL;
	uint32 fetch_offset;
	addr = calc_addr(way, index);
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

uint32 Cache::fetch_word(Mapper* physmem, uint32 addr, int32 mode, DeviceExc *client)
{
	uint32 offset;
	uint32 way, index;
	Cache::Entry *entry = NULL;

	if (addr % 4 != 0) {
		client->exception(AdEL,mode);
		return 0xffffffff;
	}

	//check if cache block is fetched
	if (!cache_hit(addr, index, way, offset)) {
		//cache miss
		cache_fetch(addr, physmem, mode, client, index, way, offset);
		cache_miss_counts++;
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
	return entry->data[offset>>2];
}

uint16 Cache::fetch_halfword(Mapper* physmem, uint32 addr, DeviceExc *client)
{

	uint32 offset, index, way;
	Cache::Entry *entry = NULL;

	if (addr % 2 != 0) {
		client->exception(AdEL,DATALOAD);
		return 0xffff;
	}


	if (!cache_hit(addr, index, way, offset)) {
		//cache miss
		cache_fetch(addr, physmem, DATALOAD, client, index, way, offset);
		cache_miss_counts++;
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
	return ((uint16 *)(&entry->data[offset>>2]))[n];

}

uint8 Cache::fetch_byte(Mapper* physmem, uint32 addr, DeviceExc *client)
{
	uint32 offset, index, way;
	Cache::Entry *entry = NULL;

	if (!cache_hit(addr, index, way, offset)) {
		//cache miss
		cache_fetch(addr, physmem, DATALOAD, client, index, way, offset);
		cache_miss_counts++;
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
	return ((uint8 *)(&entry->data[offset>>2]))[n];


}

void Cache::store_word(Mapper* physmem, uint32 addr, uint32 data, DeviceExc *client)
{
	uint32 offset, way, index;
	Cache::Entry *entry = NULL;

	if (addr % 4 != 0) {
		client->exception(AdES,DATASTORE);
		return;
	}

	if (!cache_hit(addr, index, way, offset)) {
		//cache miss
		cache_fetch(addr, physmem, DATASTORE, client, index, way, offset);
		cache_miss_counts++;
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
	return;

}

void Cache::store_halfword(Mapper* physmem, uint32 addr, uint16 data,  DeviceExc *client)
{
	uint32 offset, way, index;
	Cache::Entry *entry = NULL;

	if (addr % 2 != 0) {
		client->exception(AdES,DATASTORE);
		return;
	}

	if (!cache_hit(addr, index, way, offset)) {
		//cache miss
		cache_fetch(addr, physmem, DATASTORE, client, index, way, offset);
		cache_miss_counts++;
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
	return;
}

void Cache::store_byte(Mapper* physmem, uint32 addr, uint8 data, DeviceExc *client)
{
	uint32 offset, way, index;
	Cache::Entry *entry = NULL;

	if (!cache_hit(addr, index, way, offset)) {
		//cache miss
		cache_fetch(addr, physmem, DATASTORE, client, index, way, offset);
		cache_miss_counts++;
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
	return;
}

void Cache::report_prof()
{
	uint32 cache_access = cache_miss_counts + cache_hit_counts;
	fprintf(stderr, "\tCache Miss Ratio %.5f%%\n",
		(double)cache_miss_counts / (double)cache_access * 100.0);
	fprintf(stderr, "\twrite back ratio %.5f%%\n",
		(double)cache_wb_counts / (double)cache_miss_counts * 100.0);

}