#ifndef _DEBUGUTILS_H_
#define _DEBUGUTILS_H_

#include "devicemap.h"

#define ACDBGR_SIZE	32 //16 words
#define CMD_OFFSET 0
#define ARG_OFFSET 4
#define SEND_OFFSET 8
#define TRG_OFFSET 12
#define RPLY_OFFSET 16


class DebuggerClient {
	public:
		virtual void send_commnad(uint32 cmd, uint32 arg) = 0;
		virtual bool isTriggered() = 0;
		virtual uint32 get_dbg_data() = 0;
};

class AcceleratorDebugger : public DeviceMap {
	public:
		AcceleratorDebugger(DebuggerClient *acdebugger_);
		virtual uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
		virtual void store_word(uint32 offset, uint32 data, DeviceExc *client);
	private:
		DebuggerClient* acdebugger;
		uint32 cmd, arg;
};

#endif //_DEBUGUTILS_H_