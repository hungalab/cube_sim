/*  Headers for the debugger utilities
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

#ifndef _DEBUGUTILS_H_
#define _DEBUGUTILS_H_

#include "devicemap.h"

#define ACDBGR_SIZE	16 //4 words
#define CMD_OFFSET 0
#define ARG_OFFSET 4
#define SEND_OFFSET 8
#define RPLY_OFFSET 12

//General command format
//|op 8bit|func 8bit|module 8bit|offset 8bit|
#define DBG_CMD_NOP 0
// set trigger: op = 0
#define DBG_CMD_SETTRG_OP 1
//	 func: comparator
// 		==: 0, != 1, >: 2, <: 3, >= 4, <= 5
#define DBG_CMD_COMPEQ	0
#define DBG_CMD_COMPNE	1
#define DBG_CMD_COMPGT	2
#define DBG_CMD_COMPLT	3
#define DBG_CMD_COMPGE	4
#define DBG_CMD_COMPLE	5
// write op = 1
#define DBG_CMD_WRITE_OP 2
// read op = 2
#define DBG_CMD_READ_OP 3


class DebuggerClient {
	protected:
		void __cmd_parser(uint32 cmd, uint8 &op,
			uint8 &func, uint8 &mod, uint8 &offset);
		bool __compare(uint32 a, uint32 b, uint8 comparator);
	public:
		virtual void send_commnad(uint32 cmd, uint32 arg) = 0;
		virtual bool isTriggered() = 0;
		virtual uint32 get_dbg_data() = 0;
};

class AcceleratorDebugger : public DeviceMap {
	public:
		AcceleratorDebugger(DebuggerClient *acdebugger_);
		virtual uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
		virtual uint8 fetch_byte(uint32 offset, DeviceExc *client);
		virtual void store_word(uint32 offset, uint32 data, DeviceExc *client);
		virtual void store_byte(uint32 offset, uint8 data, DeviceExc *client);
		bool isTriggered() { return acdebugger->isTriggered(); };
	private:
		DebuggerClient* acdebugger;
		uint32 cmd, arg;
		uint32 prev_fetch_addr;
		uint32 fetch_buf;
};

#endif //_DEBUGUTILS_H_