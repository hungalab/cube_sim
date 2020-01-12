#ifndef _ACCELERATOR_H_
#define _ACCELERATOR_H_

#include "router.h"
#include "types.h"
#include "range.h"
#include "acceleratorcore.h"

#define DONE_NOTIF_ADDR 0x00000
#define DMAC_NOTIF_ADDR 0x00001

class Range;
class AcceleratorCore;

class LocalMapper {
private:
	typedef std::vector<Range *> Ranges;
	Ranges ranges;

	/* Add range R to the mapping. R must not overlap with any existing
	   ranges in the mapping. Return 0 if R added sucessfully or -1 if
	   R overlapped with an existing range. The Mapper takes ownership
	   of R. */

	int add_range (Range *r);

public:
	//constructor
	LocalMapper() {};
	~LocalMapper();

	/* Add range R to the mapping, as with add_range(), but first set its
	   base address to PA. The Mapper takes ownership of R. */
	int map_at_physical_address (Range *r, uint32 pa) {
		r->setBase (pa); return add_range (r);
	}

	// data access
	// In accelerators, only the word access is allowed
	virtual uint32 fetch_word(uint32 offset, int mode);
	virtual void store_word(uint32 offset, uint32 data);

};

//abstract class for accelerator top module
class CubeAccelerator {
private:
	//data to/from router
	bool iready[VCH_SIZE];
	RouterPortSlave *rtRx;
	RouterPortMaster *rtTx;
	Router *localRouter;

	//data/address bus
	LocalMapper* localBus;

	//submodules
	AcceleratorCore *core_module;

protected:
	//constructor
	CubeAccelerator(uint32 node_ID, Router* upperRouter);

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

