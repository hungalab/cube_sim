#ifndef _ROUTERINTERFACE_H_
#define _ROUTERINTERFACE_H_

#include "deviceexc.h"
#include "devicemap.h"
#include "router.h"
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
#define ROUTER_NODE0_OFFSET			0x000000 //Actually + BA40_0000: BA40_0000
#define ROUTER_NODE1_OFFSET			0x400000 //Actually + BA40_0000: BA80_0000
#define ROUTER_NODE2_OFFSET			0x800000 //Actually + BA40_0000: BAC0_0000
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
#define RT_STATE_SW_SETUP	0x1 //waiting for send data from core (single)
#define RT_STATE_BW_SETUP	0x2 //waiting for send data from core (block)
#define RT_STATE_SR_HEAD	0x3
#define RT_STATE_SW_HEAD	0x4
#define RT_STATE_BR_HEAD	0x5
#define RT_STATE_BW_HEAD	0x6
#define RT_STATE_SW_DATA	0x7
#define RT_STATE_BW_DATA	0x8
#define RT_STATE_SR_WAIT	0x9
#define RT_STATE_BR_WAIT	0xA
#define RT_STATE_DATA_RDY	0xB //waiting for arrived data is read

class RouterUtils;
class Router;
class RouterPortMaster;
class RouterPortSlave;

typedef std::queue<uint32> FIFO;

class RouterInterface : public DeviceExc {
public:
	struct RTConfig_t {
		uint32 router_id;
		uint32 data_vch[3];
		uint32 iready;
		uint32 int_vch[3];
		uint32 done_status, dmac_status;
		uint32 done_mask, dmac_mask;
	};

private:
	int packet_size; //equals to data cache block size
	//Send/Recv FIFO
	FIFO send_fifo, recv_fifo;
	uint32 req_addr;
	uint32 use_vch;

	//router status
	int state, next_state;

	int getNodeID(uint32 addr);

	RTConfig_t* config;

    //data to/from router
	RouterPortSlave *rtRx;
	RouterPortMaster *rtTx;
	Router *localRouter;
	bool recvRdy[VCH_SIZE];

public:
	//Constructor
	RouterInterface();
	~RouterInterface() {};

	// Control-flow methods.
	void step ();
	void reset ();
	void exception(uint16 excCode, int mode = ANY,
		int coprocno = -1) { };

	// router control
	bool isBusy() { return next_state != RT_STATE_IDLE; };
	bool isDataArrived() { return state == RT_STATE_DATA_RDY; };
	bool isUnderSetup() { return state == RT_STATE_SW_SETUP || state == RT_STATE_BW_SETUP ||
									next_state == RT_STATE_SW_SETUP || next_state == RT_STATE_BW_SETUP; };
	void start(uint32 offset, int32 mode, DeviceExc *client, bool block_mode); //kick data trans. via router
    void abort(); //stop sending & waiting for data
    void setConfig(RTConfig_t* config_);

    //data to/from core
    void enqueue(uint32 data);
    uint32 dequeue();

};

class RouterIOReg: public DeviceMap {
private:
	//IO Regs
	RouterInterface::RTConfig_t *config;
	uint32 abort;
	RouterInterface *rtif;
	//clear regs.
	void clear_reg();

public:
	//Constructor
	RouterIOReg(RouterInterface *_rtif);
	~RouterIOReg();

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

#endif /* _ROUTERINTERFACE_H_ */