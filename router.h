#ifndef _ROUTER_H_
#define _ROUTER_H_

#include "types.h"

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


struct FLIT_t {
	uint32 ftype;
	uint32 data;
};

class RouterUtils {
public:
	static void make_head_flit(FLIT_t* flit, uint32 addr, uint32 mtype, uint32 src, uint32 vch,
								uint32 dst, bool tail = false);
	static void make_data_flit(FLIT_t* flit, uint32 data, bool tail = false);
	//static FLIT_t make_ack_flit();
};


#endif /* _ROUTER_H_ */