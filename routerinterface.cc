#include <stdio.h>
#include "routerinterface.h"
#include "accesstypes.h"
#include "excnames.h"
#include "vmips.h"
#include "options.h"
#include "accelerator.h"

/*******************************  RouterIOReg  *******************************/
RouterIOReg::RouterIOReg(RouterInterface *_rtif) :
							rtif(_rtif), DeviceMap(0x3F0000) {
	//init configuration & regist it to interface
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
	*config = {0, {0,0,0}, INIT_IREADY, {0,0,0},
				INIT_DONEDMAC_STAT, INIT_DONEDMAC_STAT,
				INIT_DONEDMAC_MASK, INIT_DONEDMAC_MASK};
}

//convert binary to Boolean array
void RouterIOReg::bin2bool(uint32 bindata, bool *boolarray, int len)
{
	for (int i = 0; i < len; i++) {
		if ((((unsigned int)1 << i) & bindata) != 0) {
			boolarray[i] = true;
		} else {
			boolarray[i] = false;
		}
	}
}

//convert Boolean array to binary
uint32 RouterIOReg::bool2bin(bool *boolarray, int len)
{
	uint32 bindata = 0;
	for (int i = 0; i < len; i++) {
		if (boolarray[i]) {
			bindata += (1 << i);
		}
	}
	return bindata;
}

//ready signal to mapper(client)
bool RouterIOReg::ready(uint32 offset, int32 mode, DeviceExc *client)
{
	return (offset == ROUTER_ABORT_OFFSET) || !rtif->isBusy();
}

//read from client
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
			return bool2bin(config->iready, VCH_SIZE);
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
			return bool2bin(config->done_status, REMOTE_NODE_COUNT);
			break;
		case ROUTER_DONE_MASK_OFFSET:
			return bool2bin(config->done_mask, REMOTE_NODE_COUNT);
			break;
		case ROUTER_DMAC_STAT_OFFSET:
			return bool2bin(config->dmac_status, REMOTE_NODE_COUNT);
			break;
		case ROUTER_DMAC_MASK_OFFSET:
			return bool2bin(config->dmac_mask, REMOTE_NODE_COUNT);
			break;
		case ROUTER_ABORT_OFFSET:
			return abort;
			break;
		default:
			client->exception(DBE, DATALOAD);
			return 0xFFFFFFFF;
	}
}

//write from client
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
			bin2bool(data & ROUTER_IREADY_BITMASK, config->iready, VCH_SIZE);
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
			bin2bool(data & ROUTER_DONE_STAT_BITMASK, config->done_status, REMOTE_NODE_COUNT);
			break;
		case ROUTER_DONE_MASK_OFFSET:
			bin2bool(data & ROUTER_DONE_MASK_BITMASK, config->done_mask, REMOTE_NODE_COUNT);
			break;
		case ROUTER_DMAC_STAT_OFFSET:
			bin2bool(data & ROUTER_DMAC_STAT_BITMASK,config->dmac_status, REMOTE_NODE_COUNT);
			break;
		case ROUTER_DMAC_MASK_OFFSET:
			bin2bool(data & ROUTER_DMAC_MASK_BITMASK, config->dmac_mask, REMOTE_NODE_COUNT);
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

//ready signal to mapper(client)
bool RouterRange::ready(uint32 offset, int32 mode, DeviceExc *client) {

	if (mode == DATALOAD) {
		if (!rtif->isBusy()) {
			//in case of idle, kick router
			rtif->start(offset - getBase(), mode, client, block_mode);
			return false;
		} else {
			//wait until data is arrived
			if (rtif->isDataArrived()) {
				return true;
			} else {
				return false;
			}
		}
	} else if (mode == DATASTORE) {
		if (!rtif->isBusy()) {
			//in case of idle, kick router
			rtif->start(offset - getBase(), mode, client, block_mode);
			return true;
		} else {
			//wait for sent data
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

//read arrived data
uint32 RouterRange::fetch_word(uint32 offset, int mode, DeviceExc *client) {
	return rtif->dequeue();
}

//write sent data
void RouterRange::store_word(uint32 offset, uint32 data, DeviceExc *client) {
	rtif->enqueue(data);
}

/*******************************  RouterInterface  *******************************/
RouterInterface::RouterInterface() {
	next_state = state = RT_STATE_IDLE;
	packet_size = machine->opt->option("dcachebsize")->num / 4; //word size
	registed_router_id = 0;

	//make router ports
	rtRx = new RouterPortSlave(); //receiver
	rtTx = new RouterPortMaster(); //sender
	localRouter = new Router(rtTx, rtRx, NULL); //Router for cpu core
}

//to override DeviceInt
const char *RouterInterface::descriptor_str() const
{
	return "Router Interface";
}

//decode remote node ID
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

//router kicker
void RouterInterface::start(uint32 offset, int32 mode,
							DeviceExc *client, bool block_mode) {
	req_addr = offset;
	if (mode == DATALOAD && block_mode) {
		next_state = RT_STATE_BR_HEAD;
		clear_recv_fifo();
	} else if (mode == DATALOAD && !block_mode) {
		next_state = RT_STATE_SR_HEAD;
		clear_recv_fifo();
	} else if (mode == DATASTORE && block_mode) {
		next_state = RT_STATE_BW_SETUP;
		clear_send_fifo();
	} else if (mode == DATASTORE && !block_mode) {
		next_state = RT_STATE_SW_SETUP;
		clear_send_fifo();
	}
}

void RouterInterface::step() {
	FLIT_t flit;
	int node_id;
	uint32 send_data;

	//update router ID
	if (registed_router_id != config->router_id) {
		localRouter->setID(config->router_id);
	}

	// exec router
	localRouter->step();

	//handle received data
	if (rtRx->haveData()) {
		rtRx->getData(&flit);
		switch (flit.ftype) {
			case FTYPE_DATA:
			case FTYPE_TAIL:
				recv_fifo.push(flit.data);
				break;
			case FTYPE_HEADTAIL:
				//only DONE packet will be handled (others are ignored)
				uint32 addr, mtype, vch, src, dst;
				RouterUtils::decode_headflit(&flit, &addr, &mtype, &vch, &src, &dst);
				if (mtype == MTYPE_DONE) {
					if (addr == DONE_NOTIF_ADDR) {
						config->done_status[src - 1] = true;
					} else if (addr == DMAC_NOTIF_ADDR) {
						config->dmac_status[src - 1] = true;
					}
				}
				break;
		}
	}

	//check intrrupt signal
	if (checkHWint()) {
		assertInt(IRQ5);
	} else {
		deassertInt(IRQ5);
	}

	//update status
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
				use_vch = config->data_vch[node_id];
				if (rtTx->slaveReady(use_vch)) {
					RouterUtils::make_head_flit(&flit, req_addr, MTYPE_SR, use_vch ,
											config->router_id, getNodeID(req_addr), true);
					rtTx->send(&flit, use_vch);
					next_state = RT_STATE_SR_WAIT;
				}
				break;
			case RT_STATE_SW_HEAD:
				node_id = getNodeID(req_addr);
				use_vch = config->data_vch[node_id];
				if (rtTx->slaveReady(use_vch)) {
					RouterUtils::make_head_flit(&flit, req_addr, MTYPE_SW, use_vch,
											config->router_id, getNodeID(req_addr));
					rtTx->send(&flit, use_vch);
					next_state = RT_STATE_SW_DATA;
				}
				break;
			case RT_STATE_BR_HEAD:
				node_id = getNodeID(req_addr);
				use_vch = config->data_vch[node_id];
				if (rtTx->slaveReady(use_vch)) {
					RouterUtils::make_head_flit(&flit, req_addr, MTYPE_BR, use_vch,
											config->router_id, getNodeID(req_addr), true);
					rtTx->send(&flit, use_vch);
					next_state = RT_STATE_BR_WAIT;
				}
				break;
			case RT_STATE_BW_HEAD:
				node_id = getNodeID(req_addr);
				use_vch = config->data_vch[node_id];
				if (rtTx->slaveReady(use_vch)) {
					RouterUtils::make_head_flit(&flit, req_addr, MTYPE_BW, use_vch,
											config->router_id, getNodeID(req_addr));
					rtTx->send(&flit, use_vch);
					next_state = RT_STATE_BW_DATA;
				}
				break;
			case RT_STATE_SW_DATA:
				if (rtTx->slaveReady(use_vch)) {
					send_data = send_fifo.front();
					send_fifo.pop();
					RouterUtils::make_data_flit(&flit, send_data);
					rtTx->send(&flit, use_vch);
					next_state = RT_STATE_IDLE;
				}
				break;
			case RT_STATE_BW_DATA:
				if (rtTx->slaveReady(use_vch)) {
					send_data = send_fifo.front();
					send_fifo.pop();
					RouterUtils::make_data_flit(&flit, send_data, send_fifo.size() == 0);
					rtTx->send(&flit, use_vch);
					if (send_fifo.size() == 0)
						next_state = RT_STATE_IDLE;
				}
				break;
			case RT_STATE_SR_WAIT:
				if (recv_fifo.size() == 1) {
					next_state = RT_STATE_DATA_RDY;
				}
				break;
			case RT_STATE_BR_WAIT:
				if (recv_fifo.size() == packet_size) {
					next_state = RT_STATE_DATA_RDY;
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
	rtRx->setReadyStat(config->iready);
	localRouter->reset();
	clear_send_fifo();
	clear_recv_fifo();
}

void RouterInterface::abort() {
	next_state = state = RT_STATE_IDLE;
}

//registted by RouterIOReg
void RouterInterface::setConfig(RTConfig_t* config_)
{
	config = config_;
}


//FIFO controlling functions
void RouterInterface::clear_send_fifo() {
	FIFO empty;
	// clear
	std::swap(send_fifo, empty);
}

void RouterInterface::clear_recv_fifo() {
	FIFO empty;
	// clear
	std::swap(recv_fifo, empty);
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

bool RouterInterface::checkHWint()
{
	bool int_signal = false;
	for (int i = 0; i < REMOTE_NODE_COUNT; i++) {
		int_signal |= config->done_status[i] & config->done_mask[i];
		int_signal |= config->dmac_status[i] & config->dmac_mask[i];
	}
	return int_signal;
}
