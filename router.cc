#include "router.h"
#include "options.h"
#include <stdio.h>

//for debug
#include "vmips.h"

/*******************************  RouterUtils  *******************************/
void RouterUtils::make_head_flit(FLIT_t* flit, uint32 addr, uint32 mtype, uint32 vch,
									uint32 src, uint32 dst, bool tail)
{
	flit->data = (addr << FLIT_MEMA_LSB) + ((mtype << FLIT_MT_LSB) & FLIT_MT_MASK) +
					((src << FLIT_SRC_LSB) & FLIT_SRC_MASK) +
					((vch << FLIT_VCH_LSB) & FLIT_VCH_MASK) +
					(dst & FLIT_DST_MASK);
	if (tail) {
		flit->ftype = FTYPE_HEADTAIL;
	} else {
		flit->ftype = FTYPE_HEAD;
	}
}

void RouterUtils::make_data_flit(FLIT_t* flit, uint32 data, bool tail)
{
	flit->data = data;
	if (tail) {
		flit->ftype = FTYPE_TAIL;
	} else {
		flit->ftype = FTYPE_DATA;
	}
}

void RouterUtils::make_ack_flit(FLIT_t *flit, uint32 ftype, int *cnt)
{
	flit->ftype = ftype;
	uint32 data = 0;
	for (int i = 0; i < FLIT_ACK_ENTRY; i++) {
		if (cnt[i] >= 1 << FLIT_ACK_CNT_BIT) {
			if (machine->opt->option("routermsg")->flag) {
				fprintf(stderr, "ACK count overflow!\n");
			}
		}
		data |= (cnt[i] & ((1 << FLIT_ACK_CNT_BIT) - 1)) << (FLIT_ACK_CNT_BIT * i);
	}
	flit->data = data;
}

void RouterUtils::decode_headflit(FLIT_t* flit, uint32 *addr, uint32 *mtype, uint32 *vch,
									uint32 *src, uint32 *dst)
{
	if (flit->ftype == FTYPE_HEAD || flit->ftype == FTYPE_HEADTAIL) {
		*addr = flit->data >> FLIT_MEMA_LSB;
		*mtype = (flit->data & FLIT_MT_MASK) >> FLIT_MT_LSB;
		*vch = (flit->data & FLIT_VCH_MASK) >> FLIT_VCH_LSB;
		*src = (flit->data & FLIT_SRC_MASK) >> FLIT_SRC_LSB;
		*dst = flit->data & FLIT_DST_MASK;
	} else {
		*addr = *mtype = *vch = *src = *dst = 0;
	}
}

void RouterUtils::decode_ack(FLIT_t* flit, int *cnt)
{
	int offset = flit->ftype == FTYPE_ACK2 ? VCH_SIZE / 2 : 0;
	uint32 data = flit->data;
	//init
	for (int i = 0; i < VCH_SIZE; i++) cnt[i] = 0;

	for (int i = 0; i < VCH_SIZE / 2; i++, data = data >> FLIT_ACK_CNT_BIT) {
		cnt[i + offset] = data & ((1 << FLIT_ACK_CNT_BIT) - 1);
	}
}


uint32 RouterUtils::extractDst(FLIT_t* flit)
{
	return flit->data & FLIT_DST_MASK;
}

/*******************************  RouterPortMaster  *******************************/
RouterPortMaster::RouterPortMaster()
{
	connectedSlave = NULL;
}

bool RouterPortMaster::slaveReady(uint32 vch)
{
	if (connectedSlave != NULL) {
		return connectedSlave->isReady(vch);
	} else {
		if (machine->opt->option("dbemsg")->flag) {
			fprintf(stderr, "Router trys to send data to unconnected port\n");
		}
		return true;
	}
}

void RouterPortMaster::connect(RouterPortSlave* slave_)
{
	connectedSlave = slave_;
}

void RouterPortMaster::send(FLIT_t *flit, uint32 vch)
{
	if (connectedSlave != NULL) {
		connectedSlave->pushData(flit, vch);
	}
}

/*******************************  RouterPortSlave  *******************************/
bool RouterPortSlave::isReady(uint32 vch) {
	if (readyStat != NULL) {
		return readyStat[vch];
	} else {
		//if readyStat is not registered, it always returns true
		return true;
	}

}

void RouterPortSlave::clearBuf()
{
	std::queue<FLIT_ENTRY_t> empty;
	// clear
	std::swap(buf, empty);
}

void RouterPortSlave::pushData(FLIT_t *flit, uint32 vch)
{
	FLIT_ENTRY_t data;
	data.flit = *flit;
	data.vch = vch;
	buf.push(data);
}

void RouterPortSlave::getData(FLIT_t *flit, uint32 *vch)
{
	FLIT_ENTRY_t data;
	data = buf.front();
	flit->data = data.flit.data;
	flit->ftype = data.flit.ftype;
	if (vch != NULL) *vch = data.vch;
	buf.pop();
}

/*******************************  Router  *******************************/
Router::Router(RouterPortMaster* localTx, RouterPortSlave* localRx, Router* upperRouter,
				int myid_) : myid(myid_)
{
	//input ports
	fromLocal = new RouterPortSlave(icLocalRdy); //ocLocalRdy is set by input channel
	fromLower = new RouterPortSlave();
	fromUpper = new RouterPortSlave();

	//output ports
	toLocal = new RouterPortMaster();
	toLower = new RouterPortMaster();
	toUpper = new RouterPortMaster();

	//output channels
	ocLocal = new OutputChannel(toLocal, false);
	ocUpper = new OutputChannel(toUpper);
	ocLower = new OutputChannel(toLower);

	cb = new Crossbar(&myid, ocLocal, ocUpper, ocLower);

	//input channels
	icLocal = new InputChannel(fromLocal, cb, &myid, icLocalRdy);
	icUpper = new InputChannel(fromUpper, cb, &myid);
	icLower = new InputChannel(fromLower, cb, &myid);

	cb->connectIC(icLocal, icUpper, icLower);

	//make connections
	if (upperRouter != NULL) {
		upperRouter->toLower->connect(fromUpper);
		toUpper->connect(upperRouter->fromLower);
	}
	toLocal->connect(localRx);
	localTx->connect(fromLocal);

}

void Router::reset()
{

	//input channels
	icLocal->reset();
	icUpper->reset();
	icLower->reset();

	//cb
	cb->reset();

	//output channels
	ocLocal->reset();
	ocUpper->reset();
	ocLower->reset();
}

void Router::step()
{
	// Router Pipeline
	// |FIFO enqueu|->|Routing Computation|->|VC Allocation|->|Output|
	// handle the stages in reverse order

	//oc
	ocLocal->step();
	ocUpper->step();
	ocLower->step();

	//cb
	cb->step();

	// //FIFO enqueue
	icLocal->step();
	icUpper->step();
	icLower->step();

}

/*******************************  InputChannel  *******************************/
InputChannel::InputChannel(RouterPortSlave* iport_, Crossbar *cb_, int *xpos_, bool *ordy_)
	: iport(iport_), xpos(xpos_), cb(cb_), ordy(ordy_)
{
	for (int i = 0; i < VCH_SIZE; i++) {
		vc_last_state_update_time[i] = 0;
	}
	bufMaxSize = machine->vcbufsize;
	packetMaxSize = (machine->opt->option("dcachebsize")->num / 4) + 1;
};

void InputChannel::reset()
{
	// clear
	for (int i = 0; i < VCH_SIZE; i++) {
		FBUFFER empty;
		std::swap(ibuf[i], empty);
		vc_state[i] = VC_STATE_RC;
		vc_next_state[i] = VC_STATE_RC;
	}
	iport->clearBuf();
	if (ordy != NULL) {
		for (int i = 0; i < VCH_SIZE; i++) {
			ordy[i] = true;
		}
	}
}

void InputChannel::step()
{
	FLIT_t flit;
	uint32 recv_vch;
	int i;

	int current_time = machine->num_cycles;

	for (i = 0; i < VCH_SIZE; i++) {
		if (!ibuf[i].empty()) {
			flit = ibuf[i].front();
			if (flit.ftype == FTYPE_ACK1 || flit.ftype == FTYPE_ACK2) {
				cb->forwardAck(this, &flit);
				ibuf[i].pop();
			} else {
				switch (vc_state[i]) {
					//Swtich Traversal
					case VC_STATE_ST:
						ibuf[i].pop();
						cb->send(this, &flit, i, send_port[i]);
						if (flit.ftype == FTYPE_HEADTAIL || flit.ftype == FTYPE_TAIL) {
							vc_next_state[i] = VC_STATE_RC;
							//stop request
						}
						break;
					//Virtual Channel Switch Allocation
					case VC_STATE_VSA:
						if (false) {
							//not granted
						} else if (true & cb->ready(i, send_port[i])) {
							//granted & ready
							vc_next_state[i] = VC_STATE_ST;
						}
						break;
					//Routing Computation
					case VC_STATE_RC:
						if (flit.ftype == FTYPE_HEAD || flit.ftype == FTYPE_HEADTAIL) {
							vc_next_state[i] = VC_STATE_VSA;
							send_port[i] = rtcomp(RouterUtils::extractDst(&flit));
						}
						break;
				}
			}
		}
		if (current_time > vc_last_state_update_time[i]) {
			vc_state[i] = vc_next_state[i];
			vc_last_state_update_time[i] = current_time;
		}

	}

	//handle input data
	if (iport->haveData()) {
		iport->getData(&flit, &recv_vch);
		pushData(&flit, recv_vch);
	}

	//change ordy signal
	if (ordy != NULL) {
		for (int i = 0; i < VCH_SIZE; i++) {
			ordy[i] = bufMaxSize - ibuf[i].size() >= packetMaxSize;
		}
	}
}

void InputChannel::pushData(FLIT_t *flit, uint32 vch)
{
	ibuf[vch].push(*flit);
}

/*******************************  OutputChannel  *******************************/
OutputChannel::OutputChannel(RouterPortMaster *oport_, bool ackEnabled_)
	: oport(oport_), ackEnabled(ackEnabled_)
{
	bufMaxSize = machine->vcbufsize;
	packetMaxSize = (machine->opt->option("dcachebsize")->num / 4) + 1;
};

void OutputChannel::reset()
{
	std::queue<FLIT_ENTRY_t> empty_obuf;
	std::swap(obuf, empty_obuf);
	FBUFFER empty_ackbuf;
	std::swap(iackbuf, empty_ackbuf);

	for (int i = 0; i < VCH_SIZE; i++) {
		readyStat[i] = true;
		send_count[i] = 0;
		ack_count[i] = 0;
	}
}

void OutputChannel::ackSend()
{
	bool ackSendEn = false;
	FLIT_t flit;
	if (ackFormer()) {
		for (int i = 0; i < VCH_SIZE / 2; i++) {
			if (ack_count[i] != 0) {
				ackSendEn = true;
				break;
			}
		}
		if (ackSendEn) {
			RouterUtils::make_ack_flit(&flit, FTYPE_ACK1, ack_count);
			oport->send(&flit, ACK_VCH);
			for (int i = 0; i < VCH_SIZE / 2; i++) ack_count[i] = 0;
		}
	} else {
		for (int i = VCH_SIZE / 2; i < VCH_SIZE; i++) {
			if (ack_count[i] != 0) {
				ackSendEn = true;
				break;
			}
		}
		if (ackSendEn) {
			RouterUtils::make_ack_flit(&flit, FTYPE_ACK2, &ack_count[VCH_SIZE / 2]);
			oport->send(&flit, ACK_VCH);
			for (int i = VCH_SIZE / 2; i < VCH_SIZE; i++) ack_count[i] = 0;
		}
	}

}
void OutputChannel::step()
{
	FLIT_ENTRY_t entry;
	bool counter_change = false;
	int recv_ack[VCH_SIZE];

	//send flit
	if (!obuf.empty()) {
		entry = obuf.front();
		oport->send((FLIT_t*)(&entry.flit), entry.vch);
		obuf.pop();
		if (ackEnabled) {
            send_count[entry.vch]++;
            counter_change = true;
        }
	} else {
		//return ACK packets
		if (ackEnabled) {
			ackSend();
		}
	}

	//handle iack packet
	if (!iackbuf.empty()) {
		FLIT_t flit;
		flit = iackbuf.front();
		iackbuf.pop();
		RouterUtils::decode_ack(&flit, recv_ack);
		for (int i = 0; i < VCH_SIZE; i++) {
			send_count[i] -= recv_ack[i];
		}
		counter_change = true;
	}

	//update ready signal
	if (counter_change & ackEnabled) {
		for (int i = 0; i < VCH_SIZE; i++) {
			if (bufMaxSize - send_count[i] >= packetMaxSize) {
				readyStat[i] = true;
			} else {
				readyStat[i] = false;
			}
		}
	}
}

void OutputChannel::pushData(FLIT_t *flit, uint32 vch)
{
	FLIT_ENTRY_t entry;
	entry.flit.ftype = flit->ftype;
	entry.flit.data = flit->data;
	entry.vch = vch;
	obuf.push(entry);
}

void OutputChannel::pushAck(FLIT_t *flit)
{
	iackbuf.push(*flit);
}

void OutputChannel::ackIncrement(uint32 vch)
{
	ack_count[vch]++;
}

bool OutputChannel::ocReady(uint32 vch)
{
	if (ackEnabled) {
		return readyStat[vch];
	} else {
		return true;//oport->slaveReady(vch);
	}

}
/*******************************  Crossbar  *******************************/
void Crossbar::reset()
{
}

void Crossbar::step()
{
}

Crossbar::Crossbar(int *node_id_, OutputChannel *ocLocal_, OutputChannel *ocUpper_, OutputChannel *ocLower_) :
		node_id(node_id_), ocLocal(ocLocal_), ocUpper(ocUpper_), ocLower(ocLower_)
{
	oc_to_string[ocLocal] = "OutputChannel(to Local)";
	oc_to_string[ocUpper] = "OutputChannel(to Upper)";
	oc_to_string[ocLower] = "OutputChannel(to Lower)";
};

void Crossbar::connectIC(InputChannel *icLocal_, InputChannel *icUpper_, InputChannel *icLower_)
{
	icLocal = icLocal_;
	icUpper = icUpper_;
	icLower = icLower_;
	ic_oc_map[icLocal] = ocLocal;
	ic_oc_map[icUpper] = ocUpper;
	ic_oc_map[icLower] = ocLower;
	//for debug message
	ic_to_string[icLocal] = "InputChannel(from Local)";
	ic_to_string[icUpper] = "InputChannel(from Upper)";
	ic_to_string[icLower] = "InputChannel(from Lower)";
}

void Crossbar::send(InputChannel* ic, FLIT_t *flit, uint32 vch, uint32 port)
{
	OutputChannel* trans_oc;

	//data transfer
	switch (port) {
		case LOCAL_PORT:
			trans_oc = ocLocal;
			break;
		case LOWER_PORT:
			trans_oc = ocLower;
			break;
		case UPPER_PORT:
			trans_oc = ocUpper;
			break;
		default: abort();
	}
	trans_oc->pushData(flit, vch);

	//ack count increment
	try {
		OutputChannel* oc = ic_oc_map.at(ic);
		if (machine->opt->option("routermsg")->flag) {
			fprintf(stderr, "%10d:\tRouter%d\t %s(VC%d) sends flit %X_%08X to %s\n", machine->num_cycles, *node_id,
							ic_to_string[ic], vch, flit->ftype, flit->data, oc_to_string[trans_oc]);
		}
		oc->ackIncrement(vch);
	} catch (std::out_of_range&) {
		fprintf(stderr, "Crossbar received request from unknown InputChannel\n");
		abort();
	}

}

bool Crossbar::ready(uint32 vch, uint32 port)
{
	switch (port) {
		case LOCAL_PORT:
			return ocLocal->ocReady(vch);
			break;
		case LOWER_PORT:
			return ocLower->ocReady(vch);
			break;
		case UPPER_PORT:
			return ocUpper->ocReady(vch);
			break;
		default: abort();
	}
}

void Crossbar::forwardAck(InputChannel* ic, FLIT_t *flit)
{
	try {
		OutputChannel* oc = ic_oc_map.at(ic);
		oc->pushAck(flit);
		if (machine->opt->option("routermsg")->flag) {
			fprintf(stderr, "%10d:\tRouter%d\t forwards ACK flit %X_%08X to %s\n",
							 machine->num_cycles, *node_id, flit->ftype, flit->data, oc_to_string[oc]);
		}
	} catch (std::out_of_range&) {
		fprintf(stderr, "Crossbar forwarded ack flit from unknown InputChannel\n");
		abort();
	}

}