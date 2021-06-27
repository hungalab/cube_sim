/*  Macros for decoding cache instruction which is compatible to MIPS 32
    Copyright (c) 2021 Amano laboratory, Keio University.
        Author: Takuya Kojima

    This file is part of CubeSim, a cycle accurate simulator for 3-D stacked system.

    CubeSim is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    CubeSim is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CubeSim.  If not, see <https://www.gnu.org/licenses/>.
*/


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