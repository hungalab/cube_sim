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

/*******************************  CubeAccelerator  *******************************/
CubeAccelerator::CubeAccelerator(uint32 node_ID, Router* upperRouter, bool dmac_en_)
	: dmac_en(dmac_en_)
{
	//make router ports
	rtRx = new RouterPortSlave(iready); //receiver
	rtTx = new RouterPortMaster(); //sender
	localRouter = new Router(rtTx, rtRx, upperRouter, node_ID);
	localBus = new LocalMapper();
	core_module = NULL;
	nif_state = nif_next_state = CNIF_IDLE;
	packetMaxSize = (machine->opt->option("dcachebsize")->num / 4);
	mem_bandwidth = machine->opt->option("mem_bandwidth")->num;
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
								fprintf(stderr, "%s: unknown message type(%d) is received\n",
										accelerator_name(), reg_mtype);
							}
					}
				}
			}
			break;
		case CNIF_SR_HEAD:
		case CNIF_BR_HEAD:
			if(rtTx->slaveReady(reg_vch)) {
				nif_next_state = nif_state == CNIF_SR_HEAD ? CNIF_SR_DATA : CNIF_BR_DATA;
				//response of read request; thus write message
				RouterUtils::make_head_flit(&sflit, reg_mema, nif_state == CNIF_BR_HEAD ? MTYPE_BW : MTYPE_SW,
											reg_vch, reg_dst, reg_src);
				rtTx->send(&sflit, reg_vch);
			}
			break;
		case CNIF_SR_DATA:
			RouterUtils::make_data_flit(&sflit, localBus->fetch_word(reg_mema));
			rtTx->send(&sflit, reg_vch);
			nif_next_state = CNIF_IDLE;
			break;
		case CNIF_BR_DATA:
			for (int i = 0; i < mem_bandwidth; i++) {
				read_addr = reg_mema + (packetMaxSize - dcount) * 4;
				RouterUtils::make_data_flit(&sflit, localBus->fetch_word(read_addr));
				rtTx->send(&sflit, reg_vch);
				if (--dcount == 0) {
					nif_next_state = CNIF_IDLE;
					break;
				}
			}
			break;
		case CNIF_SW_DATA:
			if (rtRx->haveData()) {
				rtRx->getData(&flit, &recv_vch);
				localBus->store_word(reg_mema, flit.data);
				nif_next_state = CNIF_IDLE;
			}
			break;
		case CNIF_BW_DATA:
			for (int i = 0; i < mem_bandwidth; i++) {
				if (rtRx->haveData()) {
					rtRx->getData(&flit, &recv_vch);
					write_addr = reg_mema + (packetMaxSize - dcount) * 4;
					localBus->store_word(write_addr, flit.data);
					if (--dcount == 0) {
						nif_next_state = CNIF_IDLE;
						break;
					}
				}
			}
			break;
		default: return;
	}

	for (int i = 0; i < VCH_SIZE; i++) {
		iready[i] = nif_next_state == CNIF_IDLE;
	}
	//update status
	nif_state = nif_next_state;

}

void CubeAccelerator::step()
{
	//handle data to/from router
	for (int i = 0; i < 1; i++) {
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
}