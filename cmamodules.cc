#include "cmamodules.h"

#define CMA_COUNT 1

using namespace CMAComponents;

void CMAMemoryModule::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	uint32 *werd;
	/* calculate address */
	werd = ((uint32 *) address) + (offset / 4);
	/* store word */
	*werd = data & mask;
}


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


PEArray::PEArray(int height_, int width_, bool preg_en,
					int se_count_, int se_channels_) :
	height(height_), width(width_), preg_enabled(preg_en),
	se_count(se_count_), se_channels(se_channels_)
{
	make_ALUs();
	make_SEs();
	make_const_regs();
	if (preg_enabled) {
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
	pregs = new PREG*[height - 1];
	for (int i = 0; i < height - 1; i++) {
		pregs[i] = new PREG();
		//Debug
		debug_str[pregs[i]] = std::string("PREG_") + std::to_string(i);
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
	for (int i = 0; i < (height - 1); i++) {
		if (((data >> i) & 0x1) != 0) {
			pregs[i]->activate();
		} else {
			pregs[i]->deactivate();
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
	if (preg_enabled) {
		for (int y = 0; y < height - 1; y++) {
			in_degrees[pregs[y]] = pregs[y]->in_degree();
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
					fprintf(stderr, "current node %s\n", debug_str[p].c_str());
					tsorted_list.push_back(p);
					for (auto j = list.begin(); j != list.end(); j++) {
						 fprintf(stderr, "next is %s %d\n", debug_str[*j].c_str(), in_degrees[*j]);
						(in_degrees[*j])--;
					}
				} else {
					fprintf(stderr, "node %s not used\n", debug_str[p].c_str());
				}
			} 
		}
		//fprintf(stderr, "del nodes %d\n", del_list.size());
		for (auto i = del_list.begin(); i != del_list.end(); i++) {
			in_degrees.erase(*i);
		}
	} while (in_degrees.size() < prev_size);

	launch_regs[0]->store(0x0f0f0f);
	launch_regs[1]->store(0x0f0f0f);
	for (auto i = tsorted_list.begin(); i != tsorted_list.end(); i++) {
		PENodeBase *p = *i;
		fprintf(stderr, "%s\n", debug_str[p].c_str());
		p->exec();
	}

	exit(1);
}

DataManipulator::DataManipulator(int interleave_size_) :
	interleave_size(interleave_size_)
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

ArrayData DataManipulator::send(ArrayData *input_data, int table_index)
{
	ArrayData send_data(interleave_size, 0);

	if (table_index < CMA_TABLE_ENTRY_SIZE) {
		bool *use_bitmap = bitmap[table_index];
		int *use_table = table[table_index];
		for (int i = 0; i < interleave_size; i++) {
			if (use_bitmap[i]) {
				send_data[i] = (*input_data)[use_table[i]];
			}
		}
	}
	return send_data;
}

void CCSOTB2::CCSOTB2_PEArray::make_connection()
{
	using namespace CCSOTB2;
	PENodeBase* node_ptr;
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			//SE to ALU
			for (int ch = SE_SOUTH; ch <= SE_WEST; ch++) {
				int pred_x = CONNECT_COORD[ch][0] + x;
				int pred_y = CONNECT_COORD[ch][1] + y;
				if ((pred_x >= 0 && pred_x < width) &&
					(pred_y >= 0 && pred_y < height)) {
					alu_sels[x][y][0]->connect(channels[pred_x][pred_y][0][OPPOSITE_CHANNEL[ch]]);
					alu_sels[x][y][1]->connect(channels[pred_x][pred_y][0][OPPOSITE_CHANNEL[ch]]);
				} else if (pred_y == -1) {
					alu_sels[x][y][0]->connect(launch_regs[x]);
					alu_sels[x][y][1]->connect(launch_regs[x]);
				} else {
					alu_sels[x][y][0]->connect(NULL);
					alu_sels[x][y][1]->connect(NULL);
				}
			}
			//ALU to ALU (Direct Link)
			for (int ch = DL_S; ch <= DL_SW; ch++) {
				int pred_x = CONNECT_COORD[ch][0] + x;
				int pred_y = CONNECT_COORD[ch][1] + y;
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
				node_ptr = launch_regs[x];
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
			if (y > 0 && x < width - 1) {
				node_ptr = alus[x+1][y-1];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][SE_NORTH]->connect(node_ptr);
			channels[x][y][0][SE_EAST]->connect(node_ptr);
			channels[x][y][0][SE_WEST]->connect(node_ptr);

			// from southwest (DL)
			if (y > 0 && x > 0) {
				node_ptr = alus[x-1][y-1];
			} else {
				node_ptr = NULL;
			}
			channels[x][y][0][SE_NORTH]->connect(node_ptr);
			channels[x][y][0][SE_EAST]->connect(node_ptr);

			// const to SE
			channels[x][y][0][SE_NORTH]->connect(pregs[y]);
		}
		gather_regs[x]->connect(channels[x][0][0][SE_NORTH]);
	}
}


// BFSQueue::~BFSQueue()
// {
// 	std::queue<PENodeBase*> empty;
// 	std::swap(nodeQueue, empty);
// 	added.clear();
// }

// void BFSQueue::push(PENodeBase* node)
// {
// 	if (added.count(node) == 0) {
// 		nodeQueue.push(node);
// 		added[node] = true;
// 	}
// }

// PENodeBase* BFSQueue::pop()
// {
// 	PENodeBase* ret = nodeQueue.front();
// 	nodeQueue.pop();
// 	return ret;
// }


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
				fprintf(stderr, "%s not used after %s\n", 
					debug_str[successors[i]].c_str(),
					debug_str[this].c_str());
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
	fprintf(stderr, "ALU in %x, %x out %x\n", inA, inB, obuf.front());
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
	obuf.push(predecessors[0]->getData());
}

uint32 MemStoreUnit::load()
{
	return obuf.front();
}

uint32 exec_nop(uint32 inA, uint32 inB)
{
	return 0;
}

uint32 exec_add(uint32 inA, uint32 inB)
{
	return CMA_DWORD_MASK & (inA + inB);
}

uint32 exec_sub(uint32 inA, uint32 inB)
{
	uint32 tmpA, tmpB;
	tmpA = inA & CMA_DWORD_MASK;
	tmpB = ~(inB & CMA_DWORD_MASK) & CMA_DWORD_MASK;
	return (tmpA + tmpB + 1U) & CMA_DATA_MASK;
}

uint32 exec_mult(uint32 inA, uint32 inB)
{
	uint32 tmpA = inA & CMA_DATA_MASK;
	uint32 carryA = inA & CMA_CARRY_MASK;
	uint32 tmpB = inB & CMA_DATA_MASK;
	uint32 carryB = inB & CMA_CARRY_MASK;
	return (carryA ^ carryB) | ((tmpA * tmpB) & CMA_DATA_MASK);
}

uint32 exec_sl(uint32 inA, uint32 inB)
{
	return CMA_DWORD_MASK & (inA << inB);
}

uint32 exec_sr(uint32 inA, uint32 inB)
{
	return CMA_DWORD_MASK & (inA >> inB);
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
	uint32 tmpA = inA & CMA_DATA_MASK;
	uint32 tmpB = inB & CMA_DATA_MASK;
	if (tmpA == tmpB) {
		return (1 << CMA_CARYY_LSB) | tmpA;
	} else {
		return 0;
	}
}

uint32 exec_gt(uint32 inA, uint32 inB)
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

