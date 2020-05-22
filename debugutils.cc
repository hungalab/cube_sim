#include "debugutils.h"

AcceleratorDebugger::AcceleratorDebugger(DebuggerClient* acdebugger_) :
	acdebugger(acdebugger_)
{
	cmd = 0;
	arg = 0;
}

uint32 AcceleratorDebugger::fetch_word(uint32 offset, int mode, DeviceExc *client)
{
	switch (offset) {
		case TRG_OFFSET:
			return acdebugger->isTriggered ? 1 : 0;
		case RPLY_OFFSET:
			return acdebugger->get_dbg_data();
		case CMD_OFFSET:
		case ARG_OFFSET:
		case SEND_OFFSET:
			// write only
			return 0;
		default:
			return 0;
	}
}

uint32 AcceleratorDebugger::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	switch (offset) {
		case CMD_OFFSET:
			cmd = data;
			break;
		case ARG_OFFSET:
			arg = data;
			break;
		case SEND_OFFSET:
			acdebugger->send_commnad(cmd, arg);
			break;
		case TRG_OFFSET:
			break;
		case RPLY_OFFSET:
			//read only
			return;
		default:
			return;
	}
}
