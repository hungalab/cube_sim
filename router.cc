#include "router.h"
#include "options.h"
#include <stdio.h>

//for debug
#include "vmips.h"

/*******************************  RouterUtils  *******************************/
void RouterUtils::make_head_flit(FLIT_t* flit, uint32 addr, uint32 mtype, uint32 vch,
									uint32 src, uint32 dst, bool tail)
{
	flit->data = (addr << FLIT_MEMA_MSB) + ((mtype << FLIT_MT_MSB) & FLIT_MT_MASK) +
					((src << FLIT_SRC_MSB) & FLIT_SRC_MASK) +
					((vch << FLIT_VCH_MSB) & FLIT_VCH_MASK) +
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
			if (machine->opt->option("dbemsg")->flag) {
				fprintf(stderr, "ACK count overflow!\n");
			}
		}
		data |= (cnt[i] << (FLIT_ACK_CNT_BIT * i)) & ((1 << FLIT_ACK_CNT_BIT) - 1);
	}
	flit->data = data;
}

void RouterUtils::decode_headflit(FLIT_t* flit, uint32 *addr, uint32 *mtype, uint32 *vch,
									uint32 *src, uint32 *dst)
{
	if (flit->ftype == FTYPE_TAIL || flit->ftype == FTYPE_HEADTAIL) {
		*addr = flit->data >> FLIT_MEMA_MSB;
		*mtype = (flit->data & FLIT_MT_MASK) >> FLIT_MT_MSB;
		*vch = (flit->data & FLIT_VCH_MASK) >> FLIT_VCH_MSB;
		*src = (flit->data & FLIT_SRC_MASK) >> FLIT_SRC_MSB;
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
			fprintf(stderr, "Router send data to unconnected port\n");
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
	fromLocal = new RouterPortSlave(ocLocalRdy);
	fromLower = new RouterPortSlave();
	fromUpper = new RouterPortSlave();

	//output ports
	toLocal = new RouterPortMaster();
	toLower = new RouterPortMaster();
	toUpper = new RouterPortMaster();

	//output channels
	ocLocal = new OutputChannel(toLocal, ocLocalRdy, false);
	ocUpper = new OutputChannel(toUpper, ocUpperRdy);
	ocLower = new OutputChannel(toLower, ocLowerRdy);

	cb = new Crossbar(ocLocal, ocUpper, ocLower);

	//input channels
	icLocal = new InputChannel(fromLocal, cb, &myid);
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

	// FLIT_t flit;
	// uint32 recv_vch;

			// //dummy just return
		// uint32 addr, mtype, vch, src, dst;
		// FLIT_t sflit;
		// if (flit.ftype == FTYPE_HEADTAIL || flit.ftype == FTYPE_HEAD) {
		// 	RouterUtils::decode_headflit(&flit, &addr, &mtype, &vch, &src, &dst);
		// 	switch (mtype) {
		// 		case MTYPE_SR:
		// 			RouterUtils::make_head_flit(&sflit, addr, MTYPE_SW, vch, dst, src);
		// 			toLocal->send(&sflit, vch);
		// 			RouterUtils::make_data_flit(&sflit, 0x12345678);
		// 			toLocal->send(&sflit, vch);
		// 			break;
		// 		case MTYPE_BR:
		// 			RouterUtils::make_head_flit(&sflit, addr, MTYPE_BW, vch, dst, src);
		// 			toLocal->send(&sflit, vch);
		// 			for (int i = 0; i < 16; i++) {
		// 				RouterUtils::make_data_flit(&sflit, i + 1, i == 15);
		// 				toLocal->send(&sflit, vch);
		// 			}
		// 			break;
		// 	}
		// }
}

/*******************************  InputChannel  *******************************/
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
}

void InputChannel::step()
{
	FLIT_t flit;
	uint32 recv_vch;
	int i;

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
						} else if (true) {
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
		vc_state[i] = vc_next_state[i];
	}

	//handle input data
	if (iport->haveData()) {
		iport->getData(&flit, &recv_vch);
		pushData(&flit, recv_vch);
	}
}

void InputChannel::pushData(FLIT_t *flit, uint32 vch)
{
	ibuf[vch].push(*flit);
}

/*******************************  OutputChannel  *******************************/
OutputChannel::OutputChannel(RouterPortMaster *oport_, bool *readyStat_, bool ackEnabled_)
	: readyStat(readyStat_), oport(oport_), ackEnabled(ackEnabled_)
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
		send_count[entry.vch]++;
		counter_change = true;
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
	if (counter_change) {
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
/*******************************  Crossbar  *******************************/
void Crossbar::reset()
{
}

void Crossbar::step()
{
}

void Crossbar::connectIC(InputChannel *icLocal_, InputChannel *icUpper_, InputChannel *icLower_)
{
	icLocal = icLocal_;
	icUpper = icUpper_;
	icLower = icLower_;
}

void Crossbar::send(InputChannel* ic, FLIT_t *flit, uint32 vch, uint32 port)
{
	//data transfer
	switch (port) {
		case LOCAL_PORT:
			ocLocal->pushData(flit, vch);
			break;
		case LOWER_PORT:
			ocLower->pushData(flit, vch);
			break;
		case UPPER_PORT:
			ocUpper->pushData(flit, vch);
			break;
		default: abort();
	}
	//ack count increment
	if (ic == icLocal) {
		ocLocal->ackIncrement(vch);
	} else if (ic == icLower) {
		ocLower->ackIncrement(vch);
	} else if (ic == icUpper) {
		ocUpper->ackIncrement(vch);
	} else {
		abort();
	}

}

void Crossbar::forwardAck(InputChannel* ic, FLIT_t *flit)
{
	if (ic == icLocal) {
		ocLocal->pushAck(flit);
	} else if (ic == icLower) {
		ocLower->pushAck(flit);
	} else if (ic == icUpper) {
		ocUpper->pushAck(flit);
	} else {
		abort();
	}
}