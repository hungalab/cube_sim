#ifndef _DMAC_H_
#define _DMAC_H_

#include "deviceexc.h"
#include "mapper.h"
#include "range.h"
#include "deviceint.h"

//Address map
#define DMAC_ADDR_BASE		0xBD030000
#define DMAC_CONF_SIZE		0x20
#define DMAC_ENABLE_OFFSET	0x00000000
#define DMAC_INTEN_OFFSET	0x00000004
#define DMAC_CTRL_OFFSET	0x00000008
#define DMAC_STATUS_OFFSET	0x0000000C
#define DMAC_ATTR_OFFSET	0x00000010
#define DMAC_LEN_OFFSET		0x00000014
#define DMAC_SRC_OFFSET		0x00000018
#define DMAC_DST_OFFSET		0x0000001C
#define DMAC_DONELEN_OFFSET	0x00000020

//BITMAP
#define DMAC_ENABLED_BIT	0x1
#define DMAC_INTEN_BIT		0x1
#define DMAC_START_BIT		0x1
#define DMAC_ABORT_BIT		0x2
#define DMAC_BUSY_BIT		0x1
#define DMAC_DONE_BIT		0x2
#define DMAC_BUSERR_BIT		0x4
#define DMAC_ADRERR_BIT		0x8
#define DMAC_ESTATE_BIT		0x10
#define DMAC_BURST_BIT		0x1
#define DMAC_ZERO_BIT		0x2

//State machine
#define DMAC_STAT_IDLE			0x0
#define DMAC_STAT_READING		0x1
#define DMAC_STAT_READ_REQ		0x2
#define DMAC_STAT_WRITING		0x3
#define DMAC_STAT_WRITE_REQ		0x4
#define DMAC_STAT_READ_DONE		0x5
#define DMAC_STAT_WRITE_DONE	0x6
#define DMAC_STAT_EXIT			0x7

class Mapper;

struct DMA_query_t {
	uint32 src;
	uint32 dst;
	uint32 len;
	bool zero_write;
	bool burst;
};

class DMACConfig : public Range {
	private:
		bool byteswapped;
		bool kicked;
		//activate
		bool enabled;
		//interrupt enabled
		bool irq_en;
		//ctrl
		bool abort;
		//status
		bool busy;
		bool done;
		bool addr_err;
		bool bus_err;
		bool end_stat;
		//attr
		bool zero_write;
		bool burst;
		//query
		uint32 dma_len, dma_src, dma_dst;
		uint32 success_len;

	public:
		DMACConfig();
		void reset();
		uint32 fetch_word(uint32 offset, int mode,
							DeviceExc *client);
		uint16 fetch_halfword(uint32 offset, DeviceExc *client);
		uint8 fetch_byte(uint32 offset, DeviceExc *client);
		void store_word(uint32 offset, uint32 data,
							DeviceExc *client);
		void store_halfword(uint32 offset, uint16 data,
							DeviceExc *client);
		void store_byte(uint32 offset, uint8 data,
							DeviceExc *client);

		bool isKicked() { return kicked; }
		bool isEnabled() { return enabled; }
		bool isAbort() { return abort; }
		bool isDone() { return done; }
		bool isIRQEn() { return irq_en; }
		void negateKicked() { kicked = false; }
		void assertBusy() { busy = true; }
		void negateBusy() { busy = false; }
		void assertDone() { done = true; }
		void assertAddrErr() { addr_err = true; }
		void assertBusErr() { bus_err = true; }
		void set_success_len(uint32 len) { success_len = len; }
		void setIsLastWrite(bool flag) { end_stat = flag; }
		DMA_query_t getQuery();
};


class DMAC : public DeviceExc, public DeviceInt  {
	private:
		Mapper *bus;
		DMACConfig *config;
		uint32 *buffer;
		int status;
		int next_status;

		int block_words;
		DMA_query_t query;
		int counter;
		int word_counter;
		int mem_bandwidth;

		bool address_valid(uint32 addr);
	public:
		//Constructor
		DMAC(Mapper &m);
		void exception(uint16 excCode, int mode = ANY,
			int coprocno = -1);

		// Control-flow methods.
		void step ();
		void reset ();

		//for device int
		const char *descriptor_str() const;
};


#endif /* _DMAC_H_ */