#include "cmamodules.h"

#define CMA_COUNT 1

using namespace CMAComponents;

uint32 ConstRegCtrl::fetch_word(uint32 offset, int mode, DeviceExc *client)
{
	return pearray->load_const(offset);
}

void ConstRegCtrl::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	pearray->store_const(offset, data);
}

uint32 DManuTableCtrl::fetch_word(uint32 offset, int mode, DeviceExc *client)
{
	uint32 table_index = (offset & CMA_TABLE_SEL_MASK)
							>> CMA_TABLE_INDEX_LSB;
	uint32 table_sel = (offset & CMA_TABLE_SEL_MASK)
							>> CMA_TABLE_INDEX_LSB;

	uint32 ret_data = 0;

	switch (table_sel) {
		case CMA_TABLE_BITMAP_OFFSET:
			for (int i = 0; i < CMA_INTERLEAVE_SIZE; i++) {
				int flag = dmanu->getBitmap(table_index, i) ? 1 : 0;
				ret_data |= flag << i;
			}
			break;
		case CMA_TABLE_FORMAR_OFFSET:
			for (int i = 0; i < CMA_INTERLEAVE_SIZE / 2; i++) {
				ret_data |= dmanu->getTable(table_index, i)
							<< (i * CMA_TABLE_ELEMENT_BITW);
			}
			break;
		case CMA_TABLE_LATTER_OFFSET:
			for (int i = 0; i < CMA_INTERLEAVE_SIZE / 2; i++) {
				int j = i + CMA_INTERLEAVE_SIZE / 2;
				ret_data |= dmanu->getTable(table_index, j)
							<< (i * CMA_TABLE_ELEMENT_BITW);
			}
			break;
	}


	return ret_data;

}

void DManuTableCtrl::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	uint32 table_index = (offset & CMA_TABLE_INDEX_MASK)
							>> CMA_TABLE_INDEX_LSB;
	uint32 table_sel = (offset & CMA_TABLE_SEL_MASK)
							>> CMA_TABLE_SEL_LSB;


	switch (table_sel) {
		case CMA_TABLE_BITMAP_OFFSET:
			for (int i = 0; i < CMA_INTERLEAVE_SIZE; i++) {
				bool flag = ((data >> i) & 0x1) != 0;
				dmanu->setBitmap(table_index, i, flag);
			}
			break;
		case CMA_TABLE_FORMAR_OFFSET:
			for (int i = 0; i < CMA_INTERLEAVE_SIZE / 2; i++) {
				int pos = (data >> (i * CMA_TABLE_ELEMENT_BITW)) &
						CMA_TABLE_ELEMENT_MASK;
				dmanu->setTable(table_index, i, pos);
			}
			break;
		case CMA_TABLE_LATTER_OFFSET:
			for (int i = 0; i < CMA_INTERLEAVE_SIZE / 2; i++) {
				int j = i + CMA_INTERLEAVE_SIZE / 2;
				int pos = (data >> (i * CMA_TABLE_ELEMENT_BITW)) &
						CMA_TABLE_ELEMENT_MASK;
				dmanu->setTable(table_index, j, pos);
			}
			break;
	}

}

void RMCALUConfigCtrl::store_word(uint32 offset, uint32 data, DeviceExc *client){
	uint32 row, col, config;
	row = (data & CMA_RMC_ROW_BITMAP_MASK) >> CMA_RMC_ROW_BITMAP_LSB;
	col = (data & CMA_RMC_COL_BITMAP_MASK) >> CMA_RMC_COL_BITMAP_LSB;
	config = data & CMA_RMC_CONFIG_MASK;
	pearray->config_ALU_RMC(col, row, config);
}

void PREGConfigCtrl::store_word(uint32 offset, uint32 data, DeviceExc *client){
	pearray->config_PREG(data);
}

void RMCSEConfigCtrl::store_word(uint32 offset, uint32 data, DeviceExc *client){
	uint32 row, col, config;
	row = (data & CMA_RMC_ROW_BITMAP_MASK) >> CMA_RMC_ROW_BITMAP_LSB;
	col = (data & CMA_RMC_COL_BITMAP_MASK) >> CMA_RMC_COL_BITMAP_LSB;
	config = data & CMA_RMC_CONFIG_MASK;
	pearray->config_SE_RMC(col, row, config);
}

void PEConfigCtrl::store_word(uint32 offset, uint32 data, DeviceExc *client){
	uint32 pe_addr = offset >> CMA_PE_ADDR_LSB;
	pearray->config_PE(pe_addr, data);
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


PEArray::PEArray(int height_, int width_, int preg_channels_,
					int se_count_, int se_channels_) :
	height(height_), width(width_), preg_channels(preg_channels_),
	se_count(se_count_), se_channels(se_channels_)
{
	make_ALUs();
	make_SEs();
	make_const_regs();
	if (preg_channels > 0) {
		make_pregs();
	}
	make_memports();
	config_changed = false;
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
	in_degrees.clear();
	std::vector <PENodeBase*>().swap(tsorted_list);

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

			//Debug
			debug_str[alus[x][y]] = std::string("ALU_") + 
										std::to_string(x) + 
										std::string("_") + std::to_string(y);
			debug_str[alu_sels[x][y][0]] = std::string("SEL_A_") + 
										std::to_string(x) + 
										std::string("_") + std::to_string(y);
			debug_str[alu_sels[x][y][1]] = std::string("SEL_B_") + 
										std::to_string(x) + 
										std::string("_") + std::to_string(y);
		}
	}
}

void PEArray::make_SEs()
{
	channels = new MUX****[width];
	for (int x = 0; x < width; x++) {
		channels[x] = new MUX***[height];
		for (int y = 0; y < height; y++) {
			channels[x][y] = new MUX**[se_count];
			for (int se = 0; se < se_count; se++) {
				channels[x][y][se] = new MUX*[se_channels];
				for (int ch = 0; ch < se_channels; ch++) {
					channels[x][y][se][ch] = new MUX();
				}
				//Debug
				debug_str[channels[x][y][0][0]] = std::string("OUT_NORTH_") + 
										std::to_string(x) + 
										std::string("_") + std::to_string(y);
				debug_str[channels[x][y][0][1]] = std::string("OUT_SOUTH_") + 
										std::to_string(x) + 
										std::string("_") + std::to_string(y);
				debug_str[channels[x][y][0][2]] = std::string("OUT_EAST_") + 
										std::to_string(x) + 
										std::string("_") + std::to_string(y);
				debug_str[channels[x][y][0][3]] = std::string("OUT_WEST_") + 
										std::to_string(x) + 
										std::string("_") + std::to_string(y);
			}
		}
	}
}

void PEArray::make_const_regs()
{
	cregs = new ConstReg*[height * 2];
	for (int i = 0; i < height * 2; i++) {
		cregs[i] = new ConstReg();
		//Debug
		debug_str[cregs[i]] = std::string("CONST_") + std::to_string(i);
	}
}

void PEArray::make_pregs()
{
	pregs = new PREG***[width];
	for (int x = 0; x < width; x++) {
		pregs[x] = new PREG**[height - 1];
		for (int y = 0; y < height - 1; y++) {
			pregs[x][y] = new PREG*[preg_channels];
			for (int i = 0; i < preg_channels; i++) {
				pregs[x][y][i] = new PREG();
				//Debug
				debug_str[pregs[x][y][i]] = std::string("PREG_") + std::to_string(x) + "_" + std::to_string(y) + "_" +
				std::to_string(i);
			}
		}
	}
}

void PEArray::make_memports()
{
	launch_regs = new MemLoadUnit*[width];
	gather_regs = new MemStoreUnit*[width];
	for (int x = 0; x < width; x++) {
		launch_regs[x] = new MemLoadUnit();
		gather_regs[x] = new MemStoreUnit();
		//Debug
		debug_str[launch_regs[x]] = std::string("MEMIN_") + std::to_string(x);
		debug_str[gather_regs[x]] = std::string("MEMOUT_") + std::to_string(x);
	}
}

uint32 PEArray::load_const(uint32 addr)
{
	uint32 index = addr >> 2;
	if (index > height * 2) {
		return 0;
	} else {
		return cregs[index]->getData();
	}
}

void PEArray::store_const(uint32 addr, uint32 data)
{
	uint32 index = addr >> 2;
	if (index < height * 2) {
		cregs[index]->writeData(data);
	}
}

void PEArray::config_PREG(uint32 data)
{
	bool flag;
	for (int y = 0; y < (height - 1); y++) {
		if (((data >> y) & 0x1) != 0) {
			for (int x = 0; x < width; x++) {
				for (int i = 0; i < preg_channels; i++) {
					pregs[x][y][i]->activate();
				}
			}
		} else {
			for (int x = 0; x < width; x++) {
				for (int i = 0; i < preg_channels; i++) {
					pregs[x][y][i]->deactivate();
				}
			}
		}
	}
	config_changed = true;
}

void PEArray::decode_alu_conf(uint32 data, uint32& opcode,
									uint32& sel_a, uint32& sel_b)
{
	opcode = (data & CMA_OPCODE_MASK) >> CMA_OPCODE_LSB;
	sel_a = (data & CMA_SEL_A_MASK) >> CMA_SEL_A_LSB;
	sel_b = data & CMA_SEL_B_MASK;
}

void PEArray::decode_se_conf(uint32 data, uint32& se_north,
									uint32& se_south, uint32& se_east,
									uint32& se_west)
{
	se_north = (data & CMA_SE_NORTH_MASK) >> CMA_SE_NORTH_LSB;
	se_south = (data & CMA_SE_SOUTH_MASK) >> CMA_SE_SOUTH_LSB;
	se_east = (data & CMA_SE_EAST_MASK) >> CMA_SE_EAST_LSB;
	se_west = data & CMA_SE_WEST_MASK;
}

void PEArray::config_ALU_RMC(int col_bitmap, int row_bitmap, int data)
{
	//decode data
	uint32 opcode, sel_a, sel_b;
	decode_alu_conf(data, opcode, sel_a, sel_b);

	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			if ((((col_bitmap >> x) & 0x1) != 0) && 
				(((row_bitmap >> y) & 0x1) != 0)) {
					alus[x][y]->config(opcode);
					alu_sels[x][y][0]->config(sel_a);
					alu_sels[x][y][1]->config(sel_b);
				}
		}
	}
	config_changed = true;
}

void PEArray::config_SE_RMC(int col_bitmap, int row_bitmap, int data)
{
	//decode data
	uint32 se_north, se_south, se_east, se_west;
	decode_se_conf(data, se_north, se_south, se_east, se_west);

	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			if ((((col_bitmap >> x) & 0x1) != 0) && 
				(((row_bitmap >> y) & 0x1) != 0)) {
					channels[x][y][0][0]->config(se_north);
					channels[x][y][0][1]->config(se_south);
					channels[x][y][0][2]->config(se_east);
					channels[x][y][0][3]->config(se_west);
				}
		}
	}
	config_changed = true;
}

void PEArray::config_PE(int pe_addr, int data)
{
	int x, y;
	x = pe_addr % width;
	y = pe_addr / width;
	config_PE(x, y, data);
}

void PEArray::config_PE(int x, int y, int data)
{
	uint32 opcode, sel_a, sel_b;
	uint32 se_north, se_south, se_east, se_west;

	decode_alu_conf(data & CMA_ALU_CONFIG_MASK, opcode, sel_a, sel_b);
	decode_se_conf((data & CMA_SE_CONFIG_MASK) >> CMA_SE_CONF_LSB,
					 se_north, se_south, se_east, se_west);
	alus[x][y]->config(opcode);
	alu_sels[x][y][0]->config(sel_a);
	alu_sels[x][y][1]->config(sel_b);
	channels[x][y][0][0]->config(se_north);
	channels[x][y][0][1]->config(se_south);
	channels[x][y][0][2]->config(se_east);
	channels[x][y][0][3]->config(se_west);
	config_changed = true;
}

void PEArray::analyze_dataflow()
{
	//init in_degrees
	in_degrees.clear();
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			in_degrees[alus[x][y]] = alus[x][y]->in_degree();
			in_degrees[alu_sels[x][y][0]] = alu_sels[x][y][0]->in_degree();
			in_degrees[alu_sels[x][y][1]] = alu_sels[x][y][1]->in_degree();
			for (int se = 0; se < se_count; se++) {
				for (int ch = 0; ch < se_channels; ch++) {
					in_degrees[channels[x][y][se][ch]] =
						channels[x][y][se][ch]->in_degree();
				}
			}
		}
		in_degrees[launch_regs[x]] = launch_regs[x]->in_degree();
		in_degrees[gather_regs[x]] = gather_regs[x]->in_degree();
	}
	for (int y = 0; y < height; y++) {
		in_degrees[cregs[y]] = cregs[y]->in_degree();
		in_degrees[cregs[y+height]] = cregs[y+height]->in_degree();
	}
	if (preg_channels > 0) {
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height - 1; y++) {
				for (int i = 0; i < preg_channels; i++) {
					in_degrees[pregs[x][y][i]] 
						= pregs[x][y][i]->in_degree();
				}
			}
		}
	}

	//make topological sort
	NodeList().swap(tsorted_list);
	NodeList del_list;
	int prev_size;
	do {
		prev_size = in_degrees.size();
		//fprintf(stderr, "left nodes: %lu\n", in_degrees.size());
		for (auto i = in_degrees.begin(); i != in_degrees.end(); i++) {
			PENodeBase *p = i->first;
			int in_deg = i->second;
			if (in_deg == 0) {
				del_list.push_back(p);
				NodeList list = p->use_successors();
				if (list.size() > 0) {
					//fprintf(stderr, "current node %s\n", debug_str[p].c_str());
					tsorted_list.push_back(p);
					for (auto j = list.begin(); j != list.end(); j++) {
						 //fprintf(stderr, "next is %s %d\n", debug_str[*j].c_str(), in_degrees[*j]);
						(in_degrees[*j])--;
					}
				}
			}
		}
		//fprintf(stderr, "del nodes %d\n", del_list.size());
		for (auto i = del_list.begin(); i != del_list.end(); i++) {
			in_degrees.erase(*i);
		}
	} while (in_degrees.size() < prev_size);

	config_changed = false;
}

void PEArray::exec()
{
	if (config_changed) {
		analyze_dataflow();
	}
	for (auto it = tsorted_list.begin(); it != tsorted_list.end(); it++) {
		PENodeBase *p = *it;
		p->exec();
	}

	for (int i = 0; i < width; i++) {
		gather_regs[i]->exec();
	}
	for (auto it = tsorted_list.begin(); it != tsorted_list.end(); it++) {
		PENodeBase *p = *it;
		p->update();
	}
}

void PEArray::launch(ArrayData input_data)
{
	for (int i = 0; i < width; i++) {
		launch_regs[i]->store(input_data[i]);
	}
}

ArrayData PEArray::gather()
{
	ArrayData output_data(width);
	for (int i = 0; i < width; i++) {
		output_data[i] = gather_regs[i]->load();
	}
	return output_data;
}

uint32 PEArray::debug_fetch_launch(uint32 col)
{
	return (col < width) ? launch_regs[col]->getData() : 0;
}

void PEArray::debug_store_launch(uint32 col, uint32 data)
{
	if (col < width) {
		launch_regs[col]->debug_push_data(data & CMA_DWORD_MASK);
	}
}

uint32 PEArray::debug_fetch_gather(uint32 col)
{
	return  (col < width) ? gather_regs[col]->getData() : 0;
}

void PEArray::debug_store_gather(uint32 col, uint32 data)
{
	if (col < width) {
		gather_regs[col]->debug_push_data(data & CMA_DWORD_MASK);
	}
}

uint32 PEArray::debug_fetch_ALU(uint8 pe_addr, uint8 type)
{
	int x = pe_addr % width;
	int y = pe_addr / width;

	if (y >= height) return 0;

	switch (type) {
		case CMA_DEBUG_MOD_ALU_L:
			return alu_sels[x][y][0]->getData();
		case CMA_DEBUG_MOD_ALU_R:
			return alu_sels[x][y][1]->getData();
		case CMA_DEBUG_MOD_ALU_O:
			return alus[x][y]->getData();
		default:
			return 0;
	}
}

void PEArray::debug_store_ALU(uint8 pe_addr, uint8 type, uint32 data)
{
	int x = pe_addr % width;
	int y = pe_addr / width;

	if (y >= height) return;

	switch (type) {
		case CMA_DEBUG_MOD_ALU_L:
			alu_sels[x][y][0]->debug_push_data(data & CMA_DWORD_MASK);
			break;
		case CMA_DEBUG_MOD_ALU_R:
			alu_sels[x][y][1]->debug_push_data(data  & CMA_DWORD_MASK);
			break;
		case CMA_DEBUG_MOD_ALU_O:
			alus[x][y]->debug_push_data(data  & CMA_DWORD_MASK);
			break;
	}
	return;
}

DataManipulator::DataManipulator(int interleave_size_,
								DoubleBuffer*** dbank_,
								PEArray *pearray_) :
	interleave_size(interleave_size_),
	dbank(dbank_), pearray(pearray_)
{
	for (int i = 0; i < CMA_TABLE_ENTRY_SIZE; i++) {
		bitmap[i] = new bool[interleave_size];
		table[i] = new int[interleave_size];
	}
}

void DataManipulator::setBitmap(int index, int pos, bool flag)
{
	if (index < CMA_TABLE_ENTRY_SIZE) {
		bitmap[index][pos] = flag;
	}
}

void DataManipulator::setTable(int index, int pos, int data)
{
	if (index < CMA_TABLE_ENTRY_SIZE) {
		table[index][pos] = data;
	}
}

bool DataManipulator::getBitmap(int index, int pos)
{
	return (index < CMA_TABLE_ENTRY_SIZE) ? bitmap[index][pos] : false;
}

int DataManipulator::getTable(int index, int pos)
{
	return (index < CMA_TABLE_ENTRY_SIZE) ? table[index][pos] : 0;
}


void DataManipulator::toPEArray(uint32 load_addr, int table_index)
{
	if (table_index < CMA_TABLE_ENTRY_SIZE) {
		bool *use_bitmap = bitmap[table_index];
		int *use_table = table[table_index];

		uint32 dmem_addr;

		ArrayData launch_data(interleave_size, 0);
		for (int i = 0; i < interleave_size; i++) {
			if (use_bitmap[i]) {
				dmem_addr = load_addr + (use_table[i] * 4);
				launch_data[i] = (**dbank)->fetch_word_from_inner(dmem_addr);
			}
		}
		pearray->launch(launch_data);
	}
}

void DataManipulator::fromPEArray(uint32 store_addr, int table_index)
{
	if (table_index < CMA_TABLE_ENTRY_SIZE) {
		bool *use_bitmap = bitmap[table_index];
		int *use_table = table[table_index];

		uint32 dmem_addr;

		ArrayData gather_data = pearray->gather();
		for (int i = 0; i < interleave_size; i++) {
			if (use_bitmap[i]) {
				dmem_addr = store_addr + i * 4;
				(**dbank)->store_word_from_inner(dmem_addr,
					gather_data[use_table[i]]);
			}
		}

	}
}


STUnit::STUnit(int interleave_size_,  DoubleBuffer*** dbank_,
				PEArray *pearray_, int max_delay_):
	DataManipulator(interleave_size_, dbank_, pearray_),
	max_delay(max_delay_)
{
	for (int i = 0;i < max_delay; i++) {
		late_signal.push_back(st_signal_t{false, 0, 0});
	}
	delay = 0;
	pending_count = 0;
};

void STUnit::step()
{
	late_signal.pop_back();
	if (late_signal.size() < max_delay) {
		late_signal.push_front(st_signal_t{false, 0, 0});
	}
	st_signal_t current_signal = late_signal[delay+1];

	if (current_signal.enable) {
		// execute store
		fromPEArray(current_signal.store_addr, current_signal.table_sel);
		pending_count--;
	}
}

void STUnit::issue(uint32 store_addr, uint32 table_index)
{
	late_signal.push_front(st_signal_t{true, table_index, store_addr});
	pending_count++;
}


void MicroController::reset()
{
	for (int i = 0; i < CMA_MC_REG_SIZE; i++) {
		regfile[i] = 0;
	}
	pc = 0;
	launch_addr = launch_incr = gather_addr = gather_incr = 0;
}

void MicroController::step()
{
	uint32 next_pc = pc + 4;
	uint16 instr = (uint16)imem->fetch_word_from_inner(pc);

	uint8 src, dst;
	uint8 ld_table_index, st_table_index;
	src = reg_src(instr);
	dst = reg_dst(instr);

	//fprintf(stderr, "MC PC = %X instr %X\n", pc, instr);

	switch (opcode(instr)) {
		case CMA_MC_OPCODE_REG:
			switch (func(instr)) {
				case CMA_MC_FUNC_NOP:
					//nothing to do
					break;
				case CMA_MC_FUNC_ADD:
					regfile[dst] = regfile[dst] + regfile[src];
					break;
				case CMA_MC_FUNC_SUB:
					regfile[dst] = regfile[dst] - regfile[src];
					break;
				case CMA_MC_FUNC_MV:
					regfile[dst] = regfile[src];
					break;
				case CMA_MC_FUNC_DONE:
					if (st_unit->isWorking()) {
						// stall
						next_pc = pc;
					} else {
						*done_ptr = true;
					}
					break;
				case CMA_MC_FUNC_DELAY:
					st_unit->setDelay(dst);
					break;
				default:
					break;
			}
			break;
		case CMA_MC_OPCODE_LDI:
			regfile[dst] = imm(instr);
			break;
		case CMA_MC_OPCODE_ADDI:
			regfile[dst] = (uint16)((int16)regfile[dst]
									+ (int16)s_imm(instr));
			break;
		case CMA_MC_OPCODE_LD_ST_ADD:
			//exec launch & issue gather
			ld_table_index = reg_src(instr);
			st_table_index = func(instr);
			//word addressing -> byte addressing
			ld_unit->toPEArray(launch_addr << 2, ld_table_index);
			st_unit->issue(gather_addr << 2, st_table_index);
			launch_addr += launch_incr;
			gather_addr += gather_incr;
			break;
		case CMA_MC_OPCODE_BEZ:
			if (regfile[dst] == 0) {
				next_pc = (uint32)((int16)pc + 4 + (int16)s_imm(instr) * 4);
			}
			break;
		case CMA_MC_OPCODE_BEZD:
			if (regfile[dst] == 0) {
				next_pc = (uint32)((int16)pc + 4 + (int16)s_imm(instr) * 4);
				regfile[dst]--;
			}
			break;
		case CMA_MC_OPCODE_BNZ:
			if (regfile[dst] != 0) {
				next_pc = (uint32)((int16)pc + 4 + (int16)s_imm(instr) * 4);
			}
			break;
		case CMA_MC_OPCODE_BNZD:
			if (regfile[dst] != 0) {
				next_pc = (uint32)((int16)pc + 4 + (int16)s_imm(instr) * 4);
				regfile[dst]--;
			}
			break;
		case CMA_MC_OPCODE_SET_LD:
			launch_incr = func(instr);
			launch_addr = addr(instr);
			break;
		case CMA_MC_OPCODE_SET_ST:
			gather_incr = func(instr);
			gather_addr = addr(instr);
			break;
		default:
			break;
	}
	pc = next_pc;
}

uint32 MicroController::debug_fetch_regfile(uint32 sel)
{
	return (sel < CMA_MC_REG_SIZE) ? (uint32)regfile[sel] : 0;
}

void MicroController::debug_store_regfile(uint32 sel, uint32 data)
{
	if (sel < CMA_MC_REG_SIZE) {
		regfile[sel] = (uint16)data;
	}
}


void CCSOTB2::CCSOTB2_PEArray::make_connection()
{
	using namespace CCSOTB2;
	PENodeBase* node_ptr;
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			//SE to ALU
			for (int ch = IN_SE_SOUTH; ch <= IN_SE_WEST; ch++) {
				int pred_x = CONNECT_COORD[ch][0] + x;
				int pred_y = CONNECT_COORD[ch][1] + y;
				if ((pred_x >= 0 && pred_x < width) &&
					(pred_y >= 0 && pred_y < height)) {
					if (ch == IN_SE_SOUTH) {
						// from preg
						node_ptr = pregs[pred_x][pred_y][1];
					} else {
						node_ptr = channels[pred_x][pred_y][0][OPPOSITE_CHANNEL[ch]];
					}
					alu_sels[x][y][0]->connect(node_ptr);
					alu_sels[x][y][1]->connect(node_ptr);
				} else if (pred_y == -1) {
					alu_sels[x][y][0]->connect(launch_regs[x]);
					alu_sels[x][y][1]->connect(launch_regs[x]);
				} else {
					alu_sels[x][y][0]->connect(NULL);
					alu_sels[x][y][1]->connect(NULL);
				}
			}
			//ALU to ALU (Direct Link)
			//   ALU -> PREG
			if (y < height - 1) {
				pregs[x][y][0]->connect(alus[x][y]);
			}
			//   PREG -> ALU
			for (int ch = IN_DL_S; ch <= IN_DL_SW; ch++) {
				int pred_x = CONNECT_COORD[ch][0] + x;
				int pred_y = CONNECT_COORD[ch][1] + y;
				if ((pred_x >= 0 && pred_x < width) &&
					(pred_y >= 0 && pred_y < height)) {
					alu_sels[x][y][0]->connect(pregs[pred_x][pred_y][0]);
					alu_sels[x][y][1]->connect(pregs[pred_x][pred_y][0]);
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
			channels[x][y][0][OUT_SE_NORTH]->connect(alus[x][y]);
			channels[x][y][0][OUT_SE_SOUTH]->connect(alus[x][y]);
			channels[x][y][0][OUT_SE_EAST]->connect(alus[x][y]);

			// from south
			if (y > 0) {
				node_ptr = pregs[x][y-1][1];
			} else {
				node_ptr = launch_regs[x];
			}
			channels[x][y][0][OUT_SE_NORTH]->connect(node_ptr);
			channels[x][y][0][OUT_SE_EAST]->connect(node_ptr);
			channels[x][y][0][OUT_SE_WEST]->connect(node_ptr);

			// from north
			if (y < height - 1) {
				node_ptr = channels[x][y+1][0][OPPOSITE_CHANNEL[IN_SE_NORTH]];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][OUT_SE_SOUTH]->connect(node_ptr);

			// from east
			if (x < width - 1) {
				node_ptr = channels[x+1][y][0][OPPOSITE_CHANNEL[IN_SE_EAST]];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][OUT_SE_NORTH]->connect(node_ptr);
			channels[x][y][0][OUT_SE_SOUTH]->connect(node_ptr);
			channels[x][y][0][OUT_SE_WEST]->connect(node_ptr);

			// from west
			if (x > 0) {
				node_ptr = channels[x-1][y][0][OPPOSITE_CHANNEL[IN_SE_WEST]];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][OUT_SE_NORTH]->connect(node_ptr);
			channels[x][y][0][OUT_SE_SOUTH]->connect(node_ptr);
			channels[x][y][0][OUT_SE_EAST]->connect(node_ptr);

			// from south (DL)
			if (y > 0) {
				node_ptr = pregs[x][y-1][0];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][OUT_SE_NORTH]->connect(node_ptr);
			channels[x][y][0][OUT_SE_EAST]->connect(node_ptr);
			channels[x][y][0][OUT_SE_WEST]->connect(node_ptr);

			// from southeast (DL)
			if (y > 0 && x < width - 1) {
				node_ptr = pregs[x+1][y-1][0];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][OUT_SE_NORTH]->connect(node_ptr);
			channels[x][y][0][OUT_SE_EAST]->connect(node_ptr);
			channels[x][y][0][OUT_SE_WEST]->connect(node_ptr);

			// from southwest (DL)
			if (y > 0 && x > 0) {
				node_ptr = pregs[x-1][y-1][0];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][OUT_SE_NORTH]->connect(node_ptr);
			channels[x][y][0][OUT_SE_EAST]->connect(node_ptr);

			// const to SE
			channels[x][y][0][OUT_SE_NORTH]->connect(cregs[y]);

			// SE to PREG
			if (y < height - 1) {
				pregs[x][y][1]->connect(channels[x][y][0][OUT_SE_NORTH]);
			}
		}
		gather_regs[x]->connect(channels[x][0][0][OUT_SE_SOUTH]);
	}
}

PENodeBase::PENodeBase()
{

}

PENodeBase::~PENodeBase()
{
	//erase
	std::vector<PENodeBase*>().swap(predecessors);
	std::vector<PENodeBase*>().swap(successors);
}

NodeList PENodeBase::use_successors()
{
	NodeList list;
	for (int i = 0; i < successors.size(); i++) {
		if (successors[i] != NULL) {
			if (successors[i]->isUse(this)) {
				list.push_back(successors[i]);
			} else {
				// fprintf(stderr, "%s not used after %s\n", 
				// 	debug_str[successors[i]].c_str(),
				// 	debug_str[this].c_str());
			}
		}
	}
	return list;
}
void PENodeBase::connect(PENodeBase* pred)
{
	predecessors.push_back(pred);
	if (pred != NULL) {
		pred->successors.push_back(this);
	}
}

void PENodeBase::debug_push_data(uint32 data)
{
	std::queue<uint32> tmp;
	tmp.push(data);
	obuf.pop(); //discard first data
	while (!obuf.empty()) {
		tmp.push(obuf.front());
		tmp.pop();
	}
	std::swap(tmp, obuf);
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

bool MUX::isUse(PENodeBase* pred)
{
	return predecessors[config_data] == pred;
}

void ALU::exec()
{
	uint32 inA, inB;
	inA = predecessors[0]->getData();
	inB = predecessors[1]->getData();
	obuf.push(PE_op_table[opcode](inA, inB));
	//fprintf(stderr, "ALU in %x, %x out %x\n", inA, inB, obuf.front());
}

bool ALU::isUse(PENodeBase* pred)
{
	return opcode != 0;
}

void ALU::config(uint32 data)
{
	if (data < CMA_OP_SIZE) {
		opcode = data;
	}
}

int ALU::in_degree()
{
	return 2;
//	return PE_operand_size[opcode];
};

bool PREG::isUse(PENodeBase* pred)
{
	int used = false;
	for (auto it = successors.begin(); it != successors.end(); it++) {
		used |= (*it)->isUse(this);
	}
	return used;
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

uint32 ConstReg::getData() {
#if CMA_CONST_MASK == CMA_DWORDMASK
	return const_data;
#elif CMA_CONST_MASK < CMA_DWORD_MASK
	//need sing extension
	if (const_data > (CMA_CONST_MASK >> 1)) {
		//negative value
		return (CMA_DWORD_MASK ^ CMA_CONST_MASK) | const_data;
	} else {
		//positive value
		return const_data;
	}
#else
#error invalid const mask for CMA
#endif
};

void ConstReg::writeData(uint32 data) {
	const_data = data & CMA_CONST_MASK;
};


void MemStoreUnit::exec()
{
	obuf.pop();
	obuf.push(predecessors[0]->getData());
}

uint32 MemStoreUnit::load()
{
	return obuf.front();
}

uint32 CMAComponents::exec_nop(uint32 inA, uint32 inB)
{
	return 0;
}

uint32 CMAComponents::exec_add(uint32 inA, uint32 inB)
{
	return CMA_DWORD_MASK & (inA + inB);
}

uint32 CMAComponents::exec_sub(uint32 inA, uint32 inB)
{
	uint32 tmpA, tmpB;
	tmpA = inA & CMA_DWORD_MASK;
	tmpB = ~(inB & CMA_DWORD_MASK) & CMA_DWORD_MASK;
	return (tmpA + tmpB + 1U) & CMA_DATA_MASK;
}

uint32 CMAComponents::exec_mult(uint32 inA, uint32 inB)
{
	uint32 tmpA = inA & CMA_DATA_MASK;
	uint32 carryA = inA & CMA_CARRY_MASK;
	uint32 tmpB = inB & CMA_DATA_MASK;
	uint32 carryB = inB & CMA_CARRY_MASK;
	return (carryA ^ carryB) | ((tmpA * tmpB) & CMA_DATA_MASK);
}

uint32 CMAComponents::exec_sl(uint32 inA, uint32 inB)
{
	return CMA_DWORD_MASK & (inA << inB);
}

uint32 CMAComponents::exec_sr(uint32 inA, uint32 inB)
{
	return CMA_DWORD_MASK & (inA >> inB);
}

uint32 CMAComponents::exec_sra(uint32 inA, uint32 inB)
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
		return CMAComponents::exec_sr(inA, inB);
	}
}

uint32 CMAComponents::exec_sel(uint32 inA, uint32 inB)
{
	uint32 carryA = inA & CMA_CARRY_MASK;
	if (carryA == 0) {
		return inA;
	} else {
		return inB;
	}
}

uint32 CMAComponents::exec_cat(uint32 inA, uint32 inB)
{
	return inA;
}

uint32 CMAComponents::exec_not(uint32 inA, uint32 inB)
{
	return ~inA;
}

uint32 CMAComponents::exec_and(uint32 inA, uint32 inB)
{
	return inA & inB;
}

uint32 CMAComponents::exec_or(uint32 inA, uint32 inB)
{
	return inA | inB;
}

uint32 CMAComponents::exec_xor(uint32 inA, uint32 inB)
{
	return inA ^ inB;
}

uint32 CMAComponents::exec_eql(uint32 inA, uint32 inB)
{
	uint32 tmpA = inA & CMA_DATA_MASK;
	uint32 tmpB = inB & CMA_DATA_MASK;
	if (tmpA == tmpB) {
		return (1 << CMA_CARYY_LSB) | tmpA;
	} else {
		return 0;
	}
}

uint32 CMAComponents::exec_gt(uint32 inA, uint32 inB)
{
	//exec inA > inB
	uint32 tmpA = inA & CMA_DATA_MASK;
	bool carryA = (inA & CMA_CARRY_MASK) != 0;
	uint32 tmpB = inB & CMA_DATA_MASK;
	bool carryB = (inB & CMA_CARRY_MASK) != 0;

	if (((not carryA & not carryB) & (tmpA > tmpB)) | 
		(not carryA & carryB) |
		((carryA & carryB) & (tmpA < tmpB))) {
		return 1 << CMA_CARYY_LSB;
	} else {
		return 0;
	}

}

uint32 CMAComponents::exec_lt(uint32 inA, uint32 inB)
{
	//exec inA <= inB
	uint32 tmp = CMAComponents::exec_gt(inA, inB);
	if (tmp != 0) {
		return 0;
	} else {
		return 1 << CMA_CARYY_LSB;
	}
}

