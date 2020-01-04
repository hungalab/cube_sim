#include <stdio.h>
#include "routerinterface.h"
#include "accesstypes.h"
#include "excnames.h"
#include "vmips.h"
#include "options.h"

RouterIOReg::RouterIOReg(RouterInterface *_rtif) :
							rtif(_rtif), DeviceMap(0x3F0000) {
	clear_reg();
}

void RouterIOReg::clear_reg() {
	//init IO regs
	router_id = 0;
	data_vch0 = 0, data_vch1 = 0, data_vch2 = 0;
	iready = 0;
	int_vch0 = 0, int_vch1 = 0, int_vch2 = 0;
	done_status = 0, dmac_status = 0;
	done_mask = 0, dmac_mask = 0;
	abort = 0;
}

bool RouterIOReg::ready(uint32 offset, int32 mode, DeviceExc *client)
{
	return (offset == ROUTER_ABORT_OFFSET) || !rtif->isBusy();
}

uint32 RouterIOReg::fetch_word(uint32 offset, int mode, DeviceExc *client)
{
	// Read from IO Regs
	switch(offset) {
		case ROUTER_ID_OFFSET:
			return router_id;
			break;
		case ROUTER_DVCH_NODE0_OFFSET:
			return data_vch0;
			break;
		case ROUTER_DVCH_NODE1_OFFSET:
			return data_vch1;
			break;
		case ROUTER_DVCH_NODE2_OFFSET:
			return data_vch2;
			break;
		case ROUTER_IREADY_OFFSET:
			return iready;
			break;
		case ROUTER_INTVCH_NODE0_OFFSET:
			return int_vch0;
			break;
		case ROUTER_INTVCH_NODE1_OFFSET:
			return int_vch1;
			break;
		case ROUTER_INTVCH_NODE2_OFFSET:
			return int_vch2;
			break;
		case ROUTER_DONE_STAT_OFFSET:
			return done_status;
			break;
		case ROUTER_DONE_MASK_OFFSET:
			return done_mask;
			break;
		case ROUTER_DMAC_STAT_OFFSET:
			return dmac_status;
			break;
		case ROUTER_DMAC_MASK_OFFSET:
			return dmac_mask;
			break;
		case ROUTER_ABORT_OFFSET:
			return abort;
			break;
		default:
			client->exception(DBE, DATALOAD);
			return 0xFFFFFFFF;
	}
}

void RouterIOReg::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	// Write to IO Regs
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
		case ROUTER_ABORT_OFFSET:
			abort = data & ROUTER_ABORT_BITMASK;
			break;
		default:
			client->exception(DBE, DATASTORE);
			break;
	}
}

RouterRange::RouterRange(RouterInterface *_rtif, bool block_en) :
		rtif(_rtif), block_mode(block_en), DeviceMap(0xC10000) {
}

bool RouterRange::ready(uint32 offset, int32 mode, DeviceExc *client) {
	if (rtif->isBusy()) {
		return true;
	} else {
		// router idle
		rtif->start(offset, mode, client, block_mode);
		return true;
	}
}

uint32 RouterRange::fetch_word(uint32 offset, int mode, DeviceExc *client) {
	return 0;
}

void RouterRange::store_word(uint32 offset, uint32 data, DeviceExc *client) {
	return;
}

RouterInterface::RouterInterface() {
	next_state = state = RT_STATE_IDLE;
}

void RouterInterface::start(uint32 offset, int32 mode,
							DeviceExc *client, bool block_mode) {
	if (mode == DATALOAD && block_mode) {
		next_state = RT_STATE_BR_HEAD;
	} else if (mode == DATALOAD && !block_mode) {
		next_state = RT_STATE_SR_HEAD;
	} else if (mode == DATASTORE && block_mode) {
		next_state = RT_STATE_BW_HEAD;
	} else if (mode == DATASTORE && !block_mode) {
		next_state = RT_STATE_SW_HEAD;
	} else {
		//Bus Error
		if (machine->opt->option("dbemsg")->flag) {
			fprintf (stderr, "%s %s physical address 0x%x via router caused bus error ",
				(mode == DATASTORE) ? "store" : "load",
				(mode == DATASTORE) ? "to" : "from",
				offset);
			fprintf (stderr, "\n");
		}
		client->exception((mode == INSTFETCH ? IBE : DBE), mode);
	}
}

void RouterInterface::step() {
	//check soft reset
	// bool router_reset = (abort & ROUTER_RESET_BIT) != 0;
	// //check abort
	// bool router_abort = (abort & ROUTER_ABORT_BIT) != 0;
	if (state != RT_STATE_IDLE) {

	}
	state = next_state;
}

void RouterInterface::reset() {
	next_state = state = RT_STATE_IDLE;
}

// bool RouterInterface::isIOReg(uint32 addr)
// {
// 	return (addr < ROUTER_NODE0_OFFSET);
// }

