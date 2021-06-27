/*  Address mapping of local memory space in SNACC
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

//Global Address
#define SNACC_CORE_ADDR_SIZE		0x04000
#define SNACC_GLB_IMEM_OFFSET		0x00000
#define SNACC_GLB_LUT_OFFSET		0x00800
#define SNACC_GLB_DMEM_UPPER_OFFSET	0x01000
#define SNACC_GLB_DMEM_LOWER_OFFSET	0x01800
#define SNACC_GLB_RBUF_UPPER_OFFSET	0x03000
#define SNACC_GLB_RBUF_LOWER_OFFSET	0x03800
#define SNACC_GLB_BCASE_OFFSET		0x10000
#define SNACC_GLB_WBUF_OFFSET		0x14000
#define SNACC_GLB_CONF_REG_OFFSET	0x18000
#define SNACC_GLB_OUTOFRANGE		0x40000

//mem size for a single core
#define SNACC_IMEM_SIZE				0x800
#define SNACC_LUT_SIZE				0x800
#define SNACC_DMEM_SIZE				0x800
#define SNACC_RBUF_SIZE				0x800
#define SNACC_WBUF_SIZE				0x800
#define SNACC_CONF_REG_STAT_SIZE	0x040
#define SNACC_CONF_REG_DMAINFO_SIZE	0x008

//conf reg offset
#define SNACC_CONF_REG_START_OFFSET		0x0000
#define SNACC_CONF_REG_DONE_OFFSET		0x0004
#define SNACC_CONF_REG_DONEMASK_OFFSET	0x0008
#define SNACC_CONF_REG_DONECLR_OFFSET	0x000C
#define SNACC_CONF_REG_DDBSEL_OFFSET	0x0010
#define SNACC_CONF_REG_WDBSEL_OFFSET	0x0014
#define SNACC_CONF_REG_IMUXSEL_OFFSET	0x0018
#define SNACC_CONF_REG_LMUXSEL_OFFSET	0x001C
#define SNACC_CONF_REG_RBMUXSEL_OFFSET	0x0020

#define SNACC_CONF_REG_DQUERY_OFFSET		0x0028 //	read only regs
#define SNACC_CONF_REG_RBQUERY_OFFSET		0x002C //	read only regs
#define SNACC_CONF_REG_DSTAT_OFFSET			0x0030
#define SNACC_CONF_REG_RBSTAT_OFFSET		0x0034
#define SNACC_CONF_REG_DMADONE_CLR_OFFSET	0x0038
#define SNACC_CONF_REG_DMA_REQ_OFFSET		0x003C //	read only regs
// dma setting
#define SNACC_CONF_REG_DMAINFO_OFFSET		0x0040
#define SNACC_DMAINFO_UPPER_OFFSET			0x0
#define SNACC_DMAINFO_LOWER_OFFSET			0x4

//Local Address
