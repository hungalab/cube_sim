#include "router.h"
#include <stdio.h>

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