#include "cmacore.h"

#define CMA_COUNT 1

using namespace CMAComponents;

CMACore::CMACore(LocalMapper *bus_, SIGNAL_PTR done_signal_,
				ControlReg *ctrl_, int node_)
		: AcceleratorCore(bus_, done_signal_), ctrl(ctrl_), node(node_)
{
	pearray = new PEArray(CMA_PE_ARRAY_HEIGHT, CMA_PE_ARRAY_WIDTH);
};

CMACore::~CMACore()
{
	delete pearray;
}

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


void CMACore::step()
{
	if (ctrl->getRun()) {
		count++;
		if (count == result_stat[context].wait) {
			for (int i = 0; i < result_stat[context].len; i++) {
				if (result_stat[context].bank0) {
					bus->store_word(result_stat[context].result_addr + 4 * i, result_stat[context].result[i]);
				} else {
					bus->store_word(0x1000 + result_stat[context].result_addr + 4 * i, result_stat[context].result[i]);
				}
			}
			done_signal(ctrl->getDoneDMA());
			context++;
		}
	} else {
		count = 0;
	}
}

void CMACore::reset()
{
	count = 0;
	context = 0;

#if CMA_COUNT == 1
	result_stat[0] = {0x02C0, true, &comp_y[0], 74, 176};
	result_stat[1] = {0x02C0, false, &comp_y[176], 74, 176};
	result_stat[2] = {0x02C0, true, &comp_cr[0], 74, 176};
	result_stat[3] = {0x02C0, false, &comp_cr[176], 74, 176};
	result_stat[4] = {0x02C0, true, &comp_cb[0], 74, 176};
	result_stat[5] = {0x02C0, false, &comp_cb[176], 74, 176};
	result_stat[6] = {0x40, true, &comp_dct[0], 31, 64};
	result_stat[7] = {0x40, false, &comp_dct[64], 31, 64};
	result_stat[8] = {0x40, true, &comp_dct[128], 31, 64};
	result_stat[9] = {0x40, false, &comp_dct[192], 31, 64};
	result_stat[10] = {0x40, true, &comp_dct[256], 31, 64};
	result_stat[11] = {0x40, false, &comp_dct[320], 31, 64};
	result_stat[12] = {0x40, true, &comp_dct[384], 31, 64};
	result_stat[13] = {0x40, false, &comp_dct[448], 31, 64};
	result_stat[14] = {0x40, true, &comp_dct[512], 31, 64};
	result_stat[15] = {0x40, false, &comp_dct[576], 31, 64};
	result_stat[16] = {0x40, true, &comp_dct[640], 31, 64};
	result_stat[17] = {0x40, false, &comp_dct[704], 31, 64};
	result_stat[18] = {0x300, true, &comp_dct_quant[0], 31, 64};
	result_stat[19] = {0x300, true, &comp_dct_quant[64], 31, 64};
	result_stat[20] = {0x300, true, &comp_dct_quant[128], 31, 64};
	result_stat[21] = {0x300, true, &comp_dct_quant[192], 31, 64};
	result_stat[22] = {0x300, true, &comp_dct_quant[256], 31, 64};
	result_stat[23] = {0x300, true, &comp_dct_quant[320], 31, 64};
	result_stat[24] = {0x300, true, &comp_dct_quant[384], 31, 64};
	result_stat[25] = {0x300, true, &comp_dct_quant[448], 31, 64};
	result_stat[26] = {0x300, true, &comp_dct_quant[512], 31, 64};
	result_stat[27] = {0x300, true, &comp_dct_quant[576], 31, 64};
	result_stat[28] = {0x300, true, &comp_dct_quant[640], 31, 64};
	result_stat[29] = {0x300, true, &comp_dct_quant[704], 31, 64};

#elif CMA_COUNT == 2
	if (node == 1) {
		result_stat[0] = {0x02C0, true, &comp_y[0], 74, 176};
		result_stat[1] = {0x02C0, false, &comp_y[176], 74, 176};
		result_stat[2] = {0x02C0, true, &comp_cr[0], 74, 176};
		result_stat[3] = {0x02C0, false, &comp_cr[176], 74, 176};
		result_stat[4] = {0x02C0, true, &comp_cb[0], 74, 176};
		result_stat[5] = {0x02C0, false, &comp_cb[176], 74, 176};
		result_stat[6] = {0x40, true, &comp_dct[0], 31, 64};
		result_stat[7] = {0x40, true, &comp_dct[64], 31, 64};
		result_stat[8] = {0x40, true, &comp_dct[128], 31, 64};
		result_stat[9] = {0x40, true, &comp_dct[192], 31, 64};
		result_stat[10] = {0x40, false, &comp_dct[256], 31, 64};
		result_stat[11] = {0x40, false, &comp_dct[320], 31, 64};
		result_stat[12] = {0x40, true, &comp_dct[384], 31, 64};
		result_stat[13] = {0x40, true, &comp_dct[448], 31, 64};
		result_stat[14] = {0x40, true, &comp_dct[512], 31, 64};
		result_stat[15] = {0x40, true, &comp_dct[576], 31, 64};
		result_stat[16] = {0x40, false, &comp_dct[640], 31, 64};
		result_stat[17] = {0x40, false, &comp_dct[704], 31, 64};
	} else if (node == 2) {
		result_stat[0] = {0x300, true, &comp_dct_quant[0], 31, 64};
		result_stat[1] = {0x300, true, &comp_dct_quant[64], 31, 64};
		result_stat[2] = {0x300, true, &comp_dct_quant[128], 31, 64};
		result_stat[3] = {0x300, true, &comp_dct_quant[192], 31, 64};
		result_stat[4] = {0x300, false, &comp_dct_quant[256], 31, 64};
		result_stat[5] = {0x300, false, &comp_dct_quant[320], 31, 64};
		result_stat[6] = {0x300, true, &comp_dct_quant[384], 31, 64};
		result_stat[7] = {0x300, true, &comp_dct_quant[448], 31, 64};
		result_stat[8] = {0x300, true, &comp_dct_quant[512], 31, 64};
		result_stat[9] = {0x300, true, &comp_dct_quant[576], 31, 64};
		result_stat[10] = {0x300, false, &comp_dct_quant[640], 31, 64};
		result_stat[11] = {0x300, false, &comp_dct_quant[704], 31, 64};
	}

#elif CMA_COUNT == 3
	//for 3 chips
	if (node == 1) {
		result_stat[0] = {0x02C0, true, &comp_y[0], 74, 176};
		result_stat[1] = {0x02C0, false, &comp_y[176], 74, 176};
		result_stat[2] = {0x40, true, &comp_dct[0], 31, 64};
		result_stat[3] = {0x40, false, &comp_dct[64], 31, 64};
		result_stat[4] = {0x40, true, &comp_dct[128], 31, 64};
		result_stat[5] = {0x40, false, &comp_dct[192], 31, 64};
		result_stat[6] = {0x40, true, &comp_dct[256], 31, 64};
		result_stat[7] = {0x40, false, &comp_dct[320], 31, 64};
		result_stat[8] = {0x40, true, &comp_dct[384], 31, 64};
		result_stat[9] = {0x40, false, &comp_dct[448], 31, 64};
		result_stat[10] = {0x40, true, &comp_dct[512], 31, 64};
		result_stat[11] = {0x40, false, &comp_dct[576], 31, 64};
		result_stat[12] = {0x40, true, &comp_dct[640], 31, 64};
		result_stat[13] = {0x40, false, &comp_dct[704], 31, 64};
		result_stat[14] = {0x300, true, &comp_dct_quant[0], 31, 64};
		result_stat[15] = {0x300, true, &comp_dct_quant[64], 31, 64};
		result_stat[16] = {0x300, true, &comp_dct_quant[128], 31, 64};
		result_stat[17] = {0x300, true, &comp_dct_quant[192], 31, 64};
		result_stat[18] = {0x300, true, &comp_dct_quant[256], 31, 64};
		result_stat[19] = {0x300, true, &comp_dct_quant[320], 31, 64};
		result_stat[20] = {0x300, true, &comp_dct_quant[384], 31, 64};
		result_stat[21] = {0x300, true, &comp_dct_quant[448], 31, 64};
		result_stat[22] = {0x300, true, &comp_dct_quant[512], 31, 64};
		result_stat[23] = {0x300, true, &comp_dct_quant[576], 31, 64};
		result_stat[24] = {0x300, true, &comp_dct_quant[640], 31, 64};
		result_stat[25] = {0x300, true, &comp_dct_quant[704], 31, 64};
	} else if (node == 2) {
		result_stat[0] = {0x02C0, true, &comp_cr[0], 74, 176};
		result_stat[1] = {0x02C0, false, &comp_cr[176], 74, 176};
	} else if (node == 3) {
		result_stat[0] = {0x02C0, true, &comp_cb[0], 74, 176};
		result_stat[1] = {0x02C0, false, &comp_cb[176], 74, 176};
	}
#endif

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
	//array.assign(width, std::vector<PE>(column));
	//fprintf(stderr, "%X\n", array[0][0]->exec_add(2, 3));
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

}

// uint32 PE::exec(uint32 inA, uint32 inB)
// {
// 	return PE_op_table[opcode](inA, inB);
// }

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
	return (tmpA + tmpB + 1U);
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

