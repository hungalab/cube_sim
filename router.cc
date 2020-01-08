#include "router.h"
#include <stdio.h>


/*******************************  RouterUtils  *******************************/
void RouterUtils::make_head_flit(FLIT_t* flit, uint32 addr, uint32 mtype, uint32 vch,
									uint32 src, uint32 dst, bool tail)
{
	// fprintf(stderr, "addr %X\n", addr);
	// fprintf(stderr, "mtype %X\n", mtype);
	// fprintf(stderr, "vch %X\n", vch);
	// fprintf(stderr, "src %X\n", src);
	// fprintf(stderr, "dst %X\n", dst);

	flit->data = (addr << FLIT_MEMA_MSB) + ((mtype << FLIT_MT_MSB) & FLIT_MT_MASK) +
					((src << FLIT_SRC_MSB) & FLIT_SRC_MASK) +
					((vch << FLIT_VCH_MSB) & FLIT_VCH_MASK) +
					(dst & FLIT_DST_MASK);
	if (tail) {
		flit->ftype = FTYPE_HEADTAIL;
	} else {
		flit->ftype = FTYPE_HEAD;
	}
	fprintf(stderr, "gen %01X_%032X\n", flit->ftype, flit->data);
}

void RouterUtils::make_data_flit(FLIT_t* flit, uint32 data, bool tail)
{
	flit->data = data;
	if (tail) {
		flit->ftype = FTYPE_TAIL;
	} else {
		flit->ftype = FTYPE_DATA;
	}
	fprintf(stderr, "gen %01X_%032X\n", flit->ftype, flit->data);
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
	connectedSlave->pushData(flit, vch);
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
		fprintf(stderr, "router step\n");
		fromLocal->getData(&flit, &recv_vch);
		toLocal->send(&flit, recv_vch);
	}
}