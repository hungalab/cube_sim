#include <stdio.h>
#include "routerinterface.h"
#include "accesstypes.h"
#include "excnames.h"

RouterInterface::RouterInterface() : DeviceMap(0x1000000) {
	//init IO regs
	router_id = 0;
	data_vch0 = 0, data_vch1 = 0, data_vch2 = 0;
	iready = 0;
	int_vch0 = 0, int_vch1 = 0, int_vch2 = 0;
	done_status = 0, dmac_status = 0;
	done_mask = 0, dmac_mask = 0;
	abort = 0, prev_abort = 0;
	//status
	router_busy = false;
}

void RouterInterface::step() {

}

void RouterInterface::reset() {

}

bool RouterInterface::isIOReg(uint32 addr)
{
	return (addr < ROUTER_NODE0_OFFSET);
}

bool RouterInterface::ready(uint32 offset, int32 mode, DeviceExc *client)
{
	return (offset == ROUTER_ABORT_OFFSET) || !router_busy;
}

uint32 RouterInterface::fetch_word(uint32 offset, int mode, DeviceExc *client)
{
	return 0;
}

void RouterInterface::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	if (isIOReg(offset)) {
		switch(offset) {
			case ROUTER_ID_OFFSET:
				router_id = data & ROUTER_ID_BITMASK;
				break;
			case ROUTER_DVCH_NODE0_OFFSET:
				data_vch0 = data & ROUTER_DVCH_NODE0_BITMASK;
				break;
			case ROUTER_DVCH_NODE1_OFFSET:
				data_vch1 = data & ROUTER_DVCH_NODE1_BITMASK;
				break;
			case ROUTER_DVCH_NODE2_OFFSET:
				data_vch2 = data & ROUTER_DVCH_NODE2_BITMASK;
				break;
			case ROUTER_IREADY_OFFSET:
				iready = data & ROUTER_IREADY_BITMASK;
				break;
			case ROUTER_INTVCH_NODE0_OFFSET:
				int_vch0 = data & ROUTER_INTVCH_NODE0_BITMASK;
				break;
			case ROUTER_INTVCH_NODE1_OFFSET:
				int_vch1 = data & ROUTER_INTVCH_NODE1_BITMASK;
				break;
			case ROUTER_INTVCH_NODE2_OFFSET:
				int_vch2 = data & ROUTER_INTVCH_NODE2_BITMASK;
				break;
			case ROUTER_DONE_STAT_OFFSET:
				done_status = data & ROUTER_DONE_STAT_BITMASK;
				break;
			case ROUTER_DONE_MASK_OFFSET:
				done_mask = data & ROUTER_DONE_MASK_BITMASK;
				break;
			case ROUTER_DMAC_STAT_OFFSET:
				dmac_status = data & ROUTER_DMAC_STAT_BITMASK;
				break;
			case ROUTER_DMAC_MASK_OFFSET:
				dmac_mask = data & ROUTER_DMAC_MASK_BITMASK;
				break;
			// case ROUTER_ABORT_OFFSET:
			// 	ROUTER_ABORT_BITMASK
			// 	break;
			default:
				client->exception(DBE, DATASTORE);
				break;
		}
	} else {
		//Router Kick

	}
}