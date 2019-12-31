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
#define ROUTER_RESET_BIT			0x0
#define ROUTER_ABORT_BIT			0x1
#define ROUTER_BERR_ABORT_BIT		0x2
#define ROUTER_NODE0_OFFSET			0x400000
#define ROUTER_NODE1_OFFSET			0x800000
#define ROUTER_NODE2_OFFSET			0xC00000



class RouterInterface : public DeviceMap, public DeviceExc {
private:

public:
	//Constructor
	RouterInterface();
	~RouterInterface() {};

	// Control-flow methods.
	void step ();
	void reset ();

	//access interface
	bool canRead(uint32 offset) { return true; };
    bool canWrite(uint32 offset) { return true; };

	uint32 fetch_word(uint32 offset, int mode, DeviceExc *client) { return 0;};
	void store_word(uint32 offset, uint32 data, DeviceExc *client) {};
};

#endif /* _ROUTER_H_ */