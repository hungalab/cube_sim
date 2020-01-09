#ifndef _ROUTER_H_
#define _ROUTER_H_

#include "types.h"
#include <queue>

//Ftype
#define FTYPE_IDLE		0x0
#define FTYPE_HEAD		0x1
#define FTYPE_TAIL		0x2
#define FTYPE_HEADTAIL	0x3
#define FTYPE_DATA		0x4
#define FTYPE_RESERVED	0x5
#define FTYPE_ACK1		0x6
#define FTYPE_ACK2		0x7
//Mtype
#define MTYPE_SW		0x1
#define MTYPE_BW		0x2
#define MTYPE_SR		0x3
#define MTYPE_BR		0x4
#define MTYPE_DONE		0x7

//Router formats
#define FLIT_DST_MASK	0x3 //0011
#define FLIT_SRC_MSB	2
#define FLIT_SRC_MASK	0xC //1100
#define FLIT_VCH_MSB	4
#define FLIT_VCH_MASK	0x70 //0111_0000
#define FLIT_MT_MSB		7
#define FLIT_MT_MASK	0x380 //11_1000_0000
#define FLIT_MEMA_MSB	10

#define VCH_SIZE		8

struct FLIT_t {
	uint32 ftype;
	uint32 data;
};

class RouterUtils {
public:
	static void make_head_flit(FLIT_t* flit, uint32 addr, uint32 mtype, uint32 vch, uint32 src,
								uint32 dst, bool tail = false);
	static void make_data_flit(FLIT_t* flit, uint32 data, bool tail = false);
	//static FLIT_t make_ack_flit();

	static void decode_headflit(FLIT_t* flit, uint32 *addr, uint32 *mtype, uint32 *vch, uint32 *src,
								uint32 *dst);
};

class RouterPortSlave {
private:
	struct SlaveBufEntry_t
	{
		FLIT_t flit;
		uint32 vch;
	};
	bool *readyStat;
	std::queue<SlaveBufEntry_t> buf;
public:
	//Constructor
	RouterPortSlave(bool *readyStat_) : readyStat(readyStat_) {};
	RouterPortSlave() {};
	~RouterPortSlave() {};

	void clearBuf();
	void setReadyStat(bool *readyStat_) { readyStat = readyStat_; };

	//used by master
	bool isReady(uint32 vch);
	void pushData(FLIT_t *flit, uint32 vch);

	//used by router/core
	bool haveData() { return !buf.empty(); } ;
	void getData(FLIT_t *flit, uint32 *vch);

	void debug() { fprintf(stderr, "buf data %lu\n", buf.size()); };
};

class RouterPortMaster {
private:
	RouterPortSlave *connectedSlave;
public:
	//Constructor
	RouterPortMaster();
	~RouterPortMaster() {};

	void connect(RouterPortSlave* slave_) {connectedSlave = slave_; } ;
	void send(FLIT_t *flit, uint32 vch);
	bool slaveReady(uint32 vch);

};

class Router {
private:
	int myid;
public:
	//Constructor
	Router(RouterPortMaster* localTx, RouterPortSlave* localRx,	Router* upperRouter, int myid_ = 0);
	~Router() {};

	// control flow
	void step();
	void reset();

	//Ports
	RouterPortSlave *fromLocal, *fromLower, *fromUpper;
	RouterPortMaster *toLocal, *toLower, *toUpper;
	bool rdyToLocal[VCH_SIZE], rdyToUpper[VCH_SIZE], rdyToLower[VCH_SIZE];

	void setID(int id) { myid = id; };
};


#endif /* _ROUTER_H_ */