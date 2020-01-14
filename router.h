#ifndef _ROUTER_H_
#define _ROUTER_H_

#include "types.h"
#include "vmips.h"
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
#define FLIT_DST_MASK		0x3 //0011
#define FLIT_SRC_MSB		2
#define FLIT_SRC_MASK		0xC //1100
#define FLIT_VCH_MSB		4
#define FLIT_VCH_MASK		0x70 //0111_0000
#define FLIT_MT_MSB			7
#define FLIT_MT_MASK		0x380 //11_1000_0000
#define FLIT_MEMA_MSB		10
#define FLIT_ACK_ENTRY		4
#define FLIT_ACK_CNT_BIT	5

#define VCH_SIZE		8
#define ACK_VCH			0 //ack is sent via vch0

//Virtual Channel Status
#define VC_STATE_RC		0
#define VC_STATE_VSA	1
#define VC_STATE_ST		2

#define LOCAL_PORT		0
#define LOWER_PORT		1
#define UPPER_PORT		2

struct FLIT_t {
	uint32 ftype;
	uint32 data;
};

struct FLIT_ENTRY_t
{
	FLIT_t flit;
	uint32 vch;
};

typedef std::queue<FLIT_t> FBUFFER;

class RouterUtils {
public:
	static void make_head_flit(FLIT_t* flit, uint32 addr, uint32 mtype, uint32 vch, uint32 src,
								uint32 dst, bool tail = false);
	static void make_data_flit(FLIT_t* flit, uint32 data, bool tail = false);
	static uint32 extractDst(FLIT_t* flit);
	static void make_ack_flit(FLIT_t *flit, uint32 ftype, int *cnt);
	static void decode_ack(FLIT_t* flit, int *cnt);

	static void decode_headflit(FLIT_t* flit, uint32 *addr, uint32 *mtype, uint32 *vch, uint32 *src,
								uint32 *dst);
};

class RouterPortSlave {
private:
	bool *readyStat;
	std::queue<FLIT_ENTRY_t> buf;
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

};

class RouterPortMaster {
private:
	RouterPortSlave *connectedSlave;
public:
	//Constructor
	RouterPortMaster();
	~RouterPortMaster() {};

	void connect(RouterPortSlave* slave_);
	void send(FLIT_t *flit, uint32 vch);
	bool slaveReady(uint32 vch);

};


class OutputChannel {
private:
	bool *readyStat;
	RouterPortMaster *oport;

	//for register emulation
	std::queue<FLIT_ENTRY_t> obuf;

	//for piggyback (ACK)
	bool ackEnabled;
	FBUFFER iackbuf;
	int send_count[VCH_SIZE]; //cnt
	int ack_count[VCH_SIZE]; //oack
	int bufMaxSize;
	int packetMaxSize;

	bool ackFormer() { return machine->num_cycles % 2 == 0; }
	void ackSend();

public:
	OutputChannel(RouterPortMaster *oport_, bool *readyStat_, bool ackEnabled_ = true);
	~OutputChannel() {};

	void reset();
	void step();
	void pushData(FLIT_t *flit, uint32 vch);
	void pushAck(FLIT_t *flit);
	void ackIncrement(uint32 vch);
	bool ocReady(uint32 vch) { return readyStat[vch]; }

};

class InputChannel;

class Crossbar {
private:
	OutputChannel *ocLocal, *ocUpper, *ocLower;
	InputChannel *icLocal, *icUpper, *icLower;
public:
	Crossbar(OutputChannel *ocLocal_, OutputChannel *ocUpper_, OutputChannel *ocLower_) :
		ocLocal(ocLocal_), ocUpper(ocUpper_), ocLower(ocLower_) {};
	~Crossbar() {};

	void reset();
	void step();

	void send(InputChannel* ic, FLIT_t *flit, uint32 vch, uint32 port);
	void forwardAck(InputChannel* ic, FLIT_t *flit);

	void connectIC(InputChannel *icLocal_, InputChannel *icUpper_, InputChannel *icLower_);
	bool ready(uint32 vch, uint32 port);

};

class InputChannel {
private:
	RouterPortSlave *iport;
	Crossbar *cb;
	int *xpos;

	//for vc
	int vc_state[VCH_SIZE];
	int vc_next_state[VCH_SIZE];

	//for fifo
	FBUFFER ibuf[VCH_SIZE];

	//for rtcomp
	uint32 send_port[VCH_SIZE];
	uint32 rtcomp(uint32 dst) {
		return dst == *xpos ? LOCAL_PORT :
			   dst > *xpos  ? LOWER_PORT : UPPER_PORT;
	}

public:
	//constructor
	InputChannel(RouterPortSlave* iport_, Crossbar *cb_, int *xpos_) : iport(iport_), xpos(xpos_), cb(cb_) {};
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