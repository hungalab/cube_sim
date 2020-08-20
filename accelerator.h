#ifndef _ACCELERATOR_H_
#define _ACCELERATOR_H_

#include "router.h"
#include "types.h"
#include "range.h"
#include "mapper.h"
#include "debugutils.h"
#include "devicemap.h"

#define DONE_NOTIF_ADDR 0x00000
#define DMAC_NOTIF_ADDR 0x00001
#define LOCAL_MEMA_MSB	22
#define LOCAL_MEMA_MASK	0x3FFFFF

// cnif config regs
#define NIF_CONFIG_BASE			0x04100
#define NIF_CONFIG_SIZE			0x900 //default 0x4100~0x5000
#define DMA_KICK_OFFSET			0x00000
#define DMA_DST_OFFSET			0x00100
#define DMA_DST_MASK			0xFFFFFF //[23:0]
#define DMA_SRC_OFFSET			0x00200
#define DMA_SRC_MASK			0x3FFFFF //[21:0]
#define DMA_LEN_OFFSET			0x00300
#define DMA_LEN_MASK			0xF		 //[3:0]; max 15words
#define VC_LIST_OFFSET			0x00400
#define VC_NORMAL_MASK			0xE00
#define VC_NORMAL_LSB			9
#define VC_DMA_MASK				0x1C0
#define VC_DMA_LSB				6
#define VC_DMADONE_MASK			0x38
#define VC_DMADONE_LSB			3
#define VC_DONE_MASK			0x7

//Cube Network interface status
#define CNIF_IDLE		0
#define CNIF_ERROR		1
#define CNIF_SR_HEAD	2 //reply write head
#define CNIF_BR_HEAD	3
#define CNIF_SR_DATA	4
#define CNIF_BR_DATA	5
#define CNIF_SW_DATA	6
#define CNIF_BW_DATA	7
#define CNIF_DMA_HEAD	8
#define CNIF_DMA_DATA	9
#define CNIF_DMA_DONE	10
#define CNIF_DONE		11

//allocated size in the address space per node
#define AC_KSEG0_TOP	0x9A400000
#define AC_KSEG1_TOP	0xBA400000
#define AC_ALLOC_SIZE	0x400000

class Range;
class Mapper;

class LocalMapper {
private:
	typedef std::vector<Range *> Ranges;
	Ranges ranges;

	//find cache
	Range *last_used_mapping;

	/* Add range R to the mapping. R must not overlap with any existing
	   ranges in the mapping. Return 0 if R added sucessfully or -1 if
	   R overlapped with an existing range. The Mapper takes ownership
	   of R. */

	int add_range (Range *r);
	Range * find_mapping_range(uint32 laddr);

public:
	//constructor
	LocalMapper() : last_used_mapping(NULL) {};
	~LocalMapper();

	/* Add range R to the mapping, as with add_range(), but first set its
	   base address to PA. The Mapper takes ownership of R. */
	int map_at_local_address (Range *r, uint32 laddr) {
		r->setBase (laddr); return add_range (r);
	}

	// data access
	// In accelerators, only the word access is allowed
	uint32 fetch_word(uint32 laddr);
	void store_word(uint32 laddr, uint32 data);

};

class SysBusInterface : public Range {
private:
	LocalMapper *lbus;
public:
	SysBusInterface(LocalMapper *lbus_) : lbus(lbus_),
		Range (0, AC_ALLOC_SIZE, 0, MEM_READ_WRITE) {};
	~SysBusInterface() {};
	//bus connection
	uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
	uint16 fetch_halfword(uint32 offset, DeviceExc *client);
	uint8 fetch_byte(uint32 offset, DeviceExc *client);
	void store_word(uint32 offset, uint32 data, DeviceExc *client);
	void store_halfword(uint32 offset, uint16 data,
		DeviceExc *client);
	void store_byte(uint32 offset, uint8 data, DeviceExc *client);
};

class NetworkInterfaceConfig : public Range {
private:
	uint32 dma_dst, dma_src, dma_len;
	uint32 vc_normal, vc_dma, vc_dmadone, vc_done;
	bool dmaKicked;
	bool dma_en;
public:
	//Constructor
	NetworkInterfaceConfig(uint32 config_addr_base, bool dma_en_);
	~NetworkInterfaceConfig() {}

	void clearReg();
	uint32 getDMADstAddr()	{ return dma_dst & LOCAL_MEMA_MASK; }
	uint32 getDMADstID()	{ return dma_dst >> LOCAL_MEMA_MSB; }
	uint32 getDMAsrc()		{ return dma_src; }
	uint32 getDMAlen()		{ return dma_len; }
	uint32 getVCnormal()	{ return vc_normal; }
	uint32 getVCdma()		{ return vc_dma; }
	uint32 getVCdmadone()	{ return vc_dmadone; }
	uint32 getVCdone()		{ return vc_done; }
	bool isDMAKicked()		{ return dmaKicked & dma_en; }
	void clearDMAKicked()	{ dmaKicked = false; }

	uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
	void store_word(uint32 offset, uint32 data, DeviceExc *client);

};

//Base class for accelertor
class AcceleratorBase {
protected:
	AcceleratorBase();
	//data/address bus
	LocalMapper* localBus;

	virtual void core_step() = 0;
	virtual void core_reset() = 0;

public:
	//make submodules and connect them to bus
	virtual void setup() = 0;
	//accelerator name
	virtual const char *accelerator_name() = 0;

};

//abstract class for accelerator top module
class CubeAccelerator : virtual public AcceleratorBase,
	public DebuggerClient {
private:
	//data to/from router
	bool iready[VCH_SIZE];
	RouterPortSlave *rtRx;
	RouterPortMaster *rtTx;
	Router *localRouter;

	//Cube nif
	int node_ID;
	NetworkInterfaceConfig* nif_config;
	uint32 reg_mema, reg_mtype, reg_vch, reg_src, reg_dst;
	int dcount;
	int packetMaxSize;
	int nif_state, nif_next_state;
	bool dmac_en;
	int mem_bandwidth;
	int remain_dma_len;
	bool done_pending;
	bool dma_after_done_en;

	void nif_step();

protected:
	//constructor
	CubeAccelerator() : dmac_en(false) {}; //for bus mode, nothing to do
	CubeAccelerator(uint32 node_ID_, Router* upperRouter,
		uint32 config_addr_base = NIF_CONFIG_BASE, bool dmac_en_ = true);

public:
	//destructor
	virtual ~CubeAccelerator() {};

	//control flow
	void step();
	void reset();

	void done_signal(bool dma_enable);

	Router* getRouter() { return localRouter; };

};

class BusConAccelerator : public DeviceExc,
			virtual public AcceleratorBase {
private:
	SysBusInterface *if_single, *if_burst;

protected:
	BusConAccelerator();

public:
	//destructor
	virtual ~BusConAccelerator() {};

	//control flow
	void step();
	void reset();

	void exception(uint16 excCode, int mode, int coprocno);

	void connect_to_bus(Mapper *sysbus, int kseg0_addr,
							int kseg1_addr);

};


#endif //_ACCELERATOR_H_

