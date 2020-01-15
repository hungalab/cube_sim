#ifndef _CACHE_H_
#define _CACHE_H_

#include <cstdio>
#include <cmath>
#include "types.h"
#include "deviceexc.h"
#include "vmips.h"
#include "mapper.h"

#define CACHE_IDLE  0
#define CACHE_WB    1
#define CACHE_FETCH 2


class Mapper;

class Cache {
public:

    // Constructor & Destructor
    Cache(Mapper* mem,
          unsigned int block_count_,
		  unsigned int block_size_,
		  unsigned int way_size_);
    ~Cache();

    // method
    void step();

    bool ready(uint32 addr);
    void request_block(uint32 addr, int mode, DeviceExc* client);
    void reset_stat();

    uint32 fetch_word(uint32 addr, int32 mode, DeviceExc *client);
    uint16 fetch_halfword(uint32 addr, DeviceExc *client);
    uint8 fetch_byte(uint32 addr, DeviceExc *client);
    void store_word(uint32 addr, uint32 data, DeviceExc *client);
    void store_halfword(uint32 addr, uint16 data,  DeviceExc *client);
    void store_byte(uint32 addr, uint8 data, DeviceExc *client);

    void cache_isolate(bool flag) {isisolated = flag;}
    void report_prof();

private:
    // connected memory
    Mapper* physmem;

	// cache config
    unsigned int block_count;
    unsigned int block_size;
    unsigned int way_size;
    unsigned int word_size; // = block_size / 4

    // bit format
    unsigned int offset_len;
    unsigned int index_len;

    // cache profile
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

    // cache status
    bool isisolated;
    unsigned int status;
    unsigned int next_status;
    int last_state_update_time;

    struct CacheOpState {
        unsigned int counter;
        uint32 requested_addr;
        uint32 way;
        uint32 index;
        int mode;
        DeviceExc *client;
    };

    CacheOpState *cache_op_state;

	// method
	void addr_separete(uint32 addr, uint32 &tag, uint32 &index, uint32 &offset);
    bool cache_hit(uint32 addr, uint32 &index, uint32 &way, uint32 &offset);
    // void cache_fetch(uint32 addr, Mapper* physmem, int mode, DeviceExc *client, uint32 &index, uint32 &way, uint32 &offset);
    // void cache_wb(Mapper* physmem, int mode, DeviceExc *client, uint32 index, uint32 way);
    void cache_fetch();
    void cache_wb();
    uint32 calc_addr(uint32 way, uint32 index);

};

#endif /*_CACHE_H_*/
