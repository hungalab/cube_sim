#include "cmamodules.h"

#define CMA_COUNT 1

using namespace CMAComponents;

// CMACore::CMACore(LocalMapper *bus_, SIGNAL_PTR done_signal_,
// 				ControlReg *ctrl_, int node_)
// 		: AcceleratorCore(bus_, done_signal_), ctrl(ctrl_), node(node_)
// {
// 	pearray = new PEArray(CMA_PE_ARRAY_HEIGHT, CMA_PE_ARRAY_WIDTH);
// };

// CMACore::~CMACore()
// {
// 	delete pearray;
// }

void CMAMemoryModule::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	uint32 *werd;
	/* calculate address */
	werd = ((uint32 *) address) + (offset / 4);
	/* store word */
	*werd = data & mask;
}

ConfigController::ConfigController(LocalMapper *bus)
{
	rmc_alu = new Range (0, CMA_ALU_RMC_SIZE, 0, MEM_READ_WRITE);
	bus->map_at_local_address(rmc_alu, CMA_ALU_RMC_ADDR);
	rmc_se = new Range (0, CMA_SE_RMC_SIZE, 0, MEM_READ_WRITE);
	bus->map_at_local_address(rmc_se, CMA_SE_RMC_ADDR);
	pe_config = new Range (0, CMA_PREG_CONF_SIZE, 0, MEM_READ_WRITE);
	bus->map_at_local_address(pe_config, CMA_PREG_CONF_ADDR);
	preg_config = new Range (0, CMA_PE_CONF_SIZE, 0, MEM_READ_WRITE);
	bus->map_at_local_address(preg_config, CMA_PE_CONF_ADDR);
}

void ControlReg::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	donedma = (data & CMA_CTRL_DONEDMA_BIT) != 0;
	run = (data & CMA_CTRL_RUN_BIT) != 0;
	bank_sel = (data & CMA_CTRL_BANKSEL_MASK) >> CMA_CTRL_BANKSEL_LSB;
}

uint32 ControlReg::fetch_word(uint32 offset, int mode, DeviceExc *client)
{
	return ((donedma ? 1 : 0) << CMA_CTRL_DONEDMA_LSB ) |
			(bank_sel << CMA_CTRL_BANKSEL_LSB) |
			((run ? 1 : 0) << CMA_CTRL_RUN_LSB);
}


PEArray::PEArray(int height_, int width_, uint32 *creg, bool preg_en,
					int se_count, int se_channels) :
	height(height_), width(width_)
{
	make_ALUs();
	make_SEs(se_count, se_channels);
	make_const_regs(creg);
	if (preg_en) {
		make_pregs();
	}
	make_memports();
	fprintf(stderr, "ALU in ");
	alus[0][0]->test();
}

PEArray::~PEArray()
{
	delete [] alus;
	delete [] alu_sels;
	delete [] channels;
	delete [] cregs;
	delete [] pregs;
	delete [] launch_regs;
	delete [] gather_regs;
}


void PEArray::make_ALUs()
{
	alus = new ALU**[width];
	alu_sels = new MUX***[width];
	for (int x = 0; x < width; x++) {
		alus[x] = new ALU*[height];
		alu_sels[x] = new MUX**[height];
		for (int y = 0; y < height; y++) {
			alus[x][y] = new ALU();
			alu_sels[x][y] = new MUX*[2];
			alu_sels[x][y][0] = new MUX();
			alu_sels[x][y][1] = new MUX();
			alus[x][y]->connect(alu_sels[x][y][0]);
			alus[x][y]->connect(alu_sels[x][y][1]);
		}
	}
}

void PEArray::make_SEs(int se_count, int channel_size)
{
	channels = new MUX****[width];
	for (int x = 0; x < width; x++) {
		channels[x] = new MUX***[height];
		for (int y = 0; y < height; y++) {
			channels[x][y] = new MUX**[se_count];
			for (int se = 0; se < se_count; se++) {
				channels[x][y][se] = new MUX*[channel_size];
				for (int ch = 0; ch < channel_size; ch++) {
					channels[x][y][se][ch] = new MUX();
				}
			}
		}
	}
}

void PEArray::make_const_regs(uint32 *creg)
{
	cregs = new ConstReg*[height * 2];
	for (int i = 0; i < height * 2; i++) {
		cregs[i] = new ConstReg(&creg[i]);
	}
}

void PEArray::make_pregs()
{
	pregs = new PREG*[height - 1];
	for (int i = 0; i < height - 1; i++) {
		pregs[i] = new PREG();
	}
}

void PEArray::make_memports()
{
	launch_regs = new MemLoadUnit*[width];
	gather_regs = new MemStoreUnit*[width];
	for (int x = 0; x < width; x++) {
		launch_regs[x] = new MemLoadUnit();
		gather_regs[x] = new MemStoreUnit();
	}
}

void CCSOTB2::CCSOTB2_PEArray::make_connection()
{
	using namespace CCSOTB2;
	PENodeBase* node_ptr;
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			//SE to ALU
			for (int ch = SE_SOUTH; ch <= SE_WEST; ch++) {
				int pred_x = CONNECT_COORD[ch][0];
				int pred_y = CONNECT_COORD[ch][1];
				if ((pred_x >= 0 && pred_x < width) &&
					(pred_y >= 0 && pred_y < height)) {
					alu_sels[x][y][0]->connect(channels[pred_x][pred_y][0][OPPOSITE_CHANNEL[ch]]);
					alu_sels[x][y][1]->connect(channels[pred_x][pred_y][0][OPPOSITE_CHANNEL[ch]]);
				} else {
					alu_sels[x][y][0]->connect(NULL);
					alu_sels[x][y][1]->connect(NULL);
				}
			}
			//ALU to ALU (Direct Link)
			for (int ch = DL_S; ch <= DL_SW; ch++) {
				int pred_x = CONNECT_COORD[ch][0];
				int pred_y = CONNECT_COORD[ch][1];
				if ((pred_x >= 0 && pred_x < width) &&
					(pred_y >= 0 && pred_y < height)) {
					alu_sels[x][y][0]->connect(alus[pred_x][pred_y]);
					alu_sels[x][y][1]->connect(alus[pred_x][pred_y]);
				} else {
					alu_sels[x][y][0]->connect(NULL);
					alu_sels[x][y][1]->connect(NULL);
				}
			}
			//Const to ALU
			alu_sels[x][y][0]->connect(cregs[y]);
			alu_sels[x][y][0]->connect(cregs[y + height]);
			alu_sels[x][y][1]->connect(cregs[y]);
			alu_sels[x][y][1]->connect(cregs[y + height]);

			//own ALU to SE
			channels[x][y][0][SE_NORTH]->connect(alus[x][y]);
			channels[x][y][0][SE_SOUTH]->connect(alus[x][y]);
			channels[x][y][0][SE_EAST]->connect(alus[x][y]);

			// from south
			if (y > 0) {
				node_ptr = channels[x][y-1][0][SE_SOUTH];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][SE_NORTH]->connect(node_ptr);
			channels[x][y][0][SE_EAST]->connect(node_ptr);
			channels[x][y][0][SE_WEST]->connect(node_ptr);

			// from north
			if (y < height - 1) {
				node_ptr = channels[x][y+1][0][SE_NORTH];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][SE_SOUTH]->connect(node_ptr);

			// from east
			if (x > 0) {
				node_ptr = channels[x-1][y][0][SE_WEST];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][SE_NORTH]->connect(node_ptr);
			channels[x][y][0][SE_SOUTH]->connect(node_ptr);
			channels[x][y][0][SE_WEST]->connect(node_ptr);

			// from west
			if (x < width - 1) {
				node_ptr = channels[x+1][y][0][SE_EAST];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][SE_NORTH]->connect(node_ptr);
			channels[x][y][0][SE_SOUTH]->connect(node_ptr);
			channels[x][y][0][SE_EAST]->connect(node_ptr);

			// from south (DL)
			if (y > 0) {
				node_ptr = alus[x][y-1];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][SE_NORTH]->connect(node_ptr);
			channels[x][y][0][SE_EAST]->connect(node_ptr);
			channels[x][y][0][SE_WEST]->connect(node_ptr);

			// from southeast (DL)
			if (y > 0 && x > 0) {
				node_ptr = alus[x-1][y-1];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][SE_NORTH]->connect(node_ptr);
			channels[x][y][0][SE_EAST]->connect(node_ptr);
			channels[x][y][0][SE_WEST]->connect(node_ptr);

			// from southwest (DL)
			if (y > 0 && x > 0) {
				node_ptr = alus[x+1][y-1];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][SE_NORTH]->connect(node_ptr);
			channels[x][y][0][SE_EAST]->connect(node_ptr);

			// const to SE
			channels[x][y][0][SE_NORTH]->connect(pregs[y]);
		}
	}
}

PENodeBase::PENodeBase()
{

}

PENodeBase::~PENodeBase()
{
	//erase
	std::vector<PENodeBase*>().swap(predecessors);
}

void PENodeBase::connect(PENodeBase* pred)
{
	predecessors.push_back(pred);
}

void MUX::exec()
{
	obuf.push(predecessors[config_data]->getData());
}

void MUX::config(uint32 data)
{
	if (data < predecessors.size()) {
		config_data = data;
	}//otherwise: out of range
}

void ALU::exec()
{
	uint32 inA, inB;
	inA = predecessors[0]->getData();
	inB = predecessors[1]->getData();
	obuf.push(PE_op_table[opcode](inA, inB));
}

void ALU::config(uint32 data)
{
	if (data < CMA_OP_SIZE) {
		opcode = data;
	}
}

void PREG::exec()
{
	if (activated) {
		obuf.push(latch);
		latch = predecessors[0]->getData();
	} else {
		obuf.push(predecessors[0]->getData());
	}
}

void ConstReg::exec()
{
	obuf.push(*const_reg_ptr);
}

void MemLoadUnit::exec()
{
	obuf.push(predecessors[0]->getData());
}

void MemStoreUnit::exec()
{
	obuf.push(predecessors[0]->getData());
}

uint32 MemStoreUnit::gather()
{
	return obuf.front();
}

uint32 exec_nop(uint32 inA, uint32 inB)
{
	return 0;
}

uint32 exec_add(uint32 inA, uint32 inB)
{
	return CMA_DATA_MASK & (inA + inB);
}

uint32 exec_sub(uint32 inA, uint32 inB)
{
	uint32 tmpA, tmpB;
	tmpA = inA & CMA_DATA_MASK;
	tmpB = ~(inB & CMA_DATA_MASK) & CMA_DATA_MASK;
	return (tmpA + tmpB + 1U) & CMA_WORD_MASK;
}

uint32 exec_mult(uint32 inA, uint32 inB)
{
	uint32 tmpA = inA & CMA_WORD_MASK;
	uint32 carryA = inA & CMA_CARRY_MASK;
	uint32 tmpB = inB & CMA_WORD_MASK;
	uint32 carryB = inB & CMA_CARRY_MASK;
	return (carryA ^ carryB) | ((tmpA * tmpB) & CMA_WORD_MASK);
}

uint32 exec_sl(uint32 inA, uint32 inB)
{
	return CMA_DATA_MASK & (inA << inB);
}

uint32 exec_sr(uint32 inA, uint32 inB)
{
	return CMA_DATA_MASK & (inA >> inB);
}

uint32 exec_sra(uint32 inA, uint32 inB)
{
	uint32 carryA = inA & CMA_CARRY_MASK;
	uint32 tmp;
	if (carryA != 0) {
		if (inB > CMA_CARYY_LSB) {
			return 0;
		} else {
			tmp = inA >> inB;
			//make upper bits
			tmp |= ((1 << inB) -1) << (CMA_CARYY_LSB - inB);
			return tmp | (1 << CMA_CARYY_LSB);
		}
	} else {
		return exec_sr(inA, inB);
	}
}

uint32 exec_sel(uint32 inA, uint32 inB)
{
	uint32 carryA = inA & CMA_CARRY_MASK;
	if (carryA == 0) {
		return inA;
	} else {
		return inB;
	}
}

uint32 exec_cat(uint32 inA, uint32 inB)
{
	return inA;
}

uint32 exec_not(uint32 inA, uint32 inB)
{
	return ~inA;
}

uint32 exec_and(uint32 inA, uint32 inB)
{
	return inA & inB;
}

uint32 exec_or(uint32 inA, uint32 inB)
{
	return inA | inB;
}

uint32 exec_xor(uint32 inA, uint32 inB)
{
	return inA ^ inB;
}

uint32 exec_eql(uint32 inA, uint32 inB)
{
	uint32 tmpA = inA & CMA_WORD_MASK;
	uint32 tmpB = inB & CMA_WORD_MASK;
	if (tmpA == tmpB) {
		return (1 << CMA_CARYY_LSB) | tmpA;
	} else {
		return 0;
	}
}

uint32 exec_gt(uint32 inA, uint32 inB)
{
	//exec inA > inB
	uint32 tmpA = inA & CMA_WORD_MASK;
	bool carryA = (inA & CMA_CARRY_MASK) != 0;
	uint32 tmpB = inB & CMA_WORD_MASK;
	bool carryB = (inB & CMA_CARRY_MASK) != 0;

	if (((not carryA & not carryB) & (tmpA > tmpB)) | 
		(not carryA & carryB) |
		((carryA & carryB) & (tmpA < tmpB))) {
		return 1 << CMA_CARYY_LSB;
	} else {
		return 0;
	}

}

uint32 exec_lt(uint32 inA, uint32 inB)
{
	//exec inA <= inB
	uint32 tmp = exec_gt(inA, inB);
	if (tmp != 0) {
		return 0;
	} else {
		return 1 << CMA_CARYY_LSB;
	}
}

