#ifndef _CMA_H_
#define _CMA_H_

#include "accelerator.h"
#include "memorymodule.h"
#include "accesstypes.h"
#include "types.h"
#include "cmamodules.h"
#include "cmaAddressMap.h"

class DeviceExc;
class CubeAccelerator;
class MemoryModule;
class LocalMapper;

class CMA : public CubeAccelerator {
private:
	CMAComponents::CMAMemoryModule *dmem_front, *dmem_back, *imem, *const_reg;
	CMAComponents::CMAMemoryModule *ld_tbl, *st_tbl;
	CMAComponents::ConfigController *confCtrl;
	CMAComponents::PEArray *pearray;
	int node;

public:
	CMA(uint32 node_ID, Router* upperRouter);
	~CMA();

	void setup();
	void core_step() {};
	void core_reset() {};
	const char *accelerator_name() { return "CMA"; }

	CMAComponents::ControlReg *ctrl_reg;
};


#endif //_CMA_H_
