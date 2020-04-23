#ifndef _SNACC_H_
#define _SNACC_H_

#define CORE_NUM 4

#include "accelerator.h"
#include "snacccore.h"
#include "dbuf.h"
#include "snaccAddressMap.h"
#include "snaccmodules.h"

class CubeAccelerator;
class SNACCCore;

class SNACC : public CubeAccelerator{
	private:
		int core_count;
		SNACCCore **cores;
		bool *cleared;

		//dmem
		DoubleBuffer **dmem_upper, **dmem_lower;
		DoubleBuffer **rbuf_upper, **rbuf_lower;
		DoubleBuffer **imem; // back mem ignored
		DoubleBuffer **lut;  // back mem ignored
		DoubleBuffer *wbuf; // shared for all cores

		// Conf Regs
		SNACCComponents::ConfRegCtrl *confReg;


	public:
		SNACC(uint32 node_ID, Router* upperRouter, int core_count_);
		~SNACC();
		void setup();
		const char *accelerator_name() { return "SNACC"; }
		void core_step();
		void core_reset();
};


#endif //_SNACC_H
