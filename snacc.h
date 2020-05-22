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

		//mem
		DoubleBuffer **dmem_upper, **dmem_lower;
		DoubleBuffer **rbuf_upper, **rbuf_lower;
		DoubleBuffer **imem; // back mem ignored
		DoubleBuffer **lut;  // back mem ignored
		DoubleBuffer *wbuf; // shared for all cores

		//wrapper for broadcast
		SNACCComponents::BCastRange *bcast_dmem_upper, *bcast_dmem_lower,
					*bcast_rbuf_upper, *bcast_rbuf_lower,
					*bcast_imem, *bcast_lut;

		// wbuf arbiter
		SNACCComponents::WbufArb *wbuf_arb;
		// Conf Regs
		SNACCComponents::ConfRegCtrl *confReg;


	public:
		SNACC(uint32 node_ID, Router* upperRouter, int core_count_);
		~SNACC();
		void setup();
		const char *accelerator_name() { return "SNACC"; }
		void core_step();
		void core_reset();

		//for debuger
		virtual void send_commnad(uint32 cmd, uint32 arg);
		virtual bool isTriggered();
		virtual uint32 get_dbg_data();
};


#endif //_SNACC_H
