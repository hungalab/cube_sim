#include "cma.h"

using namespace CMAComponents;

CMA::CMA(uint32 node_ID, Router* upperRouter)
	: CubeAccelerator(node_ID, upperRouter)
{
	pearray = new CCSOTB2::CCSOTB2_PEArray(CMA_PE_ARRAY_HEIGHT,
											CMA_PE_ARRAY_WIDTH);

	// bank memory
	dmem_front = new DoubleBuffer(CMA_DBANK0_SIZE, CMA_DWORD_MASK);
	dmem_back = new DoubleBuffer(CMA_DBANK1_SIZE, CMA_DWORD_MASK);
	dbank = &dmem_front;

	imem = new DoubleBuffer(CMA_IMEM_SIZE, CMA_IWORD_MASK);
	const_reg = new ConstRegCtrl(CMA_CONST_SIZE, pearray);

	ctrl_reg = new ControlReg();

	ld_unit = new LDUnit(CMA_PE_ARRAY_WIDTH, &dbank, pearray);
	ld_tbl = new DManuTableCtrl(CMA_LD_TABLE_SIZE, ld_unit);
	st_unit = new STUnit(CMA_PE_ARRAY_WIDTH, &dbank, pearray);
	st_tbl = new DManuTableCtrl(CMA_ST_TABLE_SIZE, st_unit);

	rmc_alu = new RMCALUConfigCtrl(pearray);
	rmc_se = new RMCSEConfigCtrl(pearray);
	pe_config = new PEConfigCtrl(pearray);
	preg_config = new PREGConfigCtrl(pearray);

	mc = new MicroController(imem, ld_unit, st_unit, &mc_done);

	node = node_ID;
}

CMA::~CMA()
{
	delete dmem_front;
	delete dmem_back;
	delete imem;
	delete const_reg;
	delete ld_tbl;
	delete st_tbl;
	delete ld_unit;
	delete st_unit;
	delete rmc_alu;
	delete rmc_se;
	delete pe_config;
	delete preg_config;
}

void CMA::setup()
{
	//confCtrl = new ConfigController(localBus, pearray);
	localBus->map_at_local_address(dmem_front, CMA_DBANK0_ADDR);
	localBus->map_at_local_address(dmem_back, CMA_DBANK1_ADDR);
	localBus->map_at_local_address(imem, CMA_IMEM_ADDR);
	localBus->map_at_local_address(const_reg, CMA_CONST_ADDR);
	localBus->map_at_local_address(ld_tbl, CMA_LD_TABLE_ADDR);
	localBus->map_at_local_address(st_tbl, CMA_ST_TABLE_ADDR);
	localBus->map_at_local_address(ctrl_reg, CMA_CTRL_ADDR);

	//config
	localBus->map_at_local_address(rmc_alu, CMA_ALU_RMC_ADDR);
	localBus->map_at_local_address(rmc_se, CMA_SE_RMC_ADDR);
	localBus->map_at_local_address(pe_config, CMA_PE_CONF_ADDR);
	localBus->map_at_local_address(preg_config, CMA_PREG_CONF_ADDR);

	// localBus->store_word(CMA_CONST_ADDR, (uint32)(-1));
	// localBus->store_word(CMA_DBANK0_ADDR, 0x1234);
	// dmem_front->buf_switch();
	// ld_unit->test();
	// dmem_front->buf_switch();
	// localBus->store_word(CMA_DBANK1_ADDR, 0x5678);
	// dmem_back->buf_switch();
	// dbank = &dmem_back;
	// ld_unit->test();
	// dmem_back->buf_switch();
}

void CMA::core_reset()
{
	mc_working = false;
	mc->reset();
}

void CMA::core_step()
{
	if (ctrl_reg->getRun()) {
		if (!mc_working) {
			fprintf(stderr, "Kick CMA\n");
			// kick microcontroller
			imem->buf_switch();
			if (ctrl_reg->getBankSel() == 0) {
				// use front dmem
				dmem_front->buf_switch();
				dbank = &dmem_front;
			} else {
				// use back dmem
				dmem_back->buf_switch();
				dbank = &dmem_back;
			}
			mc_working = true;
			done_notif = false;
		}
		if (!mc_done) {
			// execute microcontroller
			mc->step();
			pearray->exec();
			st_unit->step();
		} else if (!done_notif) {
			fprintf(stderr, "nortif Done\n");
			// finish exeution
			if (ctrl_reg->getBankSel() == 0) {
				dmem_front->buf_switch();
			} else {
				dmem_back->buf_switch();
			}
			imem->buf_switch();
			done_signal(ctrl_reg->getDoneDMA());
			done_notif = true;
		}
	} else {
		mc_done = false;
		mc_working = false;
		mc->reset();
	}
}