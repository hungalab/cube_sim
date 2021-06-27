/*  Implementation of Cool Mega array as an accelerator
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


#include "cma.h"
#include "debugutils.h"

using namespace CMAComponents;

CMA::CMA()
{

}

CMA::CMA(uint32 node_ID, Router* upperRouter)
	: CubeAccelerator(node_ID, upperRouter)
{

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
	pearray = new CCSOTB2::CCSOTB2_PEArray(CMA_PE_ARRAY_HEIGHT,
											CMA_PE_ARRAY_WIDTH);

	// bank memory
	dmem_front = new DoubleBuffer(CMA_DBANK0_SIZE, CMA_DWORD_MASK);
	dmem_back = new DoubleBuffer(CMA_DBANK1_SIZE, CMA_DWORD_MASK);
	dbank = &dmem_front;

	// instruction memory
	imem = new DoubleBuffer(CMA_IMEM_SIZE, CMA_IWORD_MASK);

	// const regs
	const_reg = new ConstRegCtrl(CMA_CONST_SIZE * 2, pearray);

	// Data manipulator
	ld_unit = new LDUnit(CMA_PE_ARRAY_WIDTH, &dbank, pearray);
	ld_tbl = new DManuTableCtrl(CMA_LD_TABLE_SIZE, ld_unit);
	st_unit = new STUnit(CMA_PE_ARRAY_WIDTH, &dbank, pearray);
	st_tbl = new DManuTableCtrl(CMA_ST_TABLE_SIZE, st_unit);

	// configuration
	rmc_alu = new RMCALUConfigCtrl(pearray);
	rmc_se = new RMCSEConfigCtrl(pearray);
	pe_config = new PEConfigCtrl(pearray);
	preg_config = new PREGConfigCtrl(pearray);

	// control register
	ctrl_reg = new ControlReg();

	// Microcontroller
	mc = new MicroController(imem, ld_unit, st_unit, &mc_done);

	//for debugger
	debug_op = DBG_CMD_NOP;
	resp_data = 0;

	// address mapping
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
			ctrl_reg->negateDone();
		}
		if (!mc_done) {
			// execute microcontroller
			mc->step();
			pearray->exec();
			st_unit->step();
		} else if (!done_notif) {
			// finish exeution
			if (ctrl_reg->getBankSel() == 0) {
				dmem_front->buf_switch();
			} else {
				dmem_back->buf_switch();
			}
			imem->buf_switch();
			done_signal(ctrl_reg->getDoneDMA());
			ctrl_reg->assertDone();
			done_notif = true;
		}
	} else {
		mc_done = false;
		mc_working = false;
		mc->reset();
	}
}

void CMA::send_commnad(uint32 cmd, uint32 arg) {
	uint8 func, mod, offset;
	__cmd_parser(cmd, debug_op, func, mod, offset);
	switch (debug_op) {
		case DBG_CMD_SETTRG_OP:
			trgr_arg = arg;
			trgr_cnd = func;
			trgr_mod = mod;
			trgr_offset = offset;
			break;
		case DBG_CMD_WRITE_OP:
			debug_store(mod, offset, arg);
			break;
		case DBG_CMD_READ_OP:
			resp_data = debug_fetch(mod, offset);
			break;
		default:
			debug_op = DBG_CMD_NOP;
	}
}

bool CMA::isTriggered()
{
	uint32 mod_val = debug_fetch(trgr_mod, trgr_offset);
	fprintf(stderr, "mod(%d, %d) %X arg %X\n", trgr_mod, trgr_offset, mod_val, trgr_arg);
	return __compare(mod_val, trgr_arg, trgr_cnd);
}

uint32 CMA::get_dbg_data()
{
	return resp_data;
}

uint32 CMA::debug_fetch(uint8 mod, uint8 offset)
{
	switch (mod) {
		case CMA_DEBUG_MOD_PC:
			return mc->debug_fetch_pc();
		case CMA_DEBUG_MOD_RF:
			return mc->debug_fetch_regfile(offset);
		case CMA_DEBUG_MOD_LR:
			return pearray->debug_fetch_launch(offset);
		case CMA_DEBUG_MOD_GR:
			return pearray->debug_fetch_gather(offset);
		case CMA_DEBUG_MOD_ALU_L:
		case CMA_DEBUG_MOD_ALU_R:
		case CMA_DEBUG_MOD_ALU_O:
			return pearray->debug_fetch_ALU(offset, mod);
		default:
			return 0;
	}
}

void CMA::debug_store(uint8 mod, uint8 offset, uint32 data)
{
	switch (mod) {
		case CMA_DEBUG_MOD_PC:
			mc->debug_store_pc(data);
			break;
		case CMA_DEBUG_MOD_RF:
			mc->debug_store_regfile(offset, data);
			break;
		case CMA_DEBUG_MOD_LR:
			pearray->debug_store_launch(offset, data);
			break;
		case CMA_DEBUG_MOD_GR:
			pearray->debug_store_gather(offset, data);
			break;
		case CMA_DEBUG_MOD_ALU_L:
		case CMA_DEBUG_MOD_ALU_R:
		case CMA_DEBUG_MOD_ALU_O:
			return pearray->debug_store_ALU(offset, mod, data);
			break;
	}
}
