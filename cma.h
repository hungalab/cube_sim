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

class CMA : public CubeAccelerator {
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

public:
	CMA(uint32 node_ID, Router* upperRouter);
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
