#include "cma.h"

using namespace CMAComponents;

void CMAMemoryModule::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	uint32 *werd;
	/* calculate address */
	werd = ((uint32 *) address) + (offset / 4);
	/* store word */
	*werd = data & mask;
}

ConfigController::ConfigController(LocalMapper *bus)
{
	rmc_alu = new Range (0, CMA_ALU_RMC_SIZE, 0, MEM_READ_WRITE);
	bus->map_at_local_address(rmc_alu, CMA_ALU_RMC_ADDR);
	rmc_se = new Range (0, CMA_SE_RMC_SIZE, 0, MEM_READ_WRITE);
	bus->map_at_local_address(rmc_se, CMA_SE_RMC_ADDR);
	pe_config = new Range (0, CMA_PREG_CONF_SIZE, 0, MEM_READ_WRITE);
	bus->map_at_local_address(pe_config, CMA_PREG_CONF_ADDR);
	preg_config = new Range (0, CMA_PE_CONF_SIZE, 0, MEM_READ_WRITE);
	bus->map_at_local_address(preg_config, CMA_PE_CONF_ADDR);
}

void ControlReg::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	donedma = (data & CMA_CTRL_DONEDMA_BIT) != 0;
	run = (data & CMA_CTRL_RUN_BIT) != 0;
	bank_sel = (data & CMA_CTRL_BANKSEL_MASK) >> CMA_CTRL_BANKSEL_LSB;

	if (run) fprintf(stderr, "CMA run!\n");
}
uint32 ControlReg::fetch_word(uint32 offset, int mode, DeviceExc *client)
{
	return ((donedma ? 1 : 0) << CMA_CTRL_DONEDMA_LSB ) |
			(bank_sel << CMA_CTRL_BANKSEL_LSB) |
			((run ? 1 : 0) << CMA_CTRL_RUN_LSB);
}

CMA::CMA(uint32 node_ID, Router* upperRouter)
	: CubeAccelerator(node_ID, upperRouter)
{
	dmem_front = new CMAMemoryModule(CMA_DBANK0_SIZE, CMA_DWORD_MASK);
	dmem_back = new CMAMemoryModule(CMA_DBANK1_SIZE, CMA_DWORD_MASK);
	imem = new CMAMemoryModule(CMA_IMEM_SIZE, CMA_IWORD_MASK);
	const_reg = new CMAMemoryModule(CMA_CONST_SIZE, CMA_DWORD_MASK);
	ld_tbl = new CMAMemoryModule(CMA_LD_TABLE_SIZE, CMA_TABLE_WORD_MASK);
	st_tbl = new CMAMemoryModule(CMA_ST_TABLE_SIZE, CMA_TABLE_WORD_MASK);
	ctrl_reg = new ControlReg();
}

CMA::~CMA()
{
	delete dmem_front;
	delete dmem_back;
	delete imem;
	delete const_reg;
	delete ld_tbl;
	delete st_tbl;
}

void CMA::setup()
{
	localBus->map_at_local_address(dmem_front, CMA_DBANK0_ADDR);
	localBus->map_at_local_address(dmem_back, CMA_DBANK1_ADDR);
	localBus->map_at_local_address(imem, CMA_IMEM_ADDR);
	localBus->map_at_local_address(const_reg, CMA_CONST_ADDR);
	localBus->map_at_local_address(ld_tbl, CMA_LD_TABLE_ADDR);
	localBus->map_at_local_address(st_tbl, CMA_ST_TABLE_ADDR);
	localBus->map_at_local_address(ctrl_reg, CMA_CTRL_ADDR);
	//confCtrl = new ConfigController(localBus);
	core_module = new CMACore(this, localBus);
}