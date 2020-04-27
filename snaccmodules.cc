#include "snaccmodules.h"
#include "accesstypes.h"

using namespace SNACCComponents;

BCastRange::BCastRange(size_t size, int core_count_, DoubleBuffer **buf_list_)
	: Range (0, size, 0, MEM_READ_WRITE),
	buf_list(buf_list_), core_count(core_count_) {
}

uint32 BCastRange::fetch_word(uint32 offset, int mode, DeviceExc *client)
{
	return 0;
}

void BCastRange::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	for (int i = 0; i < core_count; i++) {
		buf_list[i]->store_word(offset, data, client);
	}
}


ConfRegCtrl::ConfRegCtrl(int core_count_,
						DoubleBuffer **dmem_u_,
						DoubleBuffer **dmem_l_,
						DoubleBuffer **rbuf_u_,
						DoubleBuffer **rbuf_l_,
						DoubleBuffer **lut_,
						DoubleBuffer **imem_,
						DoubleBuffer *wbuf_) :
	Range(0, SNACC_CONF_REG_STAT_SIZE +
			SNACC_CONF_REG_DMAINFO_SIZE * core_count_, 0, MEM_WRITE),
			core_count(core_count_),
			dmem_u(dmem_u_), dmem_l(dmem_l_), rbuf_u(rbuf_u_),
			rbuf_l(rbuf_l_), lut(lut_), imem(imem_), wbuf(wbuf_)
{
	start = done = donemask =  data_db_sel = wbuf_db_sel
	= inst_mux_sel = lut_mux_sel = rbuf_mux_sel = data_query = rbuf_query
	= data_status = rbuf_status = dma_done_clear = dma_request = 0;
	dma_info = new dmainfo_t[core_count];
	pending_clr = new bool[core_count];
	for (int i = 0; i < core_count; i++) {
		pending_clr[i] = false;
	}
	bitmap_mask = (1 << core_count) - 1;
}

uint32 ConfRegCtrl::fetch_word(uint32 offset, int mode, DeviceExc *client)
{
	switch (offset) {
		case SNACC_CONF_REG_START_OFFSET:
			return start;
		case SNACC_CONF_REG_DONE_OFFSET:
			return done;
		case SNACC_CONF_REG_DONEMASK_OFFSET:
			return donemask;
		case SNACC_CONF_REG_DONECLR_OFFSET:
			return 0;
		case SNACC_CONF_REG_DDBSEL_OFFSET:
			return data_db_sel;
		case SNACC_CONF_REG_WDBSEL_OFFSET:
			return wbuf_db_sel;
		case SNACC_CONF_REG_IMUXSEL_OFFSET:
			return inst_mux_sel;
		case SNACC_CONF_REG_LMUXSEL_OFFSET:
			return lut_mux_sel;
		case SNACC_CONF_REG_RBMUXSEL_OFFSET:
			return rbuf_mux_sel;
		case SNACC_CONF_REG_DQUERY_OFFSET:
			return data_query;
		case SNACC_CONF_REG_RBQUERY_OFFSET:
			return rbuf_query;
		case SNACC_CONF_REG_DSTAT_OFFSET:
			return data_status;
		case SNACC_CONF_REG_RBSTAT_OFFSET:
			return rbuf_status;
		case SNACC_CONF_REG_DMADONE_CLR_OFFSET:
			return dma_done_clear;
		case SNACC_CONF_REG_DMA_REQ_OFFSET:
			return dma_request;
		default:
			uint32 dmainfo_offset = offset - SNACC_CONF_REG_DMAINFO_OFFSET;
			if (dmainfo_offset >= 0 and
				dmainfo_offset < SNACC_CONF_REG_DMAINFO_SIZE * core_count) {
				int core_idx = dmainfo_offset / SNACC_CONF_REG_DMAINFO_SIZE;
				if (dmainfo_offset % SNACC_CONF_REG_DMAINFO_SIZE ==
					SNACC_DMAINFO_UPPER_OFFSET) {
					return dma_info[core_idx].upper;
				} else if (dmainfo_offset % SNACC_CONF_REG_DMAINFO_SIZE ==
					SNACC_DMAINFO_LOWER_OFFSET) {
					return dma_info[core_idx].lower;
				} else {
					return 0;
				}
			} else {
				return 0;
			}
	}
}

void ConfRegCtrl::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	uint32 wdata = data & bitmap_mask;
	switch (offset) {
		case SNACC_CONF_REG_START_OFFSET:
			start = wdata; break;
		case SNACC_CONF_REG_DONE_OFFSET:
			done = wdata; break;
		case SNACC_CONF_REG_DONEMASK_OFFSET:
			donemask = wdata; break;
		case SNACC_CONF_REG_DONECLR_OFFSET:
			for (int i = 0; i < core_count; i++) {
				pending_clr[i] = getFlag(wdata, i);
			}
		case SNACC_CONF_REG_DDBSEL_OFFSET:
			dbuf_switch(dmem_u, data_db_sel ^ wdata);
			dbuf_switch(dmem_l, data_db_sel ^ wdata);
			data_db_sel = wdata; break;
		case SNACC_CONF_REG_WDBSEL_OFFSET:
			if ((wbuf_db_sel ^ wdata) != 0) {
				wbuf->buf_switch();
			}
			wbuf_db_sel = wdata; break;
		case SNACC_CONF_REG_IMUXSEL_OFFSET:
			dbuf_switch(imem, inst_mux_sel ^ wdata);
			inst_mux_sel = wdata; break;
		case SNACC_CONF_REG_LMUXSEL_OFFSET:
			dbuf_switch(lut, lut_mux_sel ^ wdata);
			lut_mux_sel = wdata; break;
		case SNACC_CONF_REG_RBMUXSEL_OFFSET:
			dbuf_switch(rbuf_u, rbuf_mux_sel ^ wdata);
			dbuf_switch(rbuf_l, rbuf_mux_sel ^ wdata);
			rbuf_mux_sel = wdata; break;
		case SNACC_CONF_REG_DSTAT_OFFSET:
			data_status = wdata; break;
		case SNACC_CONF_REG_RBSTAT_OFFSET:
			rbuf_status = wdata; break;
		case SNACC_CONF_REG_DMADONE_CLR_OFFSET:
			dma_done_clear = wdata; break;
		// read only regs
		case SNACC_CONF_REG_DQUERY_OFFSET:
		case SNACC_CONF_REG_RBQUERY_OFFSET:
		case SNACC_CONF_REG_DMA_REQ_OFFSET:
		default:
			break;
	}
}

WbufArb::WbufArb(int core_size_) : core_size(core_size_)
{
	counter = 0;
}

bool WbufArb::isAcquired(int core_id, int arb_mode, int access_mode)
{
	bool load_acquire = (counter & 0x1);
	bool core_acquire;
	if (arb_mode == SNACC_WBUF_ARB_1CORE) {
		return true; //always enabled
	} else {
		if (arb_mode == SNACC_WBUF_ARB_4CORE) {
			core_acquire = core_id == ((counter >> 1) & 0x3);
		} else if (arb_mode == SNACC_WBUF_ARB_2CORE) {
			core_acquire = core_id == ((counter >> 1) & 0x1);
		} else {
			core_acquire = false;
		}
		if (access_mode == SNACC_WBUF_LOAD) {
			return core_acquire & load_acquire;
		} else if (access_mode == SNACC_WBUF_STORE) {
			return core_acquire;
		} else {
			return false;
		}
	}
}

void ConfRegCtrl::dbuf_switch(DoubleBuffer **dbuf, int bitmap)
{
	for (int i = 0; i < core_count; i++) {
		if (getFlag(bitmap, i)) {
			dbuf[i]->buf_switch();
		}
	}
}

const Fixed32 Fixed16::ToFixed32() const {
	// Shift to left 12 bits with sign extenstion.
	return Fixed32((static_cast<int32>(num_) << 16) >> 4);
}

const Fixed32 SNACCComponents::operator*(const Fixed16 lhs, const Fixed16 rhs) {
	auto result = Fixed32(static_cast<int32>(lhs.num_) *
							static_cast<int32>(rhs.num_));

	return result;
}

const Fixed32 SNACCComponents::operator+(const Fixed32 lhs, const Fixed32 rhs) {
	auto result = Fixed32(lhs.num_ + rhs.num_);

	// Overflow checks.
	if (lhs.num_ > 0 && rhs.num_ > 0 && result.num_ < 0) {
		result.num_ = 0x7FFFFFFF;
	}
	if (lhs.num_ < 0 && rhs.num_ < 0 && result.num_ > 0) {
		result.num_ = 0x80000000;
	}

	return result;
}

const bool SNACCComponents::operator<(const Fixed16 lhs, const Fixed16 rhs) {
  return lhs.num_ < rhs.num_;
}

const bool SNACCComponents::operator<(const Fixed32 lhs, const Fixed32 rhs) {
  return lhs.num_ < rhs.num_;
}

uint32 SNACCComponents::SignedClipMostSignificant4Bits(uint32 before) {

	if ((before & 0xF8000000) == 0 ||
			(before & 0xF8000000) == 0xF8000000) {
		// No need to clip.
		return before << 4;
	} else {
		// Value must be clipped if and only if
		// significant 5 bits are neither 11111 nor 00000.

		if (before & 0x80000000) {
			// Negative
			return 0x80000000;
		} else {
			// Positive
			return 0x7FFFFFFF;
		}
	}
}

MadUnit::MadUnit(uint32 sram_latency_, DoubleBuffer *dmem_u_,
				DoubleBuffer *dmem_l_, DoubleBuffer *rbuf_u_,
				DoubleBuffer *rbuf_l_, DoubleBuffer *lut_,
				uint32 *TR0_, uint32 *TR1_, uint32 *FR0_, uint32 *FR1_) :
	sram_latency(sram_latency_),
	dmem_u(dmem_u_), dmem_l(dmem_l_),
	rbuf_u(rbuf_u_), rbuf_l(rbuf_l_), lut(lut_),
	TR0(TR0_), TR1(TR1_), FR0(FR0_), FR1(FR1_)
{

}

void MadUnit::reset()
{
	state = next_state = SNACC_MAD_STAT_IDLE;
}

void MadUnit::start(uint8 mode_, bool *mask_, int d_step_,
						int r_step_, bool eight_bit_mode_,
						 uint32 loop_count_)
{
	if (mode_ >= 4) return;
	mode = mode_;
	eight_bit_mode = eight_bit_mode_;
	mask = mask_;
	loop_count = loop_count_;
	dmem_step = d_step_;
	rbuf_step = r_step_;

	for (mask_count = SNACC_SIMD_LANE_SIZE; mask_count > 0; mask_count--) {
		if (mask[mask_count - 1]) {
			break;
		}
	}
	if (!eight_bit_mode) {
		mask_count %= (SNACC_SIMD_LANE_SIZE) / 2;
		simd_data_width = mask_count * 2;
	} else {
		simd_data_width = mask_count;
	}

	if (wait_data_cycle > 0) {
		next_state = SNACC_MAD_STAT_DSTALL;
	} else {
		//first data is ready
		if (overDmemBoundary() || overRbufBoundary()) {
			next_state = SNACC_MAD_STAT_DSTALL;
			wait_data_cycle = 1;
		} else {
			next_state = SNACC_MAD_STAT_EX;
		}
	}

	tr0_fp = Fixed32::FromRegisterFormat(*TR0);
	tr1_fp = Fixed32::FromRegisterFormat(*TR1);
	fr0_fp = Fixed32::FromRegisterFormat(*FR0);
	fr1_fp = Fixed32::FromRegisterFormat(*FR1);
}

bool MadUnit::overDmemBoundary()
{
	//check if twice data fetching is needed due to over word-boundary
	return ((data_addr % 8) + simd_data_width) > 8;
}

bool MadUnit::overRbufBoundary()
{
	//check if twice data fetching is needed due to over word-boundary
	return ((rbuf_addr % 8) + simd_data_width) > 8;
}

void MadUnit::updataAddress()
{
	data_addr = uint32((int)data_addr + dmem_step);
	rbuf_addr = uint32((int)rbuf_addr + rbuf_step);
}

void MadUnit::step()
{
	switch (state) {
		case SNACC_MAD_STAT_DSTALL:
			if (wait_data_cycle == 0) {
				next_state = SNACC_MAD_STAT_EX;
			}
			break;
		case SNACC_MAD_STAT_LUTSTALL:
			if (wait_data_cycle == 0) {
				next_state = SNACC_MAD_STAT_FU;
			}
			break;
		case SNACC_MAD_STAT_EX:
			(this->*madModePtr[mode])();
			if (--loop_count > 0) {
				updataAddress();
				if (sram_latency > 1) {
					wait_data_cycle = sram_latency - 1;
					if (overDmemBoundary() || overRbufBoundary()) {
						wait_data_cycle++;
					}
					next_state = SNACC_MAD_STAT_DSTALL;
				}
			} else {
				if (mode == SNACC_MAD_MODE_LUT) {
					second_lut_done = false;
					next_state = SNACC_MAD_STAT_LUTSTALL;
					wait_data_cycle = sram_latency;
				} else {
					next_state = SNACC_MAD_STAT_WB;
				}
			}
			break;
		case SNACC_MAD_STAT_FU:
			if (eight_bit_mode && !second_lut_done) {
				wait_data_cycle = sram_latency - 1;
				next_state = SNACC_MAD_STAT_LUTSTALL;
				second_lut_done = true;
				fr0_fp = applyFU(tr0_fp);
			} else {
				if (eight_bit_mode) {
					fr1_fp = applyFU(tr1_fp);
				} else {
					fr0_fp = applyFU(tr0_fp);
				}
				next_state = SNACC_MAD_STAT_WB;
			}
			break;
		case SNACC_MAD_STAT_WB:
			next_state = SNACC_MAD_STAT_IDLE;
			if (eight_bit_mode) {
				*TR1 = tr1_fp.ToRegisterFormat();
				*FR1 = fr1_fp.ToRegisterFormat();
			}
			*TR0 = tr0_fp.ToRegisterFormat();
			*FR0 = fr0_fp.ToRegisterFormat();
			break;
		default:
			break;
	}
	if (wait_data_cycle > 0) {
		wait_data_cycle--;
	}
	state = next_state;
}

Fixed32 MadUnit::applyFU(Fixed32 input)
{
	uint32 lut_addr = input.ToLutIndex();
	Fixed16 lut_val = Fixed16(lut->fetch_half_from_inner(lut_addr ^ 0x3));
	return lut_val.ToFixed32();
}

const MadUnit::MemberFuncPtr MadUnit::madModePtr[4] = {
	&MadUnit::doMad, &MadUnit::doMad,
	&MadUnit::doMaxPool, &MadUnit::doAvgPool
};

void MadUnit::loadWeight(Fixed16 *array)
{
	uint32 addr;
	uint32 access_addr;
	for (int i = 0, addr = rbuf_addr;
			i < SNACC_SIMD_LANE_SIZE/2; i++, addr += 2) {
		access_addr = (((addr >> 3) << 2) + (addr & 0x3)) ^ 0x3;
		if (((addr / 4) % 2) == 0) {
			//upper
			array[i] = Fixed16(rbuf_u->fetch_half_from_inner(access_addr));
		} else {
			//lower
			array[i] = Fixed16(rbuf_l->fetch_half_from_inner(access_addr));
		}
	}
}

void MadUnit::loadData(Fixed16 *array)
{
	uint32 addr;
	uint32 access_addr;
	if (eight_bit_mode) {
		for (int i = 0, addr = data_addr;
			i < SNACC_SIMD_LANE_SIZE; i++, addr++) {
			access_addr = (((addr >> 3) << 2) + (addr & 0x3)) ^ 0x3;
			if (((addr / 4) % 2) == 0) {
				//upper
				array[i] =
					Fixed16(dmem_u->fetch_byte_from_inner(access_addr));
			} else {
				//lower
				array[i] =
					Fixed16(dmem_l->fetch_byte_from_inner(access_addr));
			}
		}
	} else {
		for (int i = 0, addr = data_addr;
			i < SNACC_SIMD_LANE_SIZE/2; i++, addr += 2) {
			access_addr = (((addr >> 3) << 2) + (addr & 0x3)) ^ 0x3;
			if (((addr / 4) % 2) == 0) {
				//upper
				array[i] =
					Fixed16(dmem_u->fetch_half_from_inner(access_addr));
			} else {
				//lower
				array[i] =
					Fixed16(dmem_l->fetch_half_from_inner(access_addr));
			}
		}
	}

}


void MadUnit::doMad()
{
	Fixed16 weight[SNACC_SIMD_LANE_SIZE/2];
	//load weight
	loadWeight(weight);
	if (eight_bit_mode) {
		Fixed16 data[SNACC_SIMD_LANE_SIZE];
		loadData(data);
		for (int i = 0; i < SNACC_SIMD_LANE_SIZE/2; i++) {
			if (mask[i]) {
				tr0_fp = tr0_fp + weight[i] * data[i];
			}
		}
		for (int i = SNACC_SIMD_LANE_SIZE/2;
				i < SNACC_SIMD_LANE_SIZE; i++) {
			if (mask[i]) {
				tr1_fp = tr1_fp + weight[i] * data[i];
			}
		}
	} else {
		Fixed16 data[SNACC_SIMD_LANE_SIZE/2];
		loadData(data);
		for (int i = 0; i < SNACC_SIMD_LANE_SIZE/2; i++) {
			if (mask[i]) {
				tr0_fp = tr0_fp + weight[i] * data[i];
			}
		}
	}
}

void MadUnit::doMaxPool()
{
	if (eight_bit_mode) {
		Fixed16 data[SNACC_SIMD_LANE_SIZE];
		loadData(data);
		for (int i = 0; i < SNACC_SIMD_LANE_SIZE/2; i++) {
			if (mask[i]) {
				tr0_fp = tr0_fp < data[i].ToFixed32() ? data[i].ToFixed32()
								: tr0_fp;
			}
		}
		for (int i = SNACC_SIMD_LANE_SIZE/2;
				i < SNACC_SIMD_LANE_SIZE; i++) {
			if (mask[i]) {
				tr1_fp = tr1_fp < data[i].ToFixed32() ? data[i].ToFixed32()
								: tr1_fp;
			}
		}
	} else {
		Fixed16 data[SNACC_SIMD_LANE_SIZE/2];
		loadData(data);
		for (int i = 0; i < SNACC_SIMD_LANE_SIZE/2; i++) {
			if (mask[i]) {
				tr0_fp = tr0_fp < data[i].ToFixed32() ? data[i].ToFixed32()
								: tr0_fp;
			}
		}
	}
}

void MadUnit::doAvgPool()
{
	fprintf(stderr, "Avg pooling is not implemented\n");
}
bool MadUnit::running()
{
	return (state != SNACC_MAD_STAT_IDLE) ||
			(state == SNACC_MAD_STAT_IDLE
				&& next_state != SNACC_MAD_STAT_IDLE);
}