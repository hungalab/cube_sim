#include "router.h"
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
		return false;
	}
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
	std::queue<SlaveBufEntry_t> empty;
	// clear
	std::swap(buf, empty);
}

void RouterPortSlave::pushData(FLIT_t *flit, uint32 vch)
{
	SlaveBufEntry_t data;
	data.flit = *flit;
	data.vch = vch;
	buf.push(data);
}

void RouterPortSlave::getData(FLIT_t *flit, uint32 *vch)
{
	SlaveBufEntry_t data;
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
	ocLocal = new OutputChannel(toLocal, ocLocalRdy);
	ocUpper = new OutputChannel(toLower, ocUpperRdy);
	ocLower = new OutputChannel(toUpper, ocLowerRdy);

	cb = new Crossbar(ocLocal, ocUpper, ocLower);

	//input channels
	icLocal = new InputChannel(fromLocal, cb, &myid);
	icUpper = new InputChannel(fromUpper, cb, &myid);
	icLower = new InputChannel(fromLower, cb, &myid);

	//make connections
	if (upperRouter != NULL) {
		upperRouter->toLower->connect(fromUpper);
		toLower->connect(upperRouter->fromLower);
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
	cb->reset();

	//FIFO enqueue
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
	}
	iport->clearBuf();

}

void InputChannel::step()
{
	FLIT_t flit;
	uint32 recv_vch;
	//handle input data
	if (iport->haveData()) {
		iport->getData(&flit, &recv_vch);
		pushData(&flit, recv_vch);
		fprintf(stderr, "push to ic\n");
	}
}

void InputChannel::pushData(FLIT_t *flit, uint32 vch)
{
	ibuf[vch].push(*flit);
}

/*******************************  OutputChannel  *******************************/
void OutputChannel::reset()
{
	for (int i = 0; i < VCH_SIZE; i++) {
		readyStat[i] = true;
	}
}

void OutputChannel::step()
{

}

/*******************************  Crossbar  *******************************/
void Crossbar::reset()
{

}

void Crossbar::step()
{

}