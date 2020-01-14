#ifndef _ACCELERATOR_H_
#define _ACCELERATOR_H_

#include "router.h"
#include "types.h"
#include "range.h"
#include "acceleratorcore.h"

#define DONE_NOTIF_ADDR 0x00000
#define DMAC_NOTIF_ADDR 0x00001

//Cube Network interface status
#define CNIF_IDLE		0
#define CNIF_ERROR		1
#define CNIF_SR_HEAD	2 //reply write head
#define CNIF_BR_HEAD	3
#define CNIF_SR_DATA	4
#define CNIF_BR_DATA	5
#define CNIF_SW_DATA	6
#define CNIF_BW_DATA	7

#define CNIF_WRITEDATA	4

#define CNIF_SWRITE	3
#define CNIF_BWRITE	4
#define CNIF_RREQ	5
#define CNIF_SREAD	10
#define CNIF_BREAD	11
#define CNIF_DREQ	12
#define CNIF_DREAD	13
#define CNIF_DONE	14
#define CNIF_DFIN	15
#define CNIF_DEND1	16
#define CNIF_DEND2	17

class Range;
class AcceleratorCore;

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

//abstract class for accelerator top module
class CubeAccelerator {
private:
	//data to/from router
	bool iready[VCH_SIZE];
	RouterPortSlave *rtRx;
	RouterPortMaster *rtTx;
	Router *localRouter;

	//submodules
	AcceleratorCore *core_module;

	//Cube nif
	uint32 reg_mema, reg_mtype, reg_vch, reg_src, reg_dst;
	int dcount;
	int packetMaxSize;
	int nif_state, nif_next_state;
	bool dmac_en;
	int mem_bandwidth;


	void nif_step();

protected:
	//constructor
	CubeAccelerator(uint32 node_ID, Router* upperRouterm, bool dmac_en_ = true);

	//data/address bus
	LocalMapper* localBus;

public:
	//destructor
	virtual ~CubeAccelerator() {};

	//control flow
	void step();
	void reset();

	//make submodules and connect them to bus
	virtual void setup() = 0;

	//accelerator name
	virtual const char *accelerator_name() = 0;

	Router* getRouter() { return localRouter; };

};


#endif //_ACCELERATOR_H_

