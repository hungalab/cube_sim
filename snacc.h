/*  Headers for SNACC class
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

#ifndef _SNACC_H_
#define _SNACC_H_

#define CORE_NUM 4

#include "accelerator.h"
#include "snacccore.h"
#include "dbuf.h"
#include "snaccAddressMap.h"
#include "snaccmodules.h"

class CubeAccelerator;
class BusConAccelerator;
class SNACCCore;

class SNACC : public CubeAccelerator, public BusConAccelerator {
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
		//for Cube Mode
		SNACC(uint32 node_ID, Router* upperRouter, int core_count_);
		//for Bus Mode
		SNACC(int core_count_);
		~SNACC();
		void setup();
		const char *accelerator_name() { return "SNACC"; }
		void core_step();
		void core_reset();

		//for debuger
		virtual void send_commnad(uint32 cmd, uint32 arg);
		virtual bool isTriggered();
		virtual uint32 get_dbg_data();
		void enable_inst_dump(int core_id);
		void enable_mad_debug(int core_id);
};


#endif //_SNACC_H
