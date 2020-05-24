#include "debugutils.h"
#include "vmips.h"
#include "mapper.h"
#include "cpu.h"
#include <stdio.h>

AcceleratorDebugger::AcceleratorDebugger(DebuggerClient* acdebugger_) :
	acdebugger(acdebugger_), DeviceMap (ACDBGR_SIZE)
{
	cmd = 0;
	arg = 0;
	prev_fetch_addr = 0x3;
}

uint32 AcceleratorDebugger::fetch_word(uint32 offset, int mode, DeviceExc *client)
{

	switch (offset) {
		case RPLY_OFFSET:
			return acdebugger->get_dbg_data();
		case CMD_OFFSET:
			return cmd;
		case ARG_OFFSET:
			return arg;
		case SEND_OFFSET:
			return 0;
		default:
			return 0;
	}
}

uint8 AcceleratorDebugger::fetch_byte(uint32 offset, DeviceExc *client)
{
	if (prev_fetch_addr != (offset & ~0x3)) {
		prev_fetch_addr = offset & ~0x3;
		fetch_buf = fetch_word(prev_fetch_addr, DATALOAD, client);
		fetch_buf = machine->physmem->host_to_mips_word(fetch_buf);
	}

    uint32 byte_offset_in_word = (offset & 0x03);
    if (!machine->cpu->is_bigendian()) {
		byte_offset_in_word = 3 - byte_offset_in_word;
    }
    uint8 rv;
    switch (byte_offset_in_word) {
		case 0: rv = (fetch_buf >> 24) & 0xff; break;
		case 1: rv = (fetch_buf >> 16) & 0xff; break;
		case 2: rv = (fetch_buf >> 8) & 0xff; break;
		case 3: rv = fetch_buf & 0xff; break;
		default: rv = 0;
	}
	return rv;
}

void AcceleratorDebugger::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	fprintf(stderr, "sw %X %x\n", offset, data);
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
		case RPLY_OFFSET:
			//read only
			return;
		default:
			return;
	}
}

void AcceleratorDebugger::store_byte(uint32 offset, uint8 data, DeviceExc *client)
{
	uint32 word_data;
	uint32 store_addr = offset & ~0x3;
	word_data = fetch_word(store_addr, DATALOAD, client);

	word_data = machine->physmem->host_to_mips_word(word_data);

    uint32 byte_offset_in_word = (offset & 0x03);

    if (!machine->cpu->is_bigendian()) {
		byte_offset_in_word = 3 - byte_offset_in_word;
    }
    switch (byte_offset_in_word) {
		case 0:
			word_data = ((data << 24) & 0xff000000) |
							(word_data & ~0xff000000);
			break;
		case 1:
			word_data = ((data << 16) & 0x00ff0000) |
							(word_data & ~0x00ff0000);
			break;
		case 2:
			word_data = ((data << 8) & 0x0000ff00) |
							(word_data & ~0x0000ff00);
			break;
		case 3:
			word_data = (data & 0x000000ff) |
							(word_data & ~0x000000ff);
			break;
		default: return;
    }
	store_word(store_addr, word_data, client);

}

void DebuggerClient::__cmd_parser(uint32 cmd, uint8 &op,
			uint8 &func, uint8 &mod, uint8 &offset)
{
	op = (cmd >> 24) & 0xFF;
	func = (cmd >> 16) & 0xFF;
	mod = (cmd >> 8) & 0xFF;
	offset = cmd & 0xFF;
}

bool DebuggerClient::__compare(uint32 a, uint32 b, uint8 comparator)
{
	switch (comparator) {
		case DBG_CMD_COMPEQ:
			return a == b;
		case DBG_CMD_COMPNE:
			return a != b;
		case DBG_CMD_COMPGT:
			return a > b;
		case DBG_CMD_COMPLT:
			return a < b;
		case DBG_CMD_COMPGE:
			return a >= b;
		case DBG_CMD_COMPLE:
			return a <= b;
		default:
			return false;
	}
}
