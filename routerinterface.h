#ifndef _ROUTER_H_
#define _ROUTER_H_

#include "deviceexc.h"
#include "devicemap.h"
#include <queue>

// router setting regs
#define ROUTER_ID_OFFSET			0x0000 //2bit
#define ROUTER_DVCH_NODE0_OFFSET	0x0004 //3bit virtual channel for data trans
#define ROUTER_DVCH_NODE1_OFFSET	0x0008 //3bit
#define ROUTER_DVCH_NODE2_OFFSET	0x000C //3bit
#define ROUTER_IREADY_OFFSET		0x0010 //8bit
#define ROUTER_INTVCH_NODE0_OFFSET	0x0014 //3bit virtual channel for done nortif.
#define ROUTER_INTVCH_NODE1_OFFSET	0x0018 //3bit
#define ROUTER_INTVCH_NODE2_OFFSET	0x001C //3bit
#define ROUTER_DONE_STAT_OFFSET		0x0020 //3bit
#define ROUTER_DONE_MASK_OFFSET		0x0024 //3bit(1: Interrupt Enabled)
#define ROUTER_DMAC_STAT_OFFSET		0x0028 //3bit
#define ROUTER_DMAC_MASK_OFFSET		0x002C //3bit(1: Interrupt Enabled)
#define ROUTER_ABORT_OFFSET			0x0030 //3bit
#define ROUTER_RESET_BIT			0x1
#define ROUTER_ABORT_BIT			0x2
#define ROUTER_BERR_ABORT_BIT		0x4 //not implemented
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

//ROUTER IF STATE
#define RT_STATE_IDLE		0x0
#define RT_STATE_SR_HEAD	0x1
#define RT_STATE_SW_HEAD	0x2
#define RT_STATE_BR_HEAD	0x3
#define RT_STATE_BW_HEAD	0x4
#define RT_STATE_SW_DATA	0x5
#define RT_STATE_BW_DATA	0x6
#define RT_READ_DONE		0x7


typedef std::queue<uint32> FIFO;

class RouterInterface : public DeviceExc {
private:
	//Send/Recv FIFO
	FIFO send_fifo, recv_fifo;

	//router status
	int state, next_state;

public:
	//Constructor
	RouterInterface();
	~RouterInterface() {};

	// Control-flow methods.
	void step ();
	void reset ();
	void exception(uint16 excCode, int mode = ANY,
		int coprocno = -1) { };
	bool isBusy() { return next_state != RT_STATE_IDLE; };
	void start(uint32 offset, int32 mode, DeviceExc *client, bool block_mode);

};

class RouterIOReg: public DeviceMap {
private:
	//IO Regs
	uint32 router_id;
	uint32 data_vch0, data_vch1, data_vch2;
	uint32 iready;
	uint32 int_vch0, int_vch1, int_vch2;
	uint32 done_status, dmac_status;
	uint32 done_mask, dmac_mask;
	uint32 abort, prev_abort;
	RouterInterface *rtif;
	//clear regs.
	void clear_reg();

public:
	//Constructor
	RouterIOReg(RouterInterface *_rtif);
	~RouterIOReg() {};

	//access interface
	bool ready(uint32 offset, int32 mode, DeviceExc *client);
	bool canRead(uint32 offset) { return true; };
    bool canWrite(uint32 offset) { return true; };

	uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
	void store_word(uint32 offset, uint32 data, DeviceExc *client);
};

class RouterRange: public DeviceMap {
private:
	RouterInterface *rtif;
	bool block_mode;
public:
	//Constructor
	RouterRange(RouterInterface *_rtif, bool block_en);
	~RouterRange() {};
	//access interface
	bool ready(uint32 offset, int32 mode, DeviceExc *client);
	bool canRead(uint32 offset) { return true; };
    bool canWrite(uint32 offset) { return true; };

	uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
	void store_word(uint32 offset, uint32 data, DeviceExc *client);
};

#endif /* _ROUTER_H_ */