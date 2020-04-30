#include "dmac.h"
#include "accesstypes.h"
#include "vmips.h"
#include "options.h"

DMAC::DMAC(Mapper &m) : bus(&m)
{
	config = new DMACConfig();
	bus->map_at_physical_address(config, DMAC_ADDR_BASE);
}

void DMAC::exception(uint16 excCode, int mode, int coprocno)
{

}

void DMAC::step()
{

}

void DMAC::reset()
{

}

DMACConfig::DMACConfig()
	: Range(0, DMAC_CONF_SIZE, 0, MEM_READ_WRITE)
{
	bool opt_bigendian = machine->opt->option("bigendian")->flag;
	byteswapped = (((opt_bigendian) && (!machine->host_bigendian))
			   || ((!opt_bigendian) && machine->host_bigendian));

	kicked = enabled = int_en = abort = busy = done = addr_err
	= bus_err = end_stat = zero_write = burst = false;
	dma_len = dma_src = dma_dst = success_len = 0;

}


uint32 DMACConfig::fetch_word(uint32 offset, int mode,
							DeviceExc *client)
{
	uint32 ret_data;
	switch (offset) {
		case DMAC_ENABLE_OFFSET:
			ret_data = enabled ? 1 : 0;
			break;
		case DMAC_INTEN_OFFSET:
			ret_data = int_en ? 1 : 0;
			break;
		case DMAC_CTRL_OFFSET:
			ret_data = abort ? DMAC_ABORT_BIT : 0;
			break;
		case DMAC_STATUS_OFFSET:
			ret_data = (busy ? DMAC_BUSY_BIT : 0) |
						(done ? DMAC_DONE_BIT : 0) |
						(addr_err ? DMAC_ADRERR_BIT : 0) |
						(bus_err ? DMAC_BUSERR_BIT : 0) |
						(end_stat ? DMAC_ESTATE_BIT : 0);
			break;
		case DMAC_ATTR_OFFSET:
			ret_data = (zero_write ? DMAC_ZERO_BIT : 0) |
						(burst ? DMAC_BURST_BIT : 0);
			break;
		case DMAC_LEN_OFFSET:
			ret_data = dma_len;
			break;
		case DMAC_SRC_OFFSET:
			ret_data = dma_src;
			break;
		case DMAC_DST_OFFSET:
			ret_data = dma_dst;
			break;
		case DMAC_DONELEN_OFFSET:
			ret_data = success_len;
			break;
		default:
			ret_data = 0;
	}

	return ret_data;
}

uint16 DMACConfig::fetch_halfword(uint32 offset, DeviceExc *client)
{
	uint32 word_data;
	if (byteswapped) {
		//big endian
		if (offset % 4 == 2) {
			word_data = fetch_word(offset & ~0x3, DATALOAD, client);
		} else {
			word_data = 0;
		}
	} else {
		//little endian
		if (offset % 4 == 0) {
			word_data = fetch_word(offset, DATALOAD, client);
		} else {
			word_data = 0;
		}
	}
	return (uint16)(word_data & 0xFFFF);
}

uint8 DMACConfig::fetch_byte(uint32 offset, DeviceExc *client)
{
	uint32 word_data;
	if (byteswapped) {
		//big endian
		if (offset % 4 == 3) {
			word_data = fetch_word(offset & ~0x3, DATALOAD, client);
		} else {
			word_data = 0;
		}
	} else {
		//little endian
		if (offset % 4 == 0) {
			word_data = fetch_word(offset, DATALOAD, client);
		} else {
			word_data = 0;
		}
	}
	return (uint8)(word_data & 0xFF);
}

void DMACConfig::store_word(uint32 offset, uint32 data,
					DeviceExc *client)
{
	switch (offset) {
		case DMAC_ENABLE_OFFSET:
			enabled = (data & DMAC_ENABLED_BIT) != 0;
			break;
		case DMAC_INTEN_OFFSET:
			int_en = (data & DMAC_INTEN_BIT) != 0;
			break;
		case DMAC_CTRL_OFFSET:
			kicked = (data & DMAC_START_BIT) != 0;
			abort = (data & DMAC_ABORT_BIT) != 0;
			break;
		case DMAC_STATUS_OFFSET:
			done = done & ((data & DMAC_DONE_BIT) == 0);
			addr_err = addr_err & ((data & DMAC_ADRERR_BIT) == 0);
			bus_err = bus_err & ((data & DMAC_BUSERR_BIT) == 0);
			end_stat = end_stat & ((data & DMAC_ESTATE_BIT) == 0);
			break;
		case DMAC_ATTR_OFFSET:
			zero_write = (data & DMAC_ZERO_BIT) != 0;
			burst = (data & DMAC_BURST_BIT) != 0;
			break;
		case DMAC_LEN_OFFSET:
			dma_len = data;
			break;
		case DMAC_SRC_OFFSET:
			dma_src = data;
			break;
		case DMAC_DST_OFFSET:
			dma_dst = data;
			break;
		case DMAC_DONELEN_OFFSET:
			success_len = data;
			break;
	}
}

void DMACConfig::store_halfword(uint32 offset, uint16 data,
					DeviceExc *client)
{
	if (byteswapped) {
		//big endian
		if (offset % 4 == 2) {
			store_word(offset & ~0x3, (uint32)data, client);
		}
	} else {
		//little endian
		if (offset % 4 == 0) {
			store_word(offset, (uint32)data, client);
		}
	}
}

void DMACConfig::store_byte(uint32 offset, uint8 data,
					DeviceExc *client)
{
	if (byteswapped) {
		//big endian
		if (offset % 4 == 3) {
			store_word(offset & ~0x3, (uint32)data, client);
		}
	} else {
		//little endian
		if (offset % 4 == 0) {
			store_word(offset, (uint32)data, client);
		}
	}
}
