#ifndef _SNACCCORE_H_
#define _SNACCCORE_H_

#include "dbuf.h"

#define SNACC_OPCODE_MASK	0xF000
#define SNACC_OPCODE_LSB	12
#define SNACC_OP_DST_MASK	0x0F00
#define SNACC_OP_DST_LSB	8
#define SNACC_OP_SRC_MASK	0x00F0
#define SNACC_OP_SRC_LSB	4
#define SNACC_OP_FUNC_MASK	0x000F
#define SNACC_OP_IMM_MASK	0x00FF

#define SNACC_REG_SIZE		16

//Core finite status machine
#define SNACC_CORE_STAT_IF	0
#define	SNACC_CORE_STAT_ID	1
#define SNACC_CORE_STAT_EX	2
#define SNACC_CORE_STAT_WB	3

#define SNACC_RFILE_SIZE	16
#define SNACC_CTRLREG_SIZE	16

#define SNACC_CORE_OPCODE_BNEQ	4
#define SNACC_CORE_OPCODE_JUMP	5

#define SNACC_REG_PC_INDEX			15
#define SNACC_CREG_ID_INDEX			0
#define SNACC_CREG_MASK_INDEX		1
#define SNACC_CREG_DSTEP_INDEX		2
#define SNACC_CREG_RSTEP_INDEX		3
#define SNACC_CREG_FPPOS_INDEX		4
#define SNACC_CREG_WBUFARB_INDEX	5
#define SNACC_CREG_MODES_INDEX		6
#define SNACC_CREG_MADMODE_MASK		0xF
#define SNACC_CREG_ACCESSMODE_LSB	0x4
#define SNACC_CREG_ACCESSMODE_MASK	0xF0


class SNACCCore {
	using MemberFuncPtr = void (SNACCCore::*)();
	private:
		int core_id;

		//reg files
		uint32 regs[SNACC_RFILE_SIZE];

		// status
		int status;
		bool stall;
		uint32 pc;
		bool halt_issued;
		bool done;

		//Inst fetch
		void if_stage();
		uint32 fetch_instr_x2;
		uint16 fetch_instr;

		//Inst Decode
		void id_stage();
		uint8 dec_opcode, dec_rd, dec_rs, dec_func;
		uint32 dec_imm;
		bool reg_write;

		//execution
		bool isBranch;
		void ex_stage();
		uint32 reg_write_data;

		//write back
		void wb_stage();

		// SRAM modules
		DoubleBuffer *dmem_u, *dmem_l, *rbuf_u, *rbuf_l, *lut, *imem, *wbuf;

		// op decoder
		static uint8 opcode(const uint16 i) {
			return (i & SNACC_OPCODE_MASK) >> SNACC_OPCODE_LSB;
		}
		static uint8 reg_dst(const uint16 i) {
			return (i & SNACC_OP_DST_MASK) >> SNACC_OP_DST_LSB;
		}
		static uint8 reg_src(const uint16 i) {
			return (i & SNACC_OP_SRC_MASK) >> SNACC_OP_SRC_LSB;
		}
		static uint8 func(const uint16 i) {
			return i & SNACC_OP_FUNC_MASK;
		}
		static uint8 imm(const uint16 i) {
			return i & SNACC_OP_IMM_MASK;
		}

		//control regs
		uint8 mad_mode, access_mode, wbuf_arb, simd_mask,
				dmem_step, rbuf_step, fp_pos;

		// Jump tables for instruction functions.
		static const MemberFuncPtr kOpcodeTable[16];
		static const MemberFuncPtr kRTypeArithmeticTable[16];
		static const MemberFuncPtr kRTypeMemoryTable[16];
		static const MemberFuncPtr kRTypeSimdTable[16];

	public:
		SNACCCore(int core_id_,
					DoubleBuffer *dmem_u_,
					DoubleBuffer *dmem_l_,
					DoubleBuffer *rbuf_u_,
					DoubleBuffer *rbuf_l_,
					DoubleBuffer *lut_,
					DoubleBuffer *imem_,
					DoubleBuffer *wbuf_);

		void step();
		void reset();
		bool isDone() { return done; };

	private:
		void Unknown();

		void RTypeMemory();
		void RTypeArithmetic();
		void RTypeSimd();
		void Loadi();
		void Bneq();
		void Jump();
		void Mad();
		void Madlp();
		void Setcr();
		void Addi();
		void Subi();
		void Sll();
		void Srl();
		void Sra();

		// RType 0: Arithmetic instructions
		void Nop();
		void Mov();
		void Add();
		void Sub();
		void Mul();
		void And();
		void Or();
		void Xor();
		void Neg();

		// RType 1: Memory instructions
		void Halt();
		void Loadw();
		void Storew();
		void Loadh();
		void Storeh();
		void Readcr();
		void Dbchange();
		void Dma();

		// RType 2: SIMD instructions
		void Loadv();

};



#endif //_SNACCCORE_H_
