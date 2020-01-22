#include "accelerator.h"
#include "error.h"
#include "vmips.h"
#include "options.h"
#include "accesstypes.h"
#include <cassert>

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

Range * LocalMapper::find_mapping_range(uint32 laddr)
{
	if (last_used_mapping && last_used_mapping->incorporates(laddr))
		return last_used_mapping;

	for (Ranges::iterator i = ranges.begin(), e = ranges.end(); i != e; ++i) {
		if ((*i)->incorporates(laddr)) {
			last_used_mapping = *i;
			return *i;
		}
	}
	return NULL;
}

uint32 LocalMapper::fetch_word(uint32 laddr)
{
	Range *l = find_mapping_range(laddr);
	uint32 offset = laddr - l->getBase();

	if (l != NULL) {
		return l->fetch_word(offset, DATALOAD, NULL);
	} else {
		if (machine->opt->option("dbemsg")->flag) {
			fprintf(stderr, "Load from unmapped local address 0x%08x\n", laddr);
		}
	}
	return 0xffffffff;
}

void LocalMapper::store_word(uint32 laddr, uint32 data)
{
	Range *l = find_mapping_range(laddr);
	uint32 offset = laddr - l->getBase();

	if (l != NULL) {
		l->store_word(offset, data, NULL);
	} else {
		if (machine->opt->option("dbemsg")->flag) {
			fprintf(stderr, "Store to unmapped local address 0x%08x\n", laddr);
		}
	}
	return;
}

/*******************************  NetworkInterfaceConfig  *******************************/
NetworkInterfaceConfig::NetworkInterfaceConfig(uint32 config_addr_base)
	: Range(config_addr_base, 0x900, 0, MEM_READ_WRITE)
{
	clearReg();
}

void NetworkInterfaceConfig::clearReg()
{
	dma_dst = 0;
	dma_src = 0;
	dma_len = 0;
	vc_normal = 0;
	vc_dma = 0;
	vc_dmadone = 0;
	vc_done = 0;
	dmaKicked = false;
}

uint32 NetworkInterfaceConfig::fetch_word(uint32 offset, int mode, DeviceExc *client)
{

	switch (offset) {
		case DMA_KICK_OFFSET:
			return 0;
			break;
		case DMA_DST_OFFSET:
			return dma_dst;
			break;
		case DMA_SRC_OFFSET:
			return dma_src;
			break;
		case DMA_LEN_OFFSET:
			return dma_len;
			break;
		case VC_LIST_OFFSET:
			return ((vc_normal << VC_NORMAL_LSB) | (vc_dma << VC_DMA_LSB) |
					(vc_dmadone << VC_DMADONE_LSB) | vc_done);
			break;
		default:
			return 0xffffffff;
	}
}

void NetworkInterfaceConfig::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	switch (offset) {
		case DMA_KICK_OFFSET:
			dmaKicked = true;
			break;
		case DMA_DST_OFFSET:
			dma_dst = data & DMA_DST_MASK;
			break;
		case DMA_SRC_OFFSET:
			dma_src = data & DMA_SRC_MASK;
			break;
		case DMA_LEN_OFFSET:
			dma_len = data & DMA_LEN_MASK;
			break;
		case VC_LIST_OFFSET:
			vc_normal = (data & VC_NORMAL_MASK) >> VC_NORMAL_LSB;
			vc_dma = (data & VC_DMA_MASK) >> VC_DMA_LSB;
			vc_dmadone = (data & VC_DMADONE_MASK) >> VC_DMADONE_LSB;
			vc_done = data & VC_DONE_MASK;
			break;
	}
	return;
}

/*******************************  CubeAccelerator  *******************************/
CubeAccelerator::CubeAccelerator(uint32 node_ID, Router* upperRouter, uint32 config_addr_base, bool dmac_en_)
	: dmac_en(dmac_en_)
{
	//make router ports
	rtRx = new RouterPortSlave(iready); //receiver
	rtTx = new RouterPortMaster(); //sender
	//build router
	localRouter = new Router(rtTx, rtRx, upperRouter, node_ID);
	//make localbus
	localBus = new LocalMapper();
	core_module = NULL;

	//setup network interface
	nif_state = nif_next_state = CNIF_IDLE;
	packetMaxSize = (machine->opt->option("dcachebsize")->num / 4);
	mem_bandwidth = machine->opt->option("mem_bandwidth")->num;
	nif_config = new NetworkInterfaceConfig(config_addr_base);
	localBus->map_at_local_address(nif_config, config_addr_base);

	done_signal_ptr = std::bind(&CubeAccelerator::done_signal, this, std::placeholders::_1);

}

void CubeAccelerator::nif_step()
{
	FLIT_t flit;
	FLIT_t sflit;
	uint32 recv_vch;
	uint32 read_addr, write_addr;

	switch (nif_state) {
		case CNIF_IDLE:
			if (rtRx->haveData()) {
				rtRx->getData(&flit, &recv_vch);
				if (flit.ftype == FTYPE_HEAD || flit.ftype == FTYPE_HEADTAIL) {
					RouterUtils::decode_headflit(&flit, &reg_mema, &reg_mtype,
													&reg_vch, &reg_src, &reg_dst);
					switch (reg_mtype) {
						case MTYPE_SW:
							nif_next_state = CNIF_SW_DATA;
							break;
						case MTYPE_BW:
							dcount = packetMaxSize;
							nif_next_state = CNIF_BW_DATA;
							break;
						case MTYPE_SR:
							nif_next_state = CNIF_SR_HEAD;
							break;
						case MTYPE_BR:
							dcount = packetMaxSize;
							nif_next_state = CNIF_BR_HEAD;
							break;
						default:
							if (machine->opt->option("routermsg")->flag) {
								fprintf(stderr, "%s: unknown message type(%d) is received (flit %X_%08X)\n",
										accelerator_name(), reg_mtype, flit.ftype, flit.data);
							}
					}
				}
			}
			break;
		case CNIF_SR_HEAD:
		case CNIF_BR_HEAD:
			if(rtTx->slaveReady(nif_config->getVCnormal())) {
				nif_next_state = nif_state == CNIF_SR_HEAD ? CNIF_SR_DATA : CNIF_BR_DATA;
				//response of read request; thus write message
				RouterUtils::make_head_flit(&sflit, reg_mema, nif_state == CNIF_BR_HEAD ? MTYPE_BW : MTYPE_SW,
											nif_config->getVCnormal(), reg_dst, reg_src);
				rtTx->send(&sflit, nif_config->getVCnormal());
			}
			break;
		case CNIF_SR_DATA:
			RouterUtils::make_data_flit(&sflit, localBus->fetch_word(reg_mema), true);
			rtTx->send(&sflit, nif_config->getVCnormal());
			nif_next_state = CNIF_IDLE;
			break;
		case CNIF_BR_DATA:
			for (int i = 0; i < mem_bandwidth; i++) {
				read_addr = reg_mema + (packetMaxSize - dcount--) * 4;
				RouterUtils::make_data_flit(&sflit, localBus->fetch_word(read_addr), dcount == 0);
				rtTx->send(&sflit, nif_config->getVCnormal());
				if (dcount == 0) {
					nif_next_state = CNIF_IDLE;
					break;
				}
			}
			break;
		case CNIF_SW_DATA:
			if (rtRx->haveData()) {
				rtRx->getData(&flit, &recv_vch);
				localBus->store_word(reg_mema, flit.data);
				if (nif_config->isDMAKicked() & (nif_config->getDMAlen() > 0)) {
					nif_next_state = CNIF_DMA_HEAD;
					remain_dma_len = nif_config->getDMAlen();
				} else {
					nif_next_state = CNIF_IDLE;
				}
			}
			break;
		case CNIF_BW_DATA:
			for (int i = 0; i < mem_bandwidth; i++) {
				if (rtRx->haveData()) {
					rtRx->getData(&flit, &recv_vch);
					write_addr = reg_mema + (packetMaxSize - dcount) * 4;
					localBus->store_word(write_addr, flit.data);
					if (--dcount == 0) {
						if (nif_config->isDMAKicked() & (nif_config->getDMAlen() > 0)) {
							nif_next_state = CNIF_DMA_HEAD;
							remain_dma_len = nif_config->getDMAlen();
						} else {
							nif_next_state = CNIF_IDLE;
						}
						break;
					}
				}
			}
			break;
		case CNIF_DMA_HEAD:
			if(rtTx->slaveReady(nif_config->getVCdma())) {
				write_addr = nif_config->getDMADstAddr() + packetMaxSize * 4 * (nif_config->getDMAlen() - remain_dma_len);
				RouterUtils::make_head_flit(&sflit, write_addr, MTYPE_BW, nif_config->getVCdma(),
											reg_dst, nif_config->getDMADstID());
				rtTx->send(&sflit, nif_config->getVCdma());
				nif_next_state = CNIF_DMA_DATA;
				dcount = packetMaxSize;
			}
			break;
		case CNIF_DMA_DATA:
			for (int i = 0; i < mem_bandwidth; i++) {
				read_addr = nif_config->getDMAsrc() + packetMaxSize * 4 * (nif_config->getDMAlen() - remain_dma_len)
							+ (packetMaxSize - dcount--) * 4;
				RouterUtils::make_data_flit(&sflit, localBus->fetch_word(read_addr), dcount == 0);
				rtTx->send(&sflit, nif_config->getVCdma());
				if (dcount == 0) {
					remain_dma_len--;
					if (remain_dma_len == 0) {
						nif_next_state = CNIF_DMA_DONE;
						nif_config->clearDMAKicked();
					} else {
						nif_next_state = CNIF_DMA_HEAD;
					}
					break;
				}
			}
			break;
		case CNIF_DMA_DONE:
			if(rtTx->slaveReady(nif_config->getVCdmadone())) {
				RouterUtils::make_head_flit(&sflit, DMAC_NOTIF_ADDR, MTYPE_DONE,
											nif_config->getVCdmadone(), reg_dst, reg_src, true);
				rtTx->send(&sflit, nif_config->getVCdmadone());
				nif_next_state = CNIF_IDLE;
			}
			break;
		case CNIF_DONE:
			if(rtTx->slaveReady(nif_config->getVCdone())) {
				RouterUtils::make_head_flit(&sflit, DONE_NOTIF_ADDR, MTYPE_DONE,
											nif_config->getVCdone(), reg_dst, 0, true);
				rtTx->send(&sflit, nif_config->getVCdone());
				if (dma_after_done_en & (nif_config->getDMAlen() > 0)) {
					nif_next_state = CNIF_DMA_HEAD;
					remain_dma_len = nif_config->getDMAlen();
				} else {
					nif_next_state = CNIF_IDLE;
				}
				done_pending = false;
				dma_after_done_en = false;
			}
			break;
		default: return;
	}

	for (int i = 0; i < VCH_SIZE; i++) {
		iready[i] = nif_next_state == CNIF_IDLE;
	}
	//update status
	if ((nif_next_state == CNIF_IDLE) & done_pending) {
		nif_next_state = CNIF_DONE;
	}
	nif_state = nif_next_state;


}

void CubeAccelerator::step()
{
	//handle data to/from router
	for (int i = 0; i < mem_bandwidth; i++) {
		localRouter->step();
	}

	nif_step();

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
	nif_state = nif_next_state = CNIF_IDLE;

	nif_config->clearReg();

	done_pending = false;
	dma_after_done_en = false;

}

void CubeAccelerator::done_signal(bool dma_enable)
{
	done_pending = true;
	dma_after_done_en = dma_enable;
}