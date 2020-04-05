#include "cma.h"

using namespace CMAComponents;

CMA::CMA(uint32 node_ID, Router* upperRouter)
	: CubeAccelerator(node_ID, upperRouter)
{
	dmem_front = new CMAMemoryModule(CMA_DBANK0_SIZE, CMA_DWORD_MASK);
	dmem_back = new CMAMemoryModule(CMA_DBANK1_SIZE, CMA_DWORD_MASK);
	imem = new CMAMemoryModule(CMA_IMEM_SIZE, CMA_IWORD_MASK);
	//const_reg = new CMAMemoryModule(CMA_CONST_SIZE, CMA_DWORD_MASK);
	ld_tbl = new CMAMemoryModule(CMA_LD_TABLE_SIZE, CMA_TABLE_WORD_MASK);
	st_tbl = new CMAMemoryModule(CMA_ST_TABLE_SIZE, CMA_TABLE_WORD_MASK);
	ctrl_reg = new ControlReg();
	node = node_ID;
}

CMA::~CMA()
{
	delete dmem_front;
	delete dmem_back;
	delete imem;
	//delete const_reg;
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
	CMACore * core_module = new CMACore(localBus, ctrl_reg, node);
}