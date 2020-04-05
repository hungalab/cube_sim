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




PEArray::PEArray(int height_, int width_) : height(height_), width(width_)
{
	array.resize(height);
	for (int i = 0; i < height; i++) {
		array[i].reserve(width);
		for (int j = 0; j < width; j++) {
			array[i].emplace_back(new PE());
		}
	}
	array[0][0]->exec();
}

PEArray::~PEArray()
{
	std::vector<std::vector<CMAComponents::PE*> >().swap(array);
}

PE::PE()
{
	// dummy connection
	input_ports[ALUCONNECTION] = &alu_out;
	input_ports[NOCONNECTION] = &empty_data;

	//test
	test0 = 10;
	test1 = 5;
	input_ports[PE_INPUT_SOUTH] = &test0;
	input_ports[PE_INPUT_CONST_A] = &test1;
	sel_a = 0;
	sel_b = 6;
	opcode = 0xB;
}

void PE::exec()
{
	uint32 inA = *(input_ports[sel_table[sel_a]]);
	uint32 inB = *(input_ports[sel_table[sel_b]]);
	alu_out = PE_op_table[opcode](inA, inB);
	fprintf(stderr, "exec %d\n", alu_out);
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

