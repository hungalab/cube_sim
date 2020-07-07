#ifndef _SNACCCORE_H_
#define _SNACCCORE_H_

#include "dbuf.h"
#include "snaccmodules.h"
#include "snaccAddressMap.h"

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

//Opcode
#define SNACC_CORE_OPCODE_RTYPE0	0
#define SNACC_CORE_OPCODE_RTYPE1	1
#define SNACC_CORE_OPCODE_RTYPE2	2
#define SNACC_CORE_OPCODE_BNEQ		4
#define SNACC_CORE_OPCODE_JUMP		5

//func
#define SNACC_CORE_FUNC_LW			1
#define SNACC_CORE_FUNC_SW			2
#define SNACC_CORE_FUNC_LH			3
#define SNACC_CORE_FUNC_SH			4

#define SNACC_REG_FR0_INDEX			11
#define SNACC_REG_FR1_INDEX			12
#define SNACC_REG_TR0_INDEX			13
#define SNACC_REG_TR1_INDEX			14
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

//Internal address map
#define SNACC_IMEM_INTERNAL_ADDR	0x00000000
#define SNACC_DMEM_INTERNAL_ADDR	0x01000000
#define SNACC_RBUF_INTERNAL_ADDR	0x05000000
#define SNACC_LUT_INTERNAL_ADDR		0x07000000
#define SNACC_WBUF_INTERNAL_ADDR	0x09000000

#define SNACC_CORE_NO_STALL				0x0
#define SNACC_CORE_WBUF_STALL			0x1
#define SNACC_CORE_MAD_STALL			0x2
#define SNACC_CORE_DBCHANGE_STALL		0x3
#define SNACC_CORE_DMA_REQ_STALL		0x4
#define SNACC_CORE_DMA_EX_STALL			0x5


class SNACCCore {
	using MemberFuncPtr = void (SNACCCore::*)();
	private:
		int core_id;
		bool dbg_msg;

		//reg files
		uint32 regs[SNACC_RFILE_SIZE];

		// status
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

		//stall management
		bool isStall();
		void stall_handle();
		int status;
		int stall_cause;

		void disassemble();

		// SRAM modules
		uint32 access_address;
		DoubleBuffer *access_mem;
		DoubleBuffer *dmem_u, *dmem_l, *rbuf_u, *rbuf_l, *lut, *imem, *wbuf;

		//Wbuf arbiter
		SNACCComponents::WbufArb *wbuf_arb;

		// mad unit
		SNACCComponents::MadUnit *mad_unit;

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
		uint8 mad_mode, access_mode, wbuf_arb_mode, fp_pos;
		int dmem_step, rbuf_step;
		bool simd_mask[SNACC_SIMD_LANE_SIZE];

		// Jump tables for instruction functions.
		static const MemberFuncPtr kOpcodeTable[16];
		static const MemberFuncPtr kRTypeArithmeticTable[16];
		static const MemberFuncPtr kRTypeMemoryTable[16];
		static const MemberFuncPtr kRTypeSimdTable[16];

		// for debug
		bool inst_dump;

	public:
		SNACCCore(int core_id_,
					DoubleBuffer *dmem_u_,
					DoubleBuffer *dmem_l_,
					DoubleBuffer *rbuf_u_,
					DoubleBuffer *rbuf_l_,
					DoubleBuffer *lut_,
					DoubleBuffer *imem_,
					DoubleBuffer *wbuf_,
					SNACCComponents::WbufArb *wbuf_arb_);

		void step();
		void reset();
		bool isDone() { return done; };
		void enable_inst_dump() { inst_dump = true; };
		void enable_mad_debug() { mad_unit->enable_debug(); };

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

static const char* InstrFormat[16] = {
		"", "", "", "Loadi r%d, 0x%X",
		"Bneq r%d, 0x%X", "Jump 0x%X",
		"Mad r%d, 0x%X", "Madlp r%d, 0x%X",
		"Setcr cr%d, 0x%X", "Addi r%d, 0x%X",
		"Subi r%d, 0x%X", "Sll r%d, 0x%X",
		"Srl r%d, 0x%X", "Sra r%d, 0x%X",
		"Unknown", "Unknown" };


static const char* RType0InstrFormat[16] = {
		"Nop", "Mov r%d, r%d", "Add r%d, r%d", "Sub r%d, r%d",
		"Mul r%d, r%d", "And r%d, r%d", "Or  r%d, r%d","Xor  r%d, r%d",
		"Neg r%d, r%d", "Unknown", "Unknown", "Unknown",
		"Unknown", "Unknown", "Unknown", "Unknown" };

static const char* RType1InstrFormat[16] = {
		"Halt", "Loadw r%d, r%d", "Storew r%d, r%d", "Loadh r%d, r%d",
		"Storeh  r%d, r%d", "Unknown", "Unknown", "Readcr r$d, cr%d",
		"Unknown", "Unknown",
		"Dbchange %d, %d", "Dma r%d, r%d",
		"Unknown", "Unknown",
		"Unknown", "Unknown" };

static const char* RType2InstrFormat[16] = {
		"Nop", "Loadv  r%d, r%d", "Unknown", "Unknown",
		"Unknown", "Unknown", "Unknown", "Unknown",
		"Unknown", "Unknown", "Unknown", "Unknown",
		"Unknown", "Unknown", "Unknown", "Unknown" };


#endif //_SNACCCORE_H_
