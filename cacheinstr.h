#ifndef _CACHEINSTR_H_
#define _CACHEINSTR_H_

/* for cache op */
#define DCACHE_OPCODE_START 0x10

/* function of cache operation */
/* for inst cache */
#define ICACHE_OP_IDX_INV	0x01 //index invalidate
#define ICACHE_OP_HIT_INV	0x09 //hit invalidate
#define ICACHE_OP_IDX_LTAG	0x04 //index load tag
#define ICACHE_OP_IDX_STAG	0x05 //index store tag

/* for data cache */
#define DCACHE_OP_IDX_INV	0x11 //index invalidate
#define DCACHE_OP_IDX_WB		0x12 //index write back
#define DCACHE_OP_IDX_WBINV	0x13 //index write back & invalidate
#define DCACHE_OP_IDX_LTAG	0x14 //index load tag
#define DCACHE_OP_IDX_STAG	0x15 //index store tag
#define DCACHE_OP_IDX_FWB	0x16 //index force write back
#define DCACHE_OP_IDX_FWBINV	0x17 //index force write back & invalidate
#define DCACHE_OP_SETLINE	0x18 //set line
#define DCACHE_OP_HIT_INV	0x19 //hit invalidate
#define DCACHE_OP_HIT_WB		0x1A //hit write back
#define DCACHE_OP_HIT_WBINV	0x1B //hit write back & invalidate
#define DCACHE_OP_CHANGE		0x1C //change
#define DCACHE_OP_RCHANGE	0x1D //reverse change
#define DCACHE_OP_HIT_FWB	0x1E //hit force write back
#define DCACHE_OP_HIT_FWBINV	0x1F //hit force write back & invalidate

#endif //_CACHEINSTR_H_