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
	start = done = donemask = done_clear = data_db_sel = wbuf_db_sel
	= inst_mux_sel = lut_mux_sel = rbuf_mux_sel = data_query = rbuf_query
	= data_status = rbuf_status = dma_done_clear = dma_request = 0;
	dma_info = new dmainfo_t[core_count];
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
			return done_clear;
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
			done_clear = wdata; break;
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
	return Fixed32((static_cast<int32_t>(num_) << 16) >> 4);
}

const Fixed32 SNACCComponents::operator*(const Fixed16 lhs, const Fixed16 rhs) {
	auto result = Fixed32(static_cast<int32_t>(lhs.num_) *
							static_cast<int32_t>(rhs.num_));

	// Overflow checks.
	if (lhs.num_ > 0 && rhs.num_ > 0 && result.num_ < 0) {
		result.num_ = 0x7FFFFFFF;
	}
	if (lhs.num_ < 0 && rhs.num_ < 0 && result.num_ > 0) {
		result.num_ = 0x80000000;
	}

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
