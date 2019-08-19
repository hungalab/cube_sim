#ifndef _CACHE_H_
#define _CACHE_H_

#include <cstdio>
#include <cmath>
#include "types.h"
#include "deviceexc.h"
#include "vmips.h"
#include "mapper.h"


class Mapper;

class Cache {
public:
	// member var
	unsigned int block_count;
	unsigned int block_size;
	unsigned int way_size;

    int cache_miss_counts;
    int cache_hit_counts;
    int cache_wb_counts;

	struct Entry {
            bool valid;
            bool dirty;
            int last_access;
            uint32 tag;
            uint32 *data;
    };

    Entry **blocks;

    // Constructor & Destructor
    Cache(unsigned int block_count_,
		  unsigned int block_size_,
		  unsigned int way_size_);
    ~Cache();

    // method
    bool cache_hit(uint32 addr, uint32 &index, uint32 &way, uint32 &offset);
    void cache_fetch(uint32 addr, Mapper* physmem, int mode, DeviceExc *client, uint32 &index, uint32 &way, uint32 &offset);
    void cache_wb(Mapper* physmem, int mode, DeviceExc *client, uint32 index, uint32 way);

    uint32 fetch_word(Mapper* physmem, uint32 addr, int32 mode, DeviceExc *client);
    uint16 fetch_halfword(Mapper* physmem, uint32 addr, DeviceExc *client);
    uint8 fetch_byte(Mapper* physmem, uint32 addr, DeviceExc *client);
    void store_word(Mapper* physmem, uint32 addr, uint32 data, DeviceExc *client);
    void store_halfword(Mapper* physmem, uint32 addr, uint16 data,  DeviceExc *client);
    void store_byte(Mapper* physmem, uint32 addr, uint8 data, DeviceExc *client);

    void cache_isolate(bool flag) {isisolated = flag;}
    void report_prof();

private:
	// member var
	unsigned int offset_len;
	unsigned int index_len;

	// method
	void addr_separete(uint32 addr, uint32 &tag, uint32 &index, uint32 &offset);

    uint32 calc_addr(uint32 way, uint32 index);
    bool isisolated;
};

#endif /*_CACHE_H_*/
