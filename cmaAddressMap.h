#ifndef _CMAADDRESSMAP_H_
#define _CMAADDRESSMAP_H_

/* Address map*/
#define CMA_DBANK0_ADDR			0x000000
#define CMA_DBANK1_ADDR			0x001000
#define CMA_IMEM_ADDR			0x002000
#define CMA_CONST_ADDR			0x003000
#define CMA_CTRL_ADDR			0x004000
#define CMA_LD_TABLE_ADDR		0x005000
#define CMA_ST_TABLE_ADDR		0x006000
#define CMA_ALU_RMC_ADDR		0x200000
#define CMA_SE_RMC_ADDR			0x210000
#define CMA_PREG_CONF_ADDR		0x260000
#define CMA_PE_CONF_ADDR		0x280000

/* Address space for each module */
#define CMA_DBANK0_SIZE			0x1000
#define CMA_DBANK1_SIZE			0x1000
#define CMA_IMEM_SIZE			0x400
#define CMA_CONST_SIZE			0x40
#define CMA_LD_TABLE_SIZE		0x100
#define CMA_ST_TABLE_SIZE		0x100
#define CMA_ALU_RMC_SIZE		0x10 //for block trans
#define CMA_SE_RMC_SIZE			0x10 //for block trans
#define CMA_CTRL_SIZE			0x4  //just one word
#define CMA_PREG_CONF_SIZE		0x4  //just one word
#define CMA_PE_CONF_SIZE		0xC000 //PE addr [15:9]
#define CMA_PE_ADDR_LSB			0x9
#define CMA_TABLE_ENTRY_SIZE	0x10
#define CMA_TABLE_SEL_LSB		0x2
#define CMA_TABLE_SEL_MASK		0x0C
#define CMA_TABLE_INDEX_LSB		0x4
#define CMA_TABLE_INDEX_MASK	0xF0
#define CMA_TABLE_FORMAR_OFFSET	0x1
#define CMA_TABLE_LATTER_OFFSET	0x2
#define CMA_TABLE_BITMAP_OFFSET	0x3

#endif //_CMAADDRESSMAP_H_
