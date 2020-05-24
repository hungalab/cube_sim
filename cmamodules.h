#ifndef _CMACORE_H_
#define _CMACORE_H_

#include "range.h"
#include "cmaAddressMap.h"
#include "accelerator.h"
#include "dbuf.h"

#include <vector>
#include <queue>
#include <deque>
#include <map>

/*for debug */
#include <string>
//For debug info
#define CMA_DEBUG_MOD_PC		0
#define CMA_DEBUG_MOD_RF		1 //reg file
#define CMA_DEBUG_MOD_LR		2 //launch reg
#define CMA_DEBUG_MOD_GR		3 //gather reg
#define CMA_DEBUG_MOD_ALU_L		4 //alu input left
#define CMA_DEBUG_MOD_ALU_R		5 //alu input right
#define CMA_DEBUG_MOD_ALU_O		6 //alu output

/* word size */
#define CMA_DWORD_MASK 			0x1FFFFFF	//25bit
#define CMA_CARRY_MASK			0x1000000
#define CMA_DATA_MASK			(CMA_DWORD_MASK ^ CMA_CARRY_MASK)
#define CMA_CARYY_LSB			24
#define CMA_CONST_MASK			0x001FFFF	//17bit
#define CMA_IWORD_MASK			0xFFFF		//16bit
#define CMA_TABLE_WORD_MASK		0xFFFFFF	//24bit
#define CMA_ALU_RMC_MASK		0x3FFFFFFF	//30bit (8bit row, 12bit col, 10bit conf)
#define CMA_SE_RMC_MASK			0x3FFFFFFF	//30bit (8bit row, 12bit col, 10bit conf)
#define CMA_PE_CONF_MASK		0x000FFFFF	//20bit

/* for controller */
#define CMA_CTRL_DONEDMA_BIT 	0x10
#define CMA_CTRL_DONEDMA_LSB	4
#define CMA_CTRL_RESTART_BIT 	0x08 //not used
#define CMA_CTRL_RESTART_LSB	3
#define CMA_CTRL_BANKSEL_MASK 	0x04
#define CMA_CTRL_BANKSEL_LSB	2
#define CMA_CTRL_RUN_BIT 		0x02
#define CMA_CTRL_RUN_LSB		1

/* for configuration */
#define CMA_RMC_ROW_BITMAP_LSB	22
#define CMA_RMC_ROW_BITMAP_MASK	0x3FC00000 //8bit [29:22]
#define CMA_RMC_COL_BITMAP_LSB	10
#define CMA_RMC_COL_BITMAP_MASK	0x003FFC00 //12bit [21:10]
#define CMA_RMC_CONFIG_MASK		0x000003FF //10bit [9:0]
#define CMA_OPCODE_MASK			0x3C0
#define CMA_OPCODE_LSB			6
#define CMA_SEL_A_MASK			0x038
#define CMA_SEL_A_LSB			3
#define CMA_SEL_B_MASK			0x007
#define CMA_SE_NORTH_MASK		0x380
#define CMA_SE_NORTH_LSB		7
#define CMA_SE_SOUTH_MASK		0x060
#define CMA_SE_SOUTH_LSB		5
#define CMA_SE_EAST_MASK		0x01C
#define CMA_SE_EAST_LSB			2
#define CMA_SE_WEST_MASK		0x003
#define CMA_ALU_CONFIG_MASK		0x000003FF
#define CMA_SE_CONFIG_MASK		0x000FFC00
#define CMA_SE_CONF_LSB			10

/* for data manipulator */
#define CMA_INTERLEAVE_SIZE		12
#define CMA_TABLE_ELEMENT_MASK	0xF
#define CMA_TABLE_ELEMENT_BITW	0x4

/* PE Array */
#define CMA_PE_ARRAY_WIDTH		12
#define CMA_PE_ARRAY_HEIGHT		8

/* PE */
#define CMA_OP_SIZE				16


/* MC */
#define CMA_MC_REG_SIZE			16
#define CMA_MC_OPCODE_MASK		0xF000
#define	CMA_MC_OPCODE_LSB		12
#define CMA_MC_DST_MASK			0x0F00
#define CMA_MC_DST_LSB			8
#define CMA_MC_SRC_MASK			0x00F0
#define CMA_MC_SRC_LSB			4
#define CMA_MC_FUNC_MASK		0x000F
#define CMA_MC_IMM_MASK			0x00FF
#define CMA_MC_ADDR_MASK		0x0FF0
#define CMA_MC_ADDR_LSB			4
#define CMA_MC_OPCODE_REG		0x0
#define CMA_MC_OPCODE_LDI		0x3
#define CMA_MC_OPCODE_ADDI		0x4
#define CMA_MC_OPCODE_LD_ST_ADD	0x7
#define CMA_MC_OPCODE_BEZ		0x8
#define CMA_MC_OPCODE_BEZD		0x9
#define CMA_MC_OPCODE_BNZ		0xA
#define CMA_MC_OPCODE_BNZD		0xB
#define CMA_MC_OPCODE_SET_LD	0xC
#define CMA_MC_OPCODE_SET_ST	0xD
#define CMA_MC_FUNC_NOP			0x0
#define CMA_MC_FUNC_ADD			0x1
#define CMA_MC_FUNC_SUB			0x2
#define CMA_MC_FUNC_MV			0x3
#define CMA_MC_FUNC_DONE		0xE
#define CMA_MC_FUNC_DELAY		0xF


class LocalMapper;

namespace CMAComponents {
	class PEArray;
	class PENodeBase;
	class DataManipulator;

	using NodeList = std::vector<CMAComponents::PENodeBase*>;
	using ArrayData = std::vector<uint32>;

	class ConstRegCtrl : public Range {
		private:
			PEArray *pearray;
			int mask;
		public:
			ConstRegCtrl(size_t size, PEArray *pearray_) :
				 Range (0, size, 0, MEM_READ_WRITE),
				pearray(pearray_) {};
			void store_word(uint32 offset, uint32 data, DeviceExc *client);
			uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
	};

	class DManuTableCtrl : public Range {
		private:
			DataManipulator *dmanu;
			DoubleBuffer **dbank;
		public:
			DManuTableCtrl(size_t size, DataManipulator *dmanu_) :
					Range (0, size, 0, MEM_READ_WRITE),
					dmanu(dmanu_) {};

			void store_word(uint32 offset, uint32 data, DeviceExc *client);
			uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
	};

	class ConfigCtrl : public Range {
		protected:
			PEArray *pearray;
		public:
			ConfigCtrl(size_t size, PEArray *pearray_) :
				Range(0, size, 0, MEM_WRITE), pearray(pearray_) {};
			~ConfigCtrl() {};
			virtual uint32 fetch_word(uint32 offset, int mode, DeviceExc *client) { return 0; }; //WRITE ONLY
			virtual void store_word(uint32 offset, uint32 data, DeviceExc *client) = 0;
	};

	class RMCALUConfigCtrl : public ConfigCtrl {
		public:
			RMCALUConfigCtrl(PEArray* pearray) :
				ConfigCtrl(CMA_ALU_RMC_SIZE, pearray) {};
			void store_word(uint32 offset, uint32 data, DeviceExc *client);
	};

	class PREGConfigCtrl : public ConfigCtrl {
		public:
			PREGConfigCtrl(PEArray* pearray) :
				ConfigCtrl(CMA_PREG_CONF_SIZE, pearray) {};
			void store_word(uint32 offset, uint32 data, DeviceExc *client);
	};

	class RMCSEConfigCtrl : public ConfigCtrl {
		public:
			RMCSEConfigCtrl(PEArray* pearray) :
				ConfigCtrl(CMA_SE_RMC_SIZE, pearray) {};
			void store_word(uint32 offset, uint32 data, DeviceExc *client);
	};

	class PEConfigCtrl : public ConfigCtrl {
		public:
			PEConfigCtrl(PEArray* pearray) :
				ConfigCtrl(CMA_PE_CONF_SIZE, pearray) {};
			void store_word(uint32 offset, uint32 data, DeviceExc *client);
	};


	class ControlReg : public Range {
		private:
			bool donedma, run;
			int bank_sel;
		public:
			ControlReg() : Range(0, CMA_CTRL_SIZE, 0, MEM_READ_WRITE), donedma(false),
								run(false), bank_sel(false) {}
			bool getDoneDMA() { return donedma; };
			bool getRun() { return run; };
			int getBankSel() { return bank_sel; };

			void store_word(uint32 offset, uint32 data, DeviceExc *client);
			uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
	};

	class DataManipulator {
		private:
			bool *bitmap[CMA_TABLE_ENTRY_SIZE];
			int *table[CMA_TABLE_ENTRY_SIZE];
			int interleave_size;

		protected:
			DoubleBuffer ***dbank;
			PEArray *pearray;

		public:
			DataManipulator(int interleave_size_, DoubleBuffer*** dbank_,
							PEArray *pearray_);
			void setBitmap(int index, int pos, bool flag);
			void setTable(int index, int pos, int data);
			bool getBitmap(int index, int pos);
			int getTable(int index, int pos);
			void toPEArray(uint32 load_addr, int table_index);
			void fromPEArray(uint32 store_addr, int table_index);
	};

	using LDUnit = DataManipulator;

	class STUnit : public DataManipulator {
		private:
			struct st_signal_t
			{
				bool enable;
				uint32 table_sel;
				uint32 store_addr;
			};

			int max_delay;
			std::deque<st_signal_t> late_signal;
			int pending_count;
			int delay;


		public:
			STUnit(int interleave_size_, DoubleBuffer*** dbank_,
					PEArray *pearray_, int max_delay_ = 16);
			void issue(uint32 store_addr, uint32 table_index);
			void step();
			void setDelay(int delay_) { delay = delay_; };
			bool isWorking() { return pending_count > 0; };
	};

	class MicroController {
		private:
			DoubleBuffer *imem;
			LDUnit *ld_unit;
			STUnit *st_unit;
			uint16 regfile[CMA_MC_REG_SIZE];
			uint32 pc;
			bool *done_ptr;
			uint32 launch_addr, launch_incr;
			uint32 gather_addr, gather_incr;

			// op decode
			static uint16 opcode(const uint16 i) {
				return (i & CMA_MC_OPCODE_MASK) >> CMA_MC_OPCODE_LSB;
			}
			static uint8 reg_dst(const uint16 i) {
				return (i & CMA_MC_DST_MASK) >> CMA_MC_DST_LSB;
			}
			static uint8 reg_src(const uint16 i) {
				return (i & CMA_MC_SRC_MASK) >> CMA_MC_SRC_LSB;
			}
			static uint8 func(const uint16 i) {
				return i & CMA_MC_FUNC_MASK;
			}
			static uint8 imm(const uint16 i) {
				return i & CMA_MC_IMM_MASK;
			}
			static int8 s_imm(const uint16 i) {
				return i & CMA_MC_IMM_MASK;
			}
			static uint8 addr(const uint16 i) {
				return (i & CMA_MC_ADDR_MASK) >> CMA_MC_ADDR_LSB;
			}

		public:
			MicroController(DoubleBuffer *imem_, LDUnit* ld_unit_,
							STUnit *st_unit_, bool *done_ptr_)
				: imem(imem_), done_ptr(done_ptr_),
				  ld_unit(ld_unit_), st_unit(st_unit_) {};
			void step();
			void reset();

			uint32 debug_fetch_pc() {return pc; };
			void debug_store_pc(uint32 data) { pc = data; };
			uint32 debug_fetch_regfile(uint32 sel);
			void debug_store_regfile(uint32 sel, uint32 data);
	};

	static std::map <PENodeBase*, std::string> debug_str;

	class PENodeBase {
		protected:
			NodeList predecessors;
			NodeList successors;
			std::queue<uint32> obuf;

		public:
			PENodeBase();
			~PENodeBase();
			void connect(CMAComponents::PENodeBase* pred);
			virtual void exec() = 0;
			virtual bool isTerminal() { return false; };
			virtual bool isUse(CMAComponents::PENodeBase* pred) = 0;
			virtual int in_degree() { return 0; };
			virtual uint32 getData() {
				return obuf.empty() ? 0 : obuf.front();
			};
			virtual void update() { obuf.pop(); };

			virtual NodeList use_successors();

			void debug_push_data(uint32 data);
	};

	class MUX : public PENodeBase {
		private:
			uint32 config_data = 1;
		public:
			void exec();
			bool isUse(CMAComponents::PENodeBase* pred);
			void config(uint32 data);
			int in_degree() { return 1; };
	};

	class ALU : public PENodeBase {
		private:
			uint32 opcode = 0;
		public:
			void exec();
			bool isUse(CMAComponents::PENodeBase* pred);
			void config(uint32 data);
			int in_degree();
	};

	class PREG : public PENodeBase {
		private:
			bool activated = false;
			uint32 latch = 0;
		public:
			void exec();
			bool isUse(CMAComponents::PENodeBase* pred);
			void activate() { activated = true; };
			void deactivate() { activated = false; };
			virtual bool isTerminal() { return activated; };
			int in_degree() { return 1; };
	};

	class ConstReg : public PENodeBase {
		private:
			uint32 const_data = 0;
		public:
			uint32 getData();
			void writeData(uint32 data);
			void exec() {}; //nothing to do
			bool isUse(CMAComponents::PENodeBase* pred) { return false; };
	};

	class MemLoadUnit : public PENodeBase {
		private:
			uint32 latch;
		public:
			MemLoadUnit() { obuf.push(0); };
			void exec() {};
			void store(uint32 data) { obuf.push(data); };
			bool isUse(CMAComponents::PENodeBase* pred) { return false; };
			void update() { if (obuf.size() > 1) obuf.pop(); };
	};

	class MemStoreUnit : public PENodeBase {
		public:
			MemStoreUnit() { obuf.push(0); };
			void exec();
			uint32 load();
			bool isUse(CMAComponents::PENodeBase* pred) { return true; };
			int in_degree() { return 1; }
	};

	class PEArray {
		protected:
			int height, width;
			int se_count, se_channels;
			int preg_channels;

			CMAComponents::ALU ***alus;
			CMAComponents::MUX ****alu_sels;
			CMAComponents::MUX *****channels;
			CMAComponents::ConstReg **cregs;
			CMAComponents::PREG ****pregs;
			CMAComponents::MemLoadUnit **launch_regs;
			CMAComponents::MemStoreUnit **gather_regs;

		private:
			//status
			bool config_changed;

			// for build PE array
			void make_ALUs();
			void make_SEs();
			void make_const_regs();
			void make_pregs();
			void make_memports();
			virtual void make_connection() = 0;

			// for execution
			std::map <PENodeBase*, int> in_degrees;
			std::vector <PENodeBase*> tsorted_list;
			void analyze_dataflow();

			// for configuration
			void decode_alu_conf(uint32 data, uint32& opcode,
									uint32& sel_a, uint32& sel_b);
			void decode_se_conf(uint32 data, uint32& se_north,
									uint32& se_south, uint32& se_east,
									uint32& se_west);

		public:
			PEArray(int height_, int width_, int preg_channels_,
					 int se_count_ = 1, int se_channels_ = 4);
			~PEArray();

			// for configuration
			uint32 load_const(uint32 addr);
			void store_const(uint32 addr, uint32 data);
			void config_PREG(uint32 data);
			void config_SE_RMC(int col_bitmap, int row_bitmap, int data);
			void config_ALU_RMC(int col_bitmap, int row_bitmap, int data);
			void config_PE(int pe_addr, int data);
			void config_PE(int x, int y, int data);

			void launch(ArrayData input_data);
			ArrayData gather();

			void exec();
			void update();

			//for debugger
			uint32 debug_fetch_launch(uint32 col);
			void debug_store_launch(uint32 col, uint32 data);
			uint32 debug_fetch_gather(uint32 col);
			void debug_store_gather(uint32 col, uint32 data);
			uint32 debug_fetch_ALU(uint8 pe_addr, uint8 type);
			void debug_store_ALU(uint8 pe_addr, uint8 type, uint32 data);
	};

	namespace CCSOTB2 {
		static int IN_SE_NORTH	= 0;
		static int IN_SE_SOUTH	= 1;
		static int IN_SE_EAST	= 2;
		static int IN_SE_WEST	= 3;
		static int IN_DL_S		= 4;
		static int IN_DL_SE		= 5;
		static int IN_DL_SW		= 6;

		static int OUT_SE_NORTH	= 0;
		static int OUT_SE_SOUTH	= 1;
		static int OUT_SE_EAST	= 2;
		static int OUT_SE_WEST	= 3;

		// indexed by input channels
		static int OPPOSITE_CHANNEL[4] = {
			OUT_SE_SOUTH, // to IN_SE_NORTH
			OUT_SE_NORTH, // to IN_SE_SOUTH
			OUT_SE_WEST,  // to IN_SE_EAST
			OUT_SE_EAST   // to IN_SE_WEST
		};

		static int CONNECT_COORD[7][2] = {
			{0, 1},  // IN_SE_NORTH
			{0, -1}, // IN_SE_SOUTH
			{1, 0},  // IN_SE_EAST
			{-1, 0}, // IN_SE_WEST
			{0, -1}, // IN_DL_S
			{1, -1}, // IN_DL_SE
			{-1, -1} // IN_DL_SW
		};


		class CCSOTB2_PEArray : public CMAComponents::PEArray {
			private:
				void make_connection();
			public:
				CCSOTB2_PEArray(int height_, int width_,
								 int se_count = 1, int se_channels = 4)
								 : PEArray(height_, width_,
								 		2, se_count, se_channels) {
					make_connection();
				};
		};
	}

	/* ALU Configuration */
	uint32 exec_nop(uint32 inA, uint32 inB);
	uint32 exec_add(uint32 inA, uint32 inB);
	uint32 exec_sub(uint32 inA, uint32 inB);
	uint32 exec_mult(uint32 inA, uint32 inB);
	uint32 exec_sl(uint32 inA, uint32 inB);
	uint32 exec_sr(uint32 inA, uint32 inB);
	uint32 exec_sra(uint32 inA, uint32 inB);
	uint32 exec_sel(uint32 inA, uint32 inB);
	uint32 exec_cat(uint32 inA, uint32 inB);
	uint32 exec_not(uint32 inA, uint32 inB);
	uint32 exec_and(uint32 inA, uint32 inB);
	uint32 exec_or(uint32 inA, uint32 inB);
	uint32 exec_xor(uint32 inA, uint32 inB);
	uint32 exec_eql(uint32 inA, uint32 inB);
	uint32 exec_gt(uint32 inA, uint32 inB);
	uint32 exec_lt(uint32 inA, uint32 inB);

	static uint32 (*PE_op_table[CMA_OP_SIZE])(uint32, uint32) = {
		exec_nop, exec_add, exec_sub, exec_mult,
		exec_sl, exec_sr, exec_sra, exec_sel,
		exec_cat, exec_not, exec_and, exec_or,
		exec_xor, exec_eql, exec_gt, exec_lt
	};

	static uint32 PE_operand_size[CMA_OP_SIZE] = {
		0, 2, 2, 2, 2, 2, 2, 2,
		1, 1, 2, 2, 2, 2, 2, 2
	};

}





#endif //_CMACORE_H_
