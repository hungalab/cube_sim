#ifndef _ROUTER_H_
#define _ROUTER_H_

#include "deviceexc.h"
#include "devicemap.h"
#include <queue>

// router setting regs
#define ROUTER_ID_OFFSET			0x10000 //2bit
#define ROUTER_DVCH_NODE0_OFFSET	0x10004 //3bit virtual channel for data trans
#define ROUTER_DVCH_NODE1_OFFSET	0x10008 //3bit
#define ROUTER_DVCH_NODE2_OFFSET	0x1000C //3bit
#define ROUTER_IREADY_OFFSET		0x10010 //8bit
#define ROUTER_INTVCH_NODE0_OFFSET	0x10014 //3bit virtual channel for done nortif.
#define ROUTER_INTVCH_NODE1_OFFSET	0x10018 //3bit
#define ROUTER_INTVCH_NODE2_OFFSET	0x1001C //3bit
#define ROUTER_DONE_STAT_OFFSET		0x10020 //3bit
#define ROUTER_DONE_MASK_OFFSET		0x10024 //3bit(1: Interrupt Enabled)
#define ROUTER_DMAC_STAT_OFFSET		0x10028 //3bit
#define ROUTER_DMAC_MASK_OFFSET		0x1002C //3bit(1: Interrupt Enabled)
#define ROUTER_ABORT_OFFSET			0x10030 //3bit
#define ROUTER_RESET_BIT			0x0
#define ROUTER_ABORT_BIT			0x1
#define ROUTER_BERR_ABORT_BIT		0x2
#define ROUTER_NODE0_OFFSET			0x400000
#define ROUTER_NODE1_OFFSET			0x800000
#define ROUTER_NODE2_OFFSET			0xC00000
//BITMASK
#define ROUTER_ID_BITMASK			0x3
#define ROUTER_DVCH_NODE0_BITMASK	0x7
#define ROUTER_DVCH_NODE1_BITMASK	0x7
#define ROUTER_DVCH_NODE2_BITMASK	0x7
#define ROUTER_IREADY_BITMASK		0xFF
#define ROUTER_INTVCH_NODE0_BITMASK	0x7
#define ROUTER_INTVCH_NODE1_BITMASK	0x7
#define ROUTER_INTVCH_NODE2_BITMASK	0x7
#define ROUTER_DONE_STAT_BITMASK	0x7
#define ROUTER_DONE_MASK_BITMASK	0x7
#define ROUTER_DMAC_STAT_BITMASK	0x7
#define ROUTER_DMAC_MASK_BITMASK	0x7
#define ROUTER_ABORT_BITMASK		0x7

typedef std::queue<uint32> Queue;

class RouterInterface : public DeviceMap, public DeviceExc {
private:
	//Send/Recv FIFO
	Queue send_fifo, recv_fifo;
	//IO Regs
	uint32 router_id;
	uint32 data_vch0, data_vch1, data_vch2;
	uint32 iready;
	uint32 int_vch0, int_vch1, int_vch2;
	uint32 done_status, dmac_status;
	uint32 done_mask, dmac_mask;
	uint32 abort, prev_abort;

	//address decode
	bool isIOReg(uint32 addr);

	//router status
	bool router_busy;

public:
	//Constructor
	RouterInterface();
	~RouterInterface() {};

	// Control-flow methods.
	void step ();
	void reset ();
	void exception(uint16 excCode, int mode = ANY,
		int coprocno = -1) { };

	//access interface
	bool ready(uint32 offset, int32 mode, DeviceExc *client);
	bool canRead(uint32 offset) { return true; };
    bool canWrite(uint32 offset) { return true; };

	uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
	void store_word(uint32 offset, uint32 data, DeviceExc *client);
};

#endif /* _ROUTER_H_ */