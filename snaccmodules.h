#ifndef _SNACCMODULES_H_
#define _SNACCMODULES_H_

#include "range.h"
#include "dbuf.h"
#include "snaccAddressMap.h"

#include <vector>

#define SNACC_WBUF_ARB_4CORE	0
#define SNACC_WBUF_ARB_2CORE	2
#define SNACC_WBUF_ARB_1CORE	1
#define SNACC_WBUF_LOAD			0
#define SNACC_WBUF_STORE		1

//for mad unit
// state machine is not compatible for the fabricated RTL
#define SNACC_MAD_STAT_IDLE		0
#define SNACC_MAD_STAT_DSTALL	1
#define SNACC_MAD_STAT_EX		2
#define SNACC_MAD_STAT_WB		3
#define SNACC_MAD_STAT_LUTSTALL	4
#define SNACC_MAD_STAT_FU		5


#define SNACC_MAD_MODE_NOLUT	0
#define SNACC_MAD_MODE_LUT		1
#define SNACC_MAD_MODE_POOL		2
#define SNACC_MAD_MODE_AVGPOOL	3 //original

#define SNACC_SIMD_LANE_SIZE	8


namespace SNACCComponents {
	class BCastRange : public Range {
		private:
			int core_count;
			DoubleBuffer **buf_list; //for each core
		public:
			BCastRange(size_t size, int core_count_,
							DoubleBuffer **buf_list_);
			uint32 fetch_word(uint32 offset, int mode,
							DeviceExc *client);
			void store_word(uint32 offset, uint32 data,
							DeviceExc *client);
	};

	class ConfRegCtrl : public Range {
		struct dmainfo_t {
			uint32 upper;
			uint32 lower;
		};
		private:
			int core_count;
			int bitmap_mask;
			int start;
			int done;
			int donemask;
			int data_db_sel;
			int wbuf_db_sel;
			int inst_mux_sel;
			int lut_mux_sel;
			int rbuf_mux_sel;
			int data_query;
			int rbuf_query;
			int data_status;
			int rbuf_status;
			int dma_done_clear;
			int dma_request;
			bool *pending_clr;
			dmainfo_t *dma_info;

			DoubleBuffer **dmem_u;
			DoubleBuffer **dmem_l;
			DoubleBuffer **rbuf_u;
			DoubleBuffer **rbuf_l;
			DoubleBuffer **lut;
			DoubleBuffer **imem;
			DoubleBuffer *wbuf;

			inline bool getFlag(int data, int idx)
			{ return ((data >> idx) & 0x1) != 0; };
			inline void assertFlag(int *data, int idx)
			{ *data = *data | (0x1 << idx); };
			inline void negateFlag(int data, int idx)
			{ data = data & ~(0x1 << idx); }

			void dbuf_switch(DoubleBuffer **dbuf, int bitmap);
		public:
			ConfRegCtrl(int core_count, 
						DoubleBuffer **dmem_u_,
						DoubleBuffer **dmem_l_,
						DoubleBuffer **rbuf_u_,
						DoubleBuffer **rbuf_l_,
						DoubleBuffer **lut_,
						DoubleBuffer **imem_,
						DoubleBuffer *wbuf_);
			~ConfRegCtrl() {};
			uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
			void store_word(uint32 offset, uint32 data, DeviceExc *client);

			bool isStart(int core_idx) { return getFlag(start, core_idx); };
			bool isDoneClr(int core_idx) {
				return pending_clr[core_idx];
			}
			void negateDoneClr(int core_idx) { 
				pending_clr[core_idx] = false;
			}
			void setDone(int core_idx) { assertFlag(&done, core_idx); };
	};

	class WbufArb {
		private:
			int core_size;
			uint8 counter;
		public:
			WbufArb(int core_size);
			bool isAcquired(int core_id, int arb_mode, int access_mode);
			void step() { counter += 1; }
	};

	class Fixed32;

	// <4.12> bits signed fixed point decimal number.
	class Fixed16 {
		public:
			Fixed16() : num_(0) {
		}

		// <4.4> bits signed fixed point decimal number.
		explicit Fixed16(uint8 num)
			: num_(static_cast<int16>(static_cast<uint16>(num) << 8)) {
		}

		// <4.12> bits signed fixed point decimal number.
		explicit Fixed16(uint16 num)
			: num_(static_cast<int16>(num)) {
		}

		// Number exceeding the range of the fixed point is clipped.
		explicit Fixed16(double num) : num_(num * (1 << 12)) {
			// Clip if number overflows.
			if (num > 0 && num_ < 0) {
				num_ = static_cast<int16>(0x7FFF);
			} else if (num < 0 && num_ > 0) {
				num_ = static_cast<int16>(0x8000);
			}
		}

		// Convert to <4.28> bits signed fixed point decimal number.
		uint32 ToRegisterFormat() const {
			uint32 result = static_cast<uint32>(num_);
			result <<= 16;
			return result;
		}

		const Fixed32 ToFixed32() const;

		float ToFloat() const {
			return static_cast<float>(num_) / (1 << 12);
		}

		friend const Fixed32 operator*(const Fixed16 lhs, const Fixed16 rhs);
		friend const Fixed32 operator+(const Fixed32 lhs, const Fixed32 rhs);
		friend const bool operator<(const Fixed16 lhs, const Fixed16 rhs);

		private:
			int16 num_;
	};

uint32 SignedClipMostSignificant4Bits(uint32 before);

// <8.24> bits signed fixed point decimal number, which is
// internal representation of multiply-and-add unit.
	struct Fixed32 {
		public:
			Fixed32() : num_(0) {}

			// Convert from <4.28> bits signed fixed point decimal number,
			// which is representation of TRs and FRs.
			static Fixed32 FromRegisterFormat(uint32 prev) {
				return Fixed32(static_cast<int32>(prev) >> 4);
			}

			// Convert to <4.28> bits signed fixed point decimal number,
			// which is representation of TRs and FRs.
			uint32 ToRegisterFormat() const {
				return SignedClipMostSignificant4Bits(num_);
			}

		// Convert to LUT index format.
		int ToLutIndex() const {
			// <8.24> SignedClipMostSignificant4Bits()
			// -> <4.28>
			// -> <4.7>
			// -> Drop last 1 bit because LUT elements are <16bit fixed> x 1024
			int lut_index =
			static_cast<int>((SignedClipMostSignificant4Bits(num_) >> 22) << 1);
			//assert(0 <= lut_index && lut_index + 1 < 2 * 1024);
			return lut_index;
		}

		friend const Fixed32 operator*(const Fixed16 lhs, const Fixed16 rhs);
		friend const Fixed32 operator+(const Fixed32 lhs, const Fixed32 rhs);
		friend const bool operator<(const Fixed32 lhs, const Fixed32 rhs);
		friend const Fixed32 Fixed16::ToFixed32() const;

		private:
			explicit Fixed32(int32 num)
				: num_(static_cast<int32>(num)) {}

			int32 num_;
	};

	class MadUnit {
		using MemberFuncPtr = void (MadUnit::*)();
		private:
			int state;
			int next_state;
			int mode;
			bool eight_bit_mode;
			int loop_count;
			bool *mask;
			int mask_count;
			int simd_data_width;
			uint32 sram_latency;
			uint32 wait_data_cycle;
			int dmem_step, rbuf_step;
			bool second_lut_done;

			//Sram Modules
			uint32 data_addr, rbuf_addr;
			DoubleBuffer *dmem_u, *dmem_l, *rbuf_u, *rbuf_l, *lut;

			//reg file
			uint32 *TR0, *TR1, *FR0, *FR1;

			Fixed32 tr0_fp, tr1_fp, fr0_fp, fr1_fp;

			bool overDmemBoundary();
			bool overRbufBoundary();
			void updataAddress();

			void loadData(Fixed16 *array);
			void loadWeight(Fixed16 *array);

			void doMad();
			void doMadLut();
			void doMaxPool();
			void doAvgPool();

			Fixed32 applyFU(Fixed32 input);

			static const MemberFuncPtr madModePtr[4];
		public:
			MadUnit(uint32 sram_latency_,
					DoubleBuffer *dmem_u_,
					DoubleBuffer *dmem_l_,
					DoubleBuffer *rbuf_u_,
					DoubleBuffer *rbuf_l_,
					DoubleBuffer *lut_,
					uint32 *TR0_, uint32 *TR1_,
					uint32 *FR0_, uint32 *FR1_);
			void start(uint8 mode_, bool *mask_, int d_step_,
						int r_step_,  bool eight_bit_mode_,
						uint32 loop_count_ = 0);
			void set_addr(uint32 data_addr_, uint32 rbuf_addr_)
			{
				data_addr = data_addr_ & 0xFFFF;
				rbuf_addr = rbuf_addr_ & 0xFFFF;
				wait_data_cycle = sram_latency;
			};
			void step();
			void reset();
			bool running();

	};

}

#endif //_SNACCMODULES_H_
