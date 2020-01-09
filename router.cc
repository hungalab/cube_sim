#include "router.h"
#include <stdio.h>


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
	return readyStat[vch];
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
	*vch = data.vch;
	buf.pop();
}
/*******************************  Router  *******************************/
Router::Router(RouterPortMaster* localTx, RouterPortSlave* localRx, Router* upperRouter,
				int myid_) : myid(myid_)
{
	//generate each instance
	fromLocal = new RouterPortSlave(rdyToLocal);
	fromLower = new RouterPortSlave(rdyToLower);
	fromUpper = new RouterPortSlave(rdyToUpper);
	toLocal = new RouterPortMaster();
	toLower = new RouterPortMaster();
	toUpper = new RouterPortMaster();

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
	for (int i = 0; i < VCH_SIZE; i++) {
		rdyToLocal[i] = true;
		rdyToUpper[i] = true;
		rdyToLower[i] = true;
	}
	fromLocal->clearBuf();
	fromLower->clearBuf();
	fromUpper->clearBuf();

}

void Router::step()
{
	//dummy just return
	FLIT_t flit;
	uint32 recv_vch;
	if (fromLocal->haveData()) {
		fromLocal->getData(&flit, &recv_vch);
		uint32 addr, mtype, vch, src, dst;
		FLIT_t sflit;
		if (flit.ftype == FTYPE_HEADTAIL || flit.ftype == FTYPE_HEAD) {
			RouterUtils::decode_headflit(&flit, &addr, &mtype, &vch, &src, &dst);
			switch (mtype) {
				case MTYPE_SR:
					RouterUtils::make_head_flit(&sflit, addr, MTYPE_SW, vch, dst, src);
					toLocal->send(&sflit, vch);
					RouterUtils::make_data_flit(&sflit, 0x12345678);
					toLocal->send(&sflit, vch);
					break;
				case MTYPE_BR:
					RouterUtils::make_head_flit(&sflit, addr, MTYPE_BW, vch, dst, src);
					toLocal->send(&sflit, vch);
					for (int i = 0; i < 16; i++) {
						RouterUtils::make_data_flit(&sflit, i + 1, i == 15);
						toLocal->send(&sflit, vch);
					}
					break;
			}
		}
		//toLocal->send(&flit, recv_vch);
	}
}