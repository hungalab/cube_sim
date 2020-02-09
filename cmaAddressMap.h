#ifndef _CMAADDRESSMAP_H_
#define _CMAADDRESSMAP_H_

/* Address map*/
#define CMA_DBANK0_ADDR		0x000000
#define CMA_DBANK1_ADDR		0x001000
#define CMA_IMEM_ADDR		0x002000
#define CMA_CONST_ADDR		0x003000
#define CMA_CTRL_ADDR		0x004000
#define CMA_LD_TABLE_ADDR	0x005000
#define CMA_ST_TABLE_ADDR	0x006000
#define CMA_ALU_RMC_ADDR	0x200000
#define CMA_SE_RMC_ADDR		0x210000
#define CMA_PREG_CONF_ADDR	0x260000
#define CMA_PE_CONF_ADDR	0x280000

/* Address space for each module */
#define CMA_DBANK0_SIZE		0x1000
#define CMA_DBANK1_SIZE		0x1000
#define CMA_IMEM_SIZE		0x400
#define CMA_CONST_SIZE		0x40
#define CMA_LD_TABLE_SIZE	0x100
#define CMA_ST_TABLE_SIZE	0x100
#define CMA_ALU_RMC_SIZE	0x10 //for block trans
#define CMA_SE_RMC_SIZE		0x10 //for block trans
#define CMA_CTRL_SIZE		0x4  //just one word
#define CMA_PREG_CONF_SIZE	0x4  //just one word
#define CMA_PE_CONF_SIZE	0xC000 //PE addr [15:9]

/* word size */
#define CMA_DWORD_MASK 		0x1FFFFFF	//25bit
#define CMA_IWORD_MASK		0xFFFF		//16bit
#define CMA_TABLE_WORD_MASK	0xFFFFFF	//24bit
#define CMA_ALU_RMC_MASK	0x3FFFFFFF	//30bit (8bit row, 12bit col, 10bit conf)
#define CMA_SE_RMC_MASK		0x3FFFFFFF	//30bit (8bit row, 12bit col, 10bit conf)
#define CMA_PE_CONF_MASK	0x000FFFFF	//20bit

/* for controller */
#define CMA_CTRL_DONEDMA_BIT 	0x10
#define CMA_CTRL_DONEDMA_LSB	4
#define CMA_CTRL_RESTART_BIT 	0x08 //not used
#define CMA_CTRL_RESTART_LSB	3
#define CMA_CTRL_BANKSEL_MASK 	0x04
#define CMA_CTRL_BANKSEL_LSB	2
#define CMA_CTRL_RUN_BIT 		0x02
#define CMA_CTRL_RUN_LSB		1

#endif //_CMAADDRESSMAP_H_
