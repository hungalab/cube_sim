#include "accelerator.h"
#include "error.h"


/*******************************  LocalMapper  *******************************/
int LocalMapper::add_range(Range *r) {
	assert (r && "Null range object passed to Mapper::add_range()");

	/* Check to make sure the range is non-overlapping. */
	for (Ranges::iterator i = ranges.begin(); i != ranges.end(); i++) {
		if (r->overlaps(*i)) {
			error("Attempt to map two VMIPS components to the "
			       "same memory area: (base %x extent %x) and "
			       "(base %x extent %x).", r->getBase(),
			       r->getExtent(), (*i)->getBase(),
			       (*i)->getExtent());
			return -1;
		}
	}

	/* Once we're satisfied that it doesn't overlap, add it to the list. */
	ranges.push_back(r);
	return 0;
}

/* Deconstruction. Deallocate the range list. */
LocalMapper::~LocalMapper()
{
	for (Ranges::iterator i = ranges.begin(); i != ranges.end(); i++)
		delete *i;
}

uint32 LocalMapper::fetch_word(uint32 offset, int mode)
{
	return 0x12345678;
}

void LocalMapper::store_word(uint32 offset, uint32 data)
{

}

/*******************************  CubeAccelerator  *******************************/
CubeAccelerator::CubeAccelerator(uint32 node_ID, Router* upperRouter)
{
	//make router ports
	rtRx = new RouterPortSlave(iready); //receiver
	rtTx = new RouterPortMaster(); //sender
	localRouter = new Router(rtTx, rtRx, upperRouter, node_ID);
	localBus = new LocalMapper();
	core_module = NULL;
}

void CubeAccelerator::step()
{
	//handle data to/from router
	localRouter->step();

	if (core_module != NULL) {
		core_module->step();
	}

}

void CubeAccelerator::reset() {
	for (int i = 0; i < VCH_SIZE; i++) {
		iready[i] = false;
	}
	localRouter->reset();
	if (core_module != NULL) {
		core_module->reset();
	}
}