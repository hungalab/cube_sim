/*  Headers for CMA class
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


#ifndef _CMA_H_
#define _CMA_H_

#include "accelerator.h"
#include "accesstypes.h"
#include "types.h"
#include "cmamodules.h"
#include "cmaAddressMap.h"
#include "dbuf.h"


class DeviceExc;
class CubeAccelerator;
class MemoryModule;
class LocalMapper;
class DoubleBuffer;

class CMA : public CubeAccelerator, public BusConAccelerator {
private:
	DoubleBuffer **dbank;
	DoubleBuffer *dmem_front, *dmem_back, *imem;
	CMAComponents::ConstRegCtrl *const_reg;
	CMAComponents::DManuTableCtrl *ld_tbl, *st_tbl;
	CMAComponents::LDUnit *ld_unit;
	CMAComponents::STUnit *st_unit;
	CMAComponents::PEArray *pearray;
	CMAComponents::RMCALUConfigCtrl *rmc_alu;
	CMAComponents::RMCSEConfigCtrl *rmc_se;
	CMAComponents::PEConfigCtrl *pe_config;
	CMAComponents::PREGConfigCtrl *preg_config;
	CMAComponents::MicroController *mc;

	//status
	bool mc_done;
	bool mc_working;
	bool done_notif;

	//for debug
	uint8 trgr_cnd, trgr_mod, trgr_offset;
	uint8 debug_op;
	uint32 trgr_arg;
	uint32 resp_data;
	uint32 debug_fetch(uint8 mod, uint8 offset);
	void debug_store(uint8 mod, uint8 offset, uint32 data);

public:
	// for cube mode
	CMA(uint32 node_ID, Router* upperRouter);
	// for bus conn mode
	CMA();
	~CMA();

	void setup();
	void core_step();
	void core_reset();

	const char *accelerator_name() { return "CMA"; }

	CMAComponents::ControlReg *ctrl_reg;

	//for debuger
	virtual void send_commnad(uint32 cmd, uint32 arg);
	virtual bool isTriggered();
	virtual uint32 get_dbg_data();
};


#endif //_CMA_H_
