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

typedef std::queue<FLIT_t> FBUFFER;

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
	RouterPortSlave() : readyStat(NULL) {};
	~RouterPortSlave() {};

	void clearBuf();
	void setReadyStat(bool *readyStat_) { readyStat = readyStat_; };

	//used by master
	bool isReady(uint32 vch);
	void pushData(FLIT_t *flit, uint32 vch);

	//used by router/core
	bool haveData() { return !buf.empty(); } ;
	void getData(FLIT_t *flit, uint32 *vch = NULL);

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




class OutputChannel {
private:
	bool *readyStat;
	RouterPortMaster *oport;
public:
	OutputChannel(RouterPortMaster *oport_, bool *readyStat_) : readyStat(readyStat_), oport(oport_) {};
	~OutputChannel() {};

	void reset();
	void step();
};

class Crossbar {
private:
	OutputChannel *ocLocal, *ocUpper, *ocLower;
public:
	Crossbar(OutputChannel *ocLocal_, OutputChannel *ocUpper_, OutputChannel *ocLower_) :
		ocLocal(ocLocal_), ocUpper(ocUpper_), ocLower(ocLower_) {};
	~Crossbar() {};

	void reset();
	void step();
};

class InputChannel {
private:
	FBUFFER ibuf[VCH_SIZE];
	RouterPortSlave *iport;
	Crossbar *cb;
	int *xpos;
public:
	//constructor
	InputChannel(RouterPortSlave* iport_, Crossbar *cb, int *xpos_) : iport(iport_), xpos(xpos_) {};
	~InputChannel() {};

	void pushData(FLIT_t *flit, uint32 vch);
	void reset();
	void step();
};



class Router {
private:
	int myid;

	//router modules
	InputChannel *icLocal, *icUpper, *icLower;
	OutputChannel *ocLocal, *ocUpper, *ocLower;
	Crossbar *cb;

	//signals between modules
	bool ocLocalRdy[VCH_SIZE], ocUpperRdy[VCH_SIZE], ocLowerRdy[VCH_SIZE];
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

	void setID(int id) { myid = id; };
};


#endif /* _ROUTER_H_ */