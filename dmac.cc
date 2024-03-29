/*  DMA controller of Geyser CPU
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

#include "dmac.h"
#include "accesstypes.h"
#include "vmips.h"
#include "options.h"
#include "excnames.h"

DMAC::DMAC(Mapper &m) : bus(&m)
{
	config = new DMACConfig();
	bus->map_at_physical_address(config, DMAC_ADDR_BASE);
	block_words = machine->opt->option("dcachebsize")->num >> 2;
	buffer = new uint32[block_words];
	mem_bandwidth = machine->opt->option("mem_bandwidth")->num;
}

void DMAC::exception(uint16 excCode, int mode, int coprocno)
{
	if (excCode == DBE) {
		exception_pending = true;
	}
}

bool DMAC::address_valid(uint32 addr)
{
	uint32 align = query.burst ? block_words * 4 : 4;

	return (addr % align) == 0;

}

void DMAC::step()
{
	uint32 addr;
	if (config->isEnabled()) {
		next_status = status;
		switch (status) {
			case DMAC_STAT_IDLE:
				if (config->isKicked()) {
					config->assertBusy();
					config->negateKicked();
					query = config->getQuery();
					counter = 0;
					word_counter = 0;
					if (query.zero_write) {
						if (address_valid(query.src)) {
							next_status = DMAC_STAT_WRITE_REQ;
						} else {
							next_status = DMAC_STAT_EXIT;
							config->assertAddrErr();
						}
					} else {
						if (address_valid(query.src)) {
							next_status = DMAC_STAT_READ_REQ;
						} else {
							next_status = DMAC_STAT_EXIT;
							config->assertAddrErr();
						}
					}
				}
				break;
			case DMAC_STAT_READ_REQ:
				if (bus->acquire_bus(this)) {
					if (query.burst) {
						addr = query.src +
									block_words * counter * 4;
						for (int i = 0; i < block_words; i++) {
							bus->request_word(addr + 4 * i,
											DATALOAD, this);
						}
					} else {
						bus->request_word(query.src + 4 * counter,
											DATALOAD, this);
					}
					word_counter = 0;
					next_status = DMAC_STAT_READING;
				}
				break;
			case DMAC_STAT_READING:
				for (int i = 0; i < mem_bandwidth; i++) {
					if (query.burst) {
						addr = query.src + block_words * counter * 4 +
									4 * word_counter;
					} else {
						addr = query.src + 4 * counter;
					}
					if (bus->ready(addr, DATALOAD, this)) {
						buffer[word_counter] =
								bus->fetch_word(addr, DATALOAD, this);
						if (++word_counter == block_words ||
								!query.burst) {
							next_status = DMAC_STAT_READ_DONE;
							break;
						}
					}
				}
				break;
			case DMAC_STAT_READ_DONE:
				bus->release_bus(this);
				if (address_valid(query.dst)) {
					config->setIsLastWrite(false);
					next_status = DMAC_STAT_WRITE_REQ;
				} else if (config->isAbort()) {
					next_status = DMAC_STAT_EXIT;
				} else {
					config->setIsLastWrite(true);
					next_status = DMAC_STAT_EXIT;
					config->assertAddrErr();
				}
				break;
			case DMAC_STAT_WRITE_REQ:
				if (bus->acquire_bus(this)) {
					if (query.burst) {
						addr = query.dst +
									block_words * counter * 4;
						for (int i = 0; i < block_words; i++) {
							bus->request_word(addr + 4 * i,
											DATASTORE, this);
						}
					} else {
						bus->request_word(query.dst + 4 * counter,
											DATASTORE, this);
					}
					word_counter = 0;
					next_status = DMAC_STAT_WRITING;
				}
				break;
			case DMAC_STAT_WRITING:
				uint32 data;
				for (int i = 0; i < mem_bandwidth; i++) {
					if (query.zero_write) {
						data = 0;
					} else {
						data = buffer[word_counter];
					}
					if (query.burst) {
						addr = query.dst + block_words * counter * 4 +
									4 * word_counter;
					} else {
						addr = query.dst + 4 * counter;
					}
					if (bus->ready(addr, DATASTORE, this)) {
						bus->store_word(addr, data, this);
						if (++word_counter == block_words || 
								!query.burst) {
							next_status = DMAC_STAT_WRITE_DONE;
							break;
						}
					}
				}
				break;
			case DMAC_STAT_WRITE_DONE:
				if (++counter == query.len) {
					next_status = DMAC_STAT_EXIT;
				} else if (config->isAbort()) {
					next_status = DMAC_STAT_EXIT;
				} else {
					if (query.zero_write) {
						next_status = DMAC_STAT_WRITE_REQ;
					} else {
						next_status = DMAC_STAT_READ_REQ;
					}
				}
				config->setIsLastWrite(true);
				break;
			case DMAC_STAT_EXIT:
				bus->release_bus(this);
				config->assertDone();
				config->negateBusy();
				config->set_success_len(counter);
				next_status = DMAC_STAT_IDLE;
				break;
		}
		if (exception_pending) {
			config->assertBusErr();
			next_status = DMAC_STAT_EXIT;
			exception_pending = false;
		}
		status = next_status;
	}
	if (config->isDone() && config->isIRQEn()) {
		assertInt(IRQ5);
	} else {
		deassertInt(IRQ5);
	}
}

void DMAC::reset()
{
	config->reset();
	status = DMAC_STAT_IDLE;
	exception_pending = false;
}

//to override DeviceInt
const char *DMAC::descriptor_str() const
{
	return "DMA Controller";
}

DMACConfig::DMACConfig()
	: Range(0, DMAC_CONF_SIZE, 0, MEM_READ_WRITE)
{
	bool opt_bigendian = machine->opt->option("bigendian")->flag;
	byteswapped = (((opt_bigendian) && (!machine->host_bigendian))
			   || ((!opt_bigendian) && machine->host_bigendian));
	reset();
}

void DMACConfig::reset()
{
	kicked = enabled = irq_en = abort = busy = done = addr_err
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
			ret_data = irq_en ? 1 : 0;
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
			irq_en = (data & DMAC_INTEN_BIT) != 0;
			break;
		case DMAC_CTRL_OFFSET:
			if (!busy) {
				kicked = (data & DMAC_START_BIT) != 0;
			}
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

DMA_query_t DMACConfig::getQuery()
{
	DMA_query_t q = {dma_src, dma_dst, dma_len,
						zero_write, burst};
	return q;
}