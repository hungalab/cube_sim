#include <stdio.h>
#include "routerinterface.h"
#include "accesstypes.h"
#include "excnames.h"
#include "vmips.h"
#include "options.h"

/*******************************  RouterIOReg  *******************************/
RouterIOReg::RouterIOReg(RouterInterface *_rtif) :
							rtif(_rtif), DeviceMap(0x3F0000) {
	config = new RouterInterface::RTConfig_t();
	rtif->setConfig(config);
	clear_reg();
}

RouterIOReg::~RouterIOReg()
{
	delete config;
}

void RouterIOReg::clear_reg() {
	//init IO regs
	abort = 0;
	*config = {0, {0,0,0}, 0, {0,0,0}, 0, 0, 0, 0};
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
			return config->router_id;
			break;
		case ROUTER_DVCH_NODE0_OFFSET:
			return config->data_vch[0];
			break;
		case ROUTER_DVCH_NODE1_OFFSET:
			return config->data_vch[1];
			break;
		case ROUTER_DVCH_NODE2_OFFSET:
			return config->data_vch[2];
			break;
		case ROUTER_IREADY_OFFSET:
			return config->iready;
			break;
		case ROUTER_INTVCH_NODE0_OFFSET:
			return config->int_vch[0];
			break;
		case ROUTER_INTVCH_NODE1_OFFSET:
			return config->int_vch[1];
			break;
		case ROUTER_INTVCH_NODE2_OFFSET:
			return config->int_vch[2];
			break;
		case ROUTER_DONE_STAT_OFFSET:
			return config->done_status;
			break;
		case ROUTER_DONE_MASK_OFFSET:
			return config->done_mask;
			break;
		case ROUTER_DMAC_STAT_OFFSET:
			return config->dmac_status;
			break;
		case ROUTER_DMAC_MASK_OFFSET:
			return config->dmac_mask;
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
			config->router_id = data & ROUTER_ID_BITMASK;
			break;
		case ROUTER_DVCH_NODE0_OFFSET:
			config->data_vch[0] = data & ROUTER_DVCH_NODE0_BITMASK;
			break;
		case ROUTER_DVCH_NODE1_OFFSET:
			config->data_vch[1] = data & ROUTER_DVCH_NODE1_BITMASK;
			break;
		case ROUTER_DVCH_NODE2_OFFSET:
			config->data_vch[2] = data & ROUTER_DVCH_NODE2_BITMASK;
			break;
		case ROUTER_IREADY_OFFSET:
			config->iready = data & ROUTER_IREADY_BITMASK;
			break;
		case ROUTER_INTVCH_NODE0_OFFSET:
			config->int_vch[0] = data & ROUTER_INTVCH_NODE0_BITMASK;
			break;
		case ROUTER_INTVCH_NODE1_OFFSET:
			config->int_vch[1] = data & ROUTER_INTVCH_NODE1_BITMASK;
			break;
		case ROUTER_INTVCH_NODE2_OFFSET:
			config->int_vch[2] = data & ROUTER_INTVCH_NODE2_BITMASK;
			break;
		case ROUTER_DONE_STAT_OFFSET:
			config->done_status = data & ROUTER_DONE_STAT_BITMASK;
			break;
		case ROUTER_DONE_MASK_OFFSET:
			config->done_mask = data & ROUTER_DONE_MASK_BITMASK;
			break;
		case ROUTER_DMAC_STAT_OFFSET:
			config->dmac_status = data & ROUTER_DMAC_STAT_BITMASK;
			break;
		case ROUTER_DMAC_MASK_OFFSET:
			config->dmac_mask = data & ROUTER_DMAC_MASK_BITMASK;
			break;
		case ROUTER_ABORT_OFFSET:
			//reset signal handling here
			if (((abort & ROUTER_RESET_BIT) != 0) &&
				((data & ROUTER_ABORT_BITMASK & ROUTER_RESET_BIT == 0))) {
				//reset signal falling
				rtif->reset();
			} else if (((abort & ROUTER_RESET_BIT) != 0) &&
				((data & ROUTER_ABORT_BITMASK & ROUTER_RESET_BIT) == 0)) {
				//abort signal falling
				rtif->abort();
			}
			abort = data & ROUTER_ABORT_BITMASK;
			break;
		default:
			client->exception(DBE, DATASTORE);
			break;
	}
}

/*******************************  RouterRange  *******************************/
RouterRange::RouterRange(RouterInterface *_rtif, bool block_en) :
		rtif(_rtif), block_mode(block_en), DeviceMap(0xC10000) {
}

bool RouterRange::ready(uint32 offset, int32 mode, DeviceExc *client) {

	if (mode == DATALOAD) {
		if (!rtif->isBusy()) {
			rtif->start(offset - getBase(), mode, client, block_mode);
			return false;
		} else {
			if (rtif->isDataArrived()) {
				return true;
			} else {
				return false;
			}
		}
	} else if (mode == DATASTORE) {
		if (!rtif->isBusy()) {
			rtif->start(offset - getBase(), mode, client, block_mode);
			return true;
		} else {
			if (rtif->isUnderSetup()) {
				return true;
			} else {
				return false;
			}
		}
	} else {
		//Bus Error
		if (machine->opt->option("dbemsg")->flag) {
			fprintf(stderr, "instruction load is not available"
					" at physical address 0x%x via router\n", offset);
		}
		client->exception((mode == INSTFETCH ? IBE : DBE), mode);
		return false;
	}

}

uint32 RouterRange::fetch_word(uint32 offset, int mode, DeviceExc *client) {
	return rtif->dequeue();
}

void RouterRange::store_word(uint32 offset, uint32 data, DeviceExc *client) {
	rtif->enqueue(data);
}

/*******************************  RouterInterface  *******************************/
RouterInterface::RouterInterface() {
	next_state = state = RT_STATE_IDLE;
	packet_size = machine->opt->option("dcachebsize")->num / 4; //word size
}

int RouterInterface::getNodeID(uint32 addr)
{
	if ((ROUTER_NODE1_OFFSET > addr) && (addr >= ROUTER_NODE0_OFFSET)) {
		return 1;
	} else if ((ROUTER_NODE2_OFFSET > addr) && (addr >= ROUTER_NODE1_OFFSET)) {
		return 2;
	} else if (addr >= ROUTER_NODE2_OFFSET) {
		return 3;
	}

	return -1;
}

void RouterInterface::start(uint32 offset, int32 mode,
							DeviceExc *client, bool block_mode) {
	req_addr = offset;
	if (mode == DATALOAD && block_mode) {
		next_state = RT_STATE_BR_HEAD;
	} else if (mode == DATALOAD && !block_mode) {
		next_state = RT_STATE_SR_HEAD;
	} else if (mode == DATASTORE && block_mode) {
		next_state = RT_STATE_BW_SETUP;
	} else if (mode == DATASTORE && !block_mode) {
		next_state = RT_STATE_SW_SETUP;
	}
}

void RouterInterface::step() {
	FLIT_t flit;
	int node_id;
	uint32 send_data;
	state = next_state;
	if (state != RT_STATE_IDLE) {
		switch (state) {
			case RT_STATE_SW_SETUP:
				if (send_fifo.size() == 1) {
					next_state = RT_STATE_SW_HEAD;
				}
				break;
			case RT_STATE_BW_SETUP:
				if (send_fifo.size() == packet_size) {
					next_state = RT_STATE_BW_HEAD;
				}
				break;
			case RT_STATE_SR_HEAD:
				node_id = getNodeID(req_addr);
				RouterUtils::make_head_flit(&flit, req_addr, MTYPE_SR, config->data_vch[node_id],
											config->router_id, getNodeID(req_addr), true);
				next_state = RT_STATE_SR_WAIT;
				break;
			case RT_STATE_SW_HEAD:
				node_id = getNodeID(req_addr);
				RouterUtils::make_head_flit(&flit, req_addr, MTYPE_SW, config->data_vch[node_id],
											config->router_id, getNodeID(req_addr));
				next_state = RT_STATE_SW_DATA;
				break;
			case RT_STATE_BR_HEAD:
				node_id = getNodeID(req_addr);
				RouterUtils::make_head_flit(&flit, req_addr, MTYPE_BR, config->data_vch[node_id],
											config->router_id, getNodeID(req_addr), true);
				next_state = RT_STATE_BR_WAIT;
				break;
			case RT_STATE_BW_HEAD:
				node_id = getNodeID(req_addr);
				RouterUtils::make_head_flit(&flit, req_addr, MTYPE_BW, config->data_vch[node_id],
											config->router_id, getNodeID(req_addr));
				next_state = RT_STATE_BW_DATA;
				break;
			case RT_STATE_SW_DATA:
				send_data = send_fifo.front();
				send_fifo.pop();
				RouterUtils::make_data_flit(&flit, send_data);
				next_state = RT_STATE_IDLE;
				break;
			case RT_STATE_BW_DATA:
				send_data = send_fifo.front();
				send_fifo.pop();
				RouterUtils::make_data_flit(&flit, send_data, send_fifo.size() == 0);
				if (send_fifo.size() == 0)
					next_state = RT_STATE_IDLE;
				break;
			case RT_STATE_SR_WAIT:
				if (recv_fifo.size() == 1) {
					next_state = RT_STATE_DATA_RDY;
				} else {
					//dummy
					recv_fifo.push(0);
				}
				break;
			case RT_STATE_BR_WAIT:
				if (recv_fifo.size() == packet_size) {
					next_state = RT_STATE_DATA_RDY;
				} else {
					//dummy
					recv_fifo.push(0);
				}
				break;
			case RT_STATE_DATA_RDY:
				if (recv_fifo.size() == 0) {
					next_state = RT_STATE_IDLE;
				}
				break;
		}
	}
}

void RouterInterface::reset() {
	next_state = state = RT_STATE_IDLE;
	FIFO empty0, empty1;
	// clear
	std::swap(send_fifo, empty0);
	std::swap(recv_fifo, empty1);
}

void RouterInterface::abort() {
	next_state = state = RT_STATE_IDLE;
}

void RouterInterface::setConfig(RTConfig_t* config_)
{
	config = config_;
}

void RouterInterface::enqueue(uint32 data)
{
	send_fifo.push(data);
}

uint32 RouterInterface::dequeue()
{
	uint32 ret_data = recv_fifo.front();
	recv_fifo.pop();
	return ret_data;
}
