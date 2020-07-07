#include "snacccore.h"
#include "snaccmodules.h"

#include "vmips.h"
#include "options.h"

using namespace SNACCComponents;

SNACCCore::SNACCCore(int core_id_,
					DoubleBuffer *dmem_u_,
					DoubleBuffer *dmem_l_,
					DoubleBuffer *rbuf_u_,
					DoubleBuffer *rbuf_l_,
					DoubleBuffer *lut_,
					DoubleBuffer *imem_,
					DoubleBuffer *wbuf_,
					WbufArb *wbuf_arb_) : core_id(core_id_),
	dmem_u(dmem_u_), dmem_l(dmem_l_), rbuf_u(rbuf_u_),
	rbuf_l(rbuf_l_), lut(lut_), imem(imem_), wbuf(wbuf_),
	wbuf_arb(wbuf_arb_), inst_dump(false)
{
	dbg_msg = machine->opt->option("excmsg")->flag;
	mad_unit = new MadUnit(machine->opt->option("snacc_sram_latency")->num,
							dmem_u, dmem_l, rbuf_u, rbuf_l, lut,
							&regs[SNACC_REG_TR0_INDEX],
							&regs[SNACC_REG_TR1_INDEX],
							&regs[SNACC_REG_FR0_INDEX],
							&regs[SNACC_REG_FR1_INDEX]);
}

void SNACCCore::step()
{
	if (!isStall()) {
		switch (status) {
			case SNACC_CORE_STAT_IF:
				if_stage();
				status = SNACC_CORE_STAT_ID;
				break;
			case SNACC_CORE_STAT_ID:
				id_stage();
				status = SNACC_CORE_STAT_EX;
				break;
			case SNACC_CORE_STAT_EX:
				ex_stage();
				status = SNACC_CORE_STAT_WB;
				break;
			case SNACC_CORE_STAT_WB:
				wb_stage();
				status = SNACC_CORE_STAT_IF;
				if (halt_issued) {
					done = true;
					halt_issued = false;
				}
				break;
		}
	} else {
		stall_handle();
	}

	mad_unit->step();

}

bool SNACCCore::isStall()
{
	return done || (stall_cause != SNACC_CORE_NO_STALL);
}

void SNACCCore::if_stage()
{
	fetch_instr = imem->fetch_half_from_inner(pc ^ 0x3);

	// if (core_id == 0) {
	// 	fprintf(stderr, "\n%08d: PC: %X instr %X\n", machine->num_cycles, pc, fetch_instr);
	// 	fprintf(stderr, "outsize %X\n", imem->fetch_word(pc, DATALOAD, NULL));
	// }
}

void SNACCCore::id_stage()
{
	//decode
	dec_opcode = opcode(fetch_instr);
	dec_rd = reg_dst(fetch_instr);
	dec_rs = reg_src(fetch_instr);
	dec_func = func(fetch_instr);

	switch (dec_opcode) {
		case SNACC_CORE_OPCODE_BNEQ:
		case SNACC_CORE_OPCODE_JUMP:
			//sign extension
			dec_imm = (int8)imm(fetch_instr);
			break;
		default:
			dec_imm = imm(fetch_instr);
			break;
	}

	//address calc for mem access
	if (dec_opcode == SNACC_CORE_OPCODE_RTYPE1) {
		switch (dec_func) {
			case SNACC_CORE_FUNC_LW:
			case SNACC_CORE_FUNC_SW:
			case SNACC_CORE_FUNC_LH:
			case SNACC_CORE_FUNC_SH:
				int addr = regs[dec_rs];
				if (addr >= SNACC_IMEM_INTERNAL_ADDR
				  & addr < SNACC_IMEM_INTERNAL_ADDR + SNACC_IMEM_SIZE) {
					access_address = addr - SNACC_IMEM_INTERNAL_ADDR;
					access_mem = imem;
				} else if (addr >= SNACC_DMEM_INTERNAL_ADDR
				  & addr < SNACC_DMEM_INTERNAL_ADDR + SNACC_DMEM_SIZE * 2) {
					access_address = addr - SNACC_DMEM_INTERNAL_ADDR;
					access_address = ((access_address >> 3) << 2)
										+ (access_address & 0x3);
					if ((addr & 0x4) == 0) {
						access_mem = dmem_l;
					} else {
						access_mem = dmem_u;
					}
				} else if (addr >= SNACC_RBUF_INTERNAL_ADDR
				  & addr < SNACC_RBUF_INTERNAL_ADDR + SNACC_RBUF_SIZE * 2) {
					access_address = addr - SNACC_RBUF_INTERNAL_ADDR;
					access_address = ((access_address >> 3) << 2)
										+ (access_address & 0x3);
					if ((addr & 0x4) == 0) {
						access_mem = rbuf_l;
					} else {
						access_mem = rbuf_u;
					}
				} else if (addr >= SNACC_LUT_INTERNAL_ADDR
				  & addr < SNACC_LUT_INTERNAL_ADDR + SNACC_LUT_SIZE) {
					access_address = addr - SNACC_LUT_INTERNAL_ADDR;
					access_mem = lut;
				} else if (addr >= SNACC_WBUF_INTERNAL_ADDR
				  & addr < SNACC_WBUF_INTERNAL_ADDR + SNACC_WBUF_SIZE) {
					access_address = addr - SNACC_WBUF_INTERNAL_ADDR;
					access_mem = wbuf;
					//set stall speculatively
					stall_cause = SNACC_CORE_WBUF_STALL;
				} else {
					if (dbg_msg) {
						fprintf(stderr, "Internal mem access exceed"
								" mapped region (0x%08X)\n", addr);
					}
					access_mem = NULL;
				}
			break;
		}
	} else {
		access_mem = NULL;
	}

	stall_handle();
	//fprintf(stderr, "opcode %d reg write %d\n", dec_opcode, reg_write);
}

void SNACCCore::stall_handle()
{
	//stall check
	switch (stall_cause) {
		case SNACC_CORE_NO_STALL:
			break;
		case SNACC_CORE_WBUF_STALL:
			switch (dec_func) {
				case SNACC_CORE_FUNC_LW:
				case SNACC_CORE_FUNC_LH:
					if (access_mem == wbuf && !wbuf_arb->isAcquired(core_id,
						 wbuf_arb_mode, SNACC_WBUF_LOAD)) {
						stall_cause = SNACC_CORE_WBUF_STALL;
					} else {
						stall_cause = SNACC_CORE_NO_STALL;
					}
					break;
				case SNACC_CORE_FUNC_SW:
				case SNACC_CORE_FUNC_SH:
					if (access_mem == wbuf && !wbuf_arb->isAcquired(core_id,
						 wbuf_arb_mode, SNACC_WBUF_STORE)) {
						stall_cause = SNACC_CORE_WBUF_STALL;
					} else {
						stall_cause = SNACC_CORE_NO_STALL;
					}
					break;
				default:
					stall_cause = SNACC_CORE_NO_STALL;
			}
			break;
		case SNACC_CORE_MAD_STALL:
			if (!mad_unit->running()) {
				stall_cause = SNACC_CORE_NO_STALL;
			}
			break;
		default:
			stall_cause = SNACC_CORE_NO_STALL;
	}
}

void SNACCCore::ex_stage()
{
	(this->*kOpcodeTable[dec_opcode])();
}

void SNACCCore::wb_stage()
{
	if (inst_dump) {
		disassemble();
	}

	if (reg_write & (dec_rd != SNACC_REG_PC_INDEX)) {
		regs[dec_rd] = reg_write_data;
		//fprintf(stderr, "regs[%d] <= %X\n", dec_rd, reg_write_data);
	}

	if (reg_write & (dec_rd == SNACC_REG_PC_INDEX)) {
		pc = reg_write_data;
	} else {
		pc += 2;
		if (isBranch) {
			pc += 2 * dec_imm;
		}
	}
	//reset status
	isBranch = false;
	reg_write = false;

}

void SNACCCore::disassemble()
{
	fprintf(stderr, "%d:\tSNACC\t PC: %5d\t", machine->num_cycles, pc);
	switch(dec_opcode) {
		case SNACC_CORE_OPCODE_RTYPE0:
			fprintf(stderr, RType0InstrFormat[dec_func], dec_rd, dec_rs);
			break;
		case SNACC_CORE_OPCODE_RTYPE1:
			fprintf(stderr, RType1InstrFormat[dec_func], dec_rd, dec_rs);
			break;
		case SNACC_CORE_OPCODE_RTYPE2:
			fprintf(stderr, RType2InstrFormat[dec_func], dec_rd, dec_rs);
			break;
		case SNACC_CORE_OPCODE_JUMP:
			fprintf(stderr, InstrFormat[dec_func], dec_imm);
			break;
		default:
			fprintf(stderr, InstrFormat[dec_opcode], dec_rd, dec_imm);
	}
	fprintf(stderr, "\n");
}


void SNACCCore::reset()
{
	pc = 0;
	for (int i = 0; i < SNACC_REG_SIZE; i++) {
		regs[i] = 0;
	}
	done = false;
	status = SNACC_CORE_STAT_IF;
	reg_write = isBranch = halt_issued = false;
	mad_mode = access_mode = wbuf_arb_mode = 
	dmem_step = rbuf_step = 0;
	fp_pos = 4;
	for (int i = 0; i < SNACC_SIMD_LANE_SIZE; i++) {
		simd_mask[i] = true;
	}

	mad_unit->reset();
}

const SNACCCore::MemberFuncPtr SNACCCore::kOpcodeTable[16] = {
		&SNACCCore::RTypeArithmetic, &SNACCCore::RTypeMemory,
		&SNACCCore::RTypeSimd, &SNACCCore::Loadi,
		&SNACCCore::Bneq, &SNACCCore::Jump,
		&SNACCCore::Mad, &SNACCCore::Madlp,
		&SNACCCore::Setcr, &SNACCCore::Addi,
		&SNACCCore::Subi, &SNACCCore::Sll,
		&SNACCCore::Srl, &SNACCCore::Sra,
		&SNACCCore::Unknown, &SNACCCore::Unknown };


const SNACCCore::MemberFuncPtr SNACCCore::kRTypeArithmeticTable[16] = {
		&SNACCCore::Nop, &SNACCCore::Mov,
		&SNACCCore::Add, &SNACCCore::Sub,
		&SNACCCore::Mul, &SNACCCore::And,
		&SNACCCore::Or, &SNACCCore::Xor,
		&SNACCCore::Neg, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Unknown };

const SNACCCore::MemberFuncPtr SNACCCore::kRTypeMemoryTable[16] = {
		&SNACCCore::Halt, &SNACCCore::Loadw,
		&SNACCCore::Storew, &SNACCCore::Loadh,
		&SNACCCore::Storeh, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Readcr,
		&SNACCCore::Unknown, &SNACCCore::Unknown,
		&SNACCCore::Dbchange, &SNACCCore::Dma,
		&SNACCCore::Unknown, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Unknown };

const SNACCCore::MemberFuncPtr SNACCCore::kRTypeSimdTable[16] = {
		&SNACCCore::Nop, &SNACCCore::Loadv,
		&SNACCCore::Unknown, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Unknown,
		&SNACCCore::Unknown, &SNACCCore::Unknown };

void SNACCCore::Unknown() {
	
	if (dbg_msg) {
  		fprintf(stderr, 
			"SNACCCore[%d]::Execute(): Unknown Instruction: "
			"opcode = %d rd = %d rs = %d funct = %d pc = 0x%X\n",
			core_id, dec_opcode, dec_rd, dec_rs, dec_func, pc);
	}
}

void SNACCCore::RTypeMemory() {
	(this->*kRTypeMemoryTable[dec_func])();
}


void SNACCCore::RTypeArithmetic() {
	(this->*kRTypeArithmeticTable[dec_func])();
}

void SNACCCore::RTypeSimd() {
	(this->*kRTypeSimdTable[dec_func])();
}

void SNACCCore::Loadi() {
	reg_write_data = ((regs[dec_rd] & 0x00FFFFFF) << 8) + dec_imm;
	reg_write = true;
}

void SNACCCore::Bneq() {
	if (regs[dec_rd] != 0) {
		isBranch = true;
	}
}

void SNACCCore::Jump() {
	isBranch = true;
}

void SNACCCore::Mad() {
	mad_unit->start(dec_imm, simd_mask, dmem_step, rbuf_step,
						 mad_mode == 0xF);
	stall_cause = SNACC_CORE_MAD_STALL;
}

void SNACCCore::Madlp() {
	mad_unit->start(dec_imm, simd_mask, dmem_step, rbuf_step,
					 mad_mode == 0xF, regs[dec_rd]);
	stall_cause = SNACC_CORE_MAD_STALL;
}

void SNACCCore::Setcr() {
	switch (dec_rd) {
		case SNACC_CREG_MASK_INDEX:
			for (int i = 0; i < SNACC_SIMD_LANE_SIZE; i++) {
				simd_mask[i] = (dec_imm & (0x1 << i)) != 0;
			}
			break;
		case SNACC_CREG_DSTEP_INDEX:
			dmem_step = (int)dec_imm;
			break;
		case SNACC_CREG_RSTEP_INDEX:
			rbuf_step = (int)dec_imm;
			break;
		case SNACC_CREG_FPPOS_INDEX:
			fp_pos = dec_imm;
			break;
		case SNACC_CREG_WBUFARB_INDEX:
			wbuf_arb_mode = dec_imm;
			break;
		case SNACC_CREG_MODES_INDEX:
			mad_mode = dec_imm & SNACC_CREG_MADMODE_MASK;
			access_mode = (dec_imm & SNACC_CREG_ACCESSMODE_MASK)
							>> SNACC_CREG_ACCESSMODE_LSB;
			break;
	}
}

void SNACCCore::Addi() {
	reg_write_data = regs[dec_rd] + dec_imm;
	reg_write = true;
}

void SNACCCore::Subi() {
	reg_write_data = regs[dec_rd] - dec_imm;
	reg_write = true;
}

void SNACCCore::Sll() {
	reg_write_data = regs[dec_rd] << dec_imm;
	reg_write = true;
}

void SNACCCore::Srl() {
	reg_write_data = regs[dec_rd] >> dec_imm;
	reg_write = true;
}

void SNACCCore::Sra() {
	reg_write_data = static_cast<int>(regs[dec_rd]) >> dec_imm;
	reg_write = true;
}

void SNACCCore::Nop() {
  // Nothing to do.
}

void SNACCCore::Mov() {
	reg_write_data = regs[dec_rs];
	reg_write = true;
}

void SNACCCore::Add() {
	reg_write_data = regs[dec_rd] + regs[dec_rs];
	reg_write = true;
}

void SNACCCore::Sub() {
	reg_write_data = regs[dec_rd] - regs[dec_rs];
	reg_write = true;
}

void SNACCCore::Mul() {
	reg_write_data = regs[dec_rd] * regs[dec_rs];
	reg_write = true;
}

void SNACCCore::And() {
	reg_write_data = regs[dec_rd] & regs[dec_rs];
	reg_write = true;
}

void SNACCCore::Or() {
	reg_write_data = regs[dec_rd] | regs[dec_rs];
	reg_write = true;
}

void SNACCCore::Xor() {
	reg_write_data = regs[dec_rd] ^ regs[dec_rs];
	reg_write = true;
}

void SNACCCore::Neg() {
	reg_write_data = ~regs[dec_rs];
	reg_write = true;
}


void SNACCCore::Halt() {
	halt_issued = true;
}

void SNACCCore::Loadw() {
	if (access_mem != NULL) {
		reg_write_data = access_mem->fetch_word_from_inner(access_address);
	}
	reg_write = true;
}

void SNACCCore::Storew() {
	if (access_mem != NULL) {
		access_mem->store_word_from_inner(access_address, regs[dec_rd]);
	}
}

void SNACCCore::Loadh() {
	uint16 half_data;
	if (access_mem != NULL) {
		//load like Big Endian
		half_data = access_mem->fetch_half_from_inner(access_address ^ 0x2);
		if (access_mode == 0) {
			// Upper mode
			reg_write_data = (uint32)half_data << 16;
		} else {
			// Lower mode
			reg_write_data = (uint32)half_data;
		}
	}
	reg_write = true;
}

void SNACCCore::Storeh() {
	if (access_mem != NULL) {
		//Store like Big Endian
		uint16 half_data;
		if (access_mode == 0) {
			// Upper mode
			half_data = (uint16)((regs[dec_rd] >> 16) & 0xFFFF);
		} else {
			// Lower mode
			half_data = (uint16)(regs[dec_rd] & 0xFFFF);
		}
		access_mem->store_half_from_inner(access_address ^ 0x2, half_data);
	}
}

void SNACCCore::Readcr() {
	switch (dec_rs) {
		case SNACC_CREG_ID_INDEX:
			reg_write_data = core_id;
			break;
		case SNACC_CREG_MASK_INDEX:
			reg_write_data = 0;
			for (int i = 0; i < SNACC_SIMD_LANE_SIZE; i++) {
				if (simd_mask[i]) {
					reg_write_data += (0x1 << i);
				}
			}
			break;
		case SNACC_CREG_DSTEP_INDEX:
			reg_write_data = dmem_step;
			break;
		case SNACC_CREG_RSTEP_INDEX:
			reg_write_data = rbuf_step;
			break;
		case SNACC_CREG_FPPOS_INDEX:
			reg_write_data = fp_pos;
			break;
		case SNACC_CREG_WBUFARB_INDEX:
			reg_write_data = wbuf_arb_mode;
			break;
		case SNACC_CREG_MODES_INDEX:
			reg_write_data =  mad_mode +
								(access_mode << SNACC_CREG_ACCESSMODE_LSB);
			break;
		default:
			reg_write_data = 0;
	}
	reg_write = true;
}

void SNACCCore::Dbchange() {
	fprintf(stderr, "Dbchange is not implemented\n");
}

void SNACCCore::Dma() {
	fprintf(stderr, "Dma is not implemented\n");
}

void SNACCCore::Loadv() {
	mad_unit->set_addr(regs[dec_rd], regs[dec_rs]);
}

