#include "snacccore.h"
#include "snaccmodules.h"

#include "vmips.h"
#include "options.h"

SNACCCore::SNACCCore(int core_id_,
					DoubleBuffer *dmem_u_,
					DoubleBuffer *dmem_l_,
					DoubleBuffer *rbuf_u_,
					DoubleBuffer *rbuf_l_,
					DoubleBuffer *lut_,
					DoubleBuffer *imem_,
					DoubleBuffer *wbuf_) : core_id(core_id_),
	dmem_u(dmem_u_), dmem_l(dmem_l_), rbuf_u(rbuf_u_),
	rbuf_l(rbuf_l_), lut(lut_), imem(imem_), wbuf(wbuf_)
{

}

void SNACCCore::step()
{

	if (!stall) {
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
					stall = true;
				}
				break;
		}
	}

}

void SNACCCore::if_stage()
{
	if (pc % 4 == 0) {
		fetch_instr_x2 = imem->fetch_word_from_inner(pc);
		fetch_instr = (fetch_instr_x2 >> 16) & 0x0FFFF;
	} else {
		fetch_instr = fetch_instr_x2 & 0x0FFFF;
	}
	fprintf(stderr, "\n%08d: PC: %X instr %X\n", machine->num_cycles, pc, fetch_instr);
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

	//fprintf(stderr, "opcode %d reg write %d\n", dec_opcode, reg_write);
}

void SNACCCore::ex_stage()
{
	(this->*kOpcodeTable[dec_opcode])();
}

void SNACCCore::wb_stage()
{
	if (reg_write & (dec_rd != SNACC_REG_PC_INDEX)) {
		regs[dec_rd] = reg_write_data;
		fprintf(stderr, "regs[%d] <= %X\n", dec_rd, reg_write_data);
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


void SNACCCore::reset()
{
	pc = 0;
	for (int i = 0; i < SNACC_REG_SIZE; i++) {
		regs[i] = 0;
	}
	done = false;
	status = SNACC_CORE_STAT_IF;
	reg_write = isBranch = stall = halt_issued = false;
	mad_mode = access_mode = wbuf_arb = 
	dmem_step = rbuf_step = 0;
	fp_pos = 4;
	simd_mask = 0xFF;
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
	if (machine->opt->option("excmsg")->flag) {
  		fprintf(stderr, 
			"SNACCCore::Execute(): Unknown Instruction: "
			"opcode = %d rd = %d rs = %d funct = %d\n",
			dec_opcode, dec_rd, dec_rs, dec_func);
	}
}

void SNACCCore::RTypeMemory() {
	(this->*kRTypeMemoryTable[dec_func])();
}


void SNACCCore::RTypeArithmetic() {
	(this->*kRTypeArithmeticTable[dec_func])();
}

void SNACCCore::RTypeSimd() {
	// (this->*kRTypeSimdTable[funct_])();
	fprintf(stderr, "RTypeSimd not impl\n");
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

const int kLutOff  = 0;
const int kLutOn   = 1;
const int kMaxPool = 2;

// void SNACCCore::DoMad(Fixed32* tr0, Fixed32* tr1) {
//   assert((imm_ == kLutOff || imm_ == kLutOn || imm_ == kMaxPool) &&
//          "Invalid MAD imm parameter!");

//   for (int i = 0; i < 4; ++i) {
//     // This element is masked.
//     const int mask = ctrl_regs[1];
//     if ((mask & (1 << i)) == 0) {
//       continue;
//     }

//     Fixed16 data_value0;
//     Fixed16 rbuf_value0;
//     Fixed16 data_value1;
//     Fixed16 rbuf_value1;

//     const bool wide_mode = (ctrl_regs[6] & 0x0F) == 0;
//     if (wide_mode) {
//       data_value0 = Fixed16(ReadMemory16(data_address_ + 2 * i));
//       rbuf_value0 = Fixed16(ReadMemory16(rbuf_address_ + 2 * i));
//     } else {
//       data_value0 = Fixed16(ReadMemory8(data_address_ + i));
//       rbuf_value0 = Fixed16(ReadMemory8(rbuf_address_ + i));
//       data_value1 = Fixed16(ReadMemory8(data_address_ + i + 4));
//       rbuf_value1 = Fixed16(ReadMemory8(rbuf_address_ + i + 4));
//     }

//     if (imm_ == kLutOff || imm_ == kLutOn) {
//       *tr0 = *tr0 + data_value0 * rbuf_value0;
//       *tr1 = *tr1 + data_value1 * rbuf_value1;
//     } else if (imm_ == kMaxPool) {
//       *tr0 = std::max(*tr0, data_value0.ToFixed32());
//       *tr1 = std::max(*tr1, data_value1.ToFixed32());
//     }
//   }
// }

// void SNACCCore::DoMadApplyToRegisters(const Fixed32& tr0, const Fixed32& tr1) {
//   assert((imm_ == kLutOff || imm_ == kLutOn || imm_ == kMaxPool) &&
//          "Invalid MAD imm parameter!");

//   const bool wide_mode = (ctrl_regs[6] & 0x0F) == 0;

//   regs[13] = tr0.ToRegisterFormat();
//   if (!wide_mode) {
//     regs[14] = tr1.ToRegisterFormat();
//   }

//   if (imm_ == kLutOn) {
//     Fixed16 fr0(ReadMemory16(tr0.ToLutIndex() + 0x07000000));
//     Fixed16 fr1(ReadMemory16(tr1.ToLutIndex() + 0x07000000));
//     regs[11] = fr0.ToRegisterFormat();
//     if (!wide_mode) {
//       regs[12] = fr1.ToRegisterFormat();
//     }
//   }
// }

void SNACCCore::Mad() {
  // rd_ is ignored for this instruction.

  // FRs are TRs that are applied LUT.
  // TR0 = r13, TR1 = r14, FR0 = r11, FR1 = r12

  // For MADLP with imm_ == kLutOff and kLutOn, iterative results are
  // accumulated.
  // For imm_ == kMaxPool, Max pooling is performed with existing TR values.

  // They are the signed fixed point decimals but decimal point location varies.
  // This problem is dealt inside Fixed16 and Fixed32 classes.

//   Fixed32 tr0 = Fixed32::FromRegisterFormat(regs[13]);
//   Fixed32 tr1 = Fixed32::FromRegisterFormat(regs[14]);
//   DoMad(&tr0, &tr1);
//   DoMadApplyToRegisters(tr0, tr1);
}

// void SNACCCore::DoMadLoop() {
//   if (mad_loop_remaining_ > 0) {
//     DoMad(&tr0_loop_, &tr1_loop_);

//     data_address_ += ctrl_regs[2];
//     rbuf_address_ += ctrl_regs[3];

//     --mad_loop_remaining_;
//     if (mad_loop_remaining_ == 0) {
//       DoMadApplyToRegisters(tr0_loop_, tr1_loop_);
//     }
//   }
// }

void SNACCCore::Madlp() {
  // Do one internal loop per one Execute() call rather than running all
//   // at one call.
//   mad_loop_remaining_ = regs[rd_];
//   tr0_loop_ = Fixed32::FromRegisterFormat(regs[13]);
//   tr1_loop_ = Fixed32::FromRegisterFormat(regs[14]);

//   DoMadLoop();
}

void SNACCCore::Setcr() {
	switch (dec_rd) {
		case SNACC_CREG_MASK_INDEX:
			simd_mask = dec_imm;
			break;
		case SNACC_CREG_DSTEP_INDEX:
			dmem_step = dec_imm;
			break;
		case SNACC_CREG_RSTEP_INDEX:
			rbuf_step = dec_imm;
			break;
		case SNACC_CREG_FPPOS_INDEX:
			fp_pos = dec_imm;
			break;
		case SNACC_CREG_WBUFARB_INDEX:
			wbuf_arb = dec_imm;
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
	//regs[rd_] = ReadMemory32(regs[rs_]);
	reg_write = true;
}

void SNACCCore::Storew() {
	//WriteMemory32(regs[rs_], regs[rd_]);
}

void SNACCCore::Loadh() {
	if (access_mode == 0) {
		// Upper mode
		// regs[rd_] =
		// static_cast<uint32_t>(ReadMemory16(regs[rs_])) << 16;
	} else {
		// Lower mode
		// regs[rd_] =
		// static_cast<uint32_t>(ReadMemory16(regs[rs_]));
	}
	reg_write = true;
}

void SNACCCore::Storeh() {
	// // fprintf(stderr, "Storeh [%x] = %x\n", regs[rs_], regs[rd_]);
	// if (access_mode == 0) {
	// // Upper mode
	// WriteMemory16(regs[rs_], (regs[rd_] & 0xFFFF0000) >> 16);
	// } else {
	// // Lower mode
	// WriteMemory16(regs[rs_], regs[rd_] & 0xFFFF);
	// }
}

void SNACCCore::Readcr() {
	switch (dec_rs) {
		case SNACC_CREG_ID_INDEX:
			reg_write_data = core_id;
			break;
		case SNACC_CREG_MASK_INDEX:
			reg_write_data = simd_mask;
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
			reg_write_data = wbuf_arb;
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
	assert(false && "Unimplemented!!");
}

void SNACCCore::Dma() {
	assert(false && "Unimplemented!!");
}

void SNACCCore::Loadv() {
  // Contrary to its name, the instruction just copies
  // rd and rs to data_address and rbuf_address, respectively.
//   data_address_ = regs[rd_];
//   rbuf_address_ = regs[rs_];
}

// uint8_t SNACCCore::ReadMemory8(uint32_t address) {
	// if (address < kInstBase + inst_size_) {
	// return inst_[address - kInstBase];
	// }
	// if (kDataBase <= address && address < kDataBase + data_size_) {
	// return data_[address - kDataBase];
	// }
	// if (kRbufBase <= address && address < kRbufBase + rbuf_size_) {
	// return rbuf_[address - kRbufBase];
	// }
	// if (kLutBase <= address && address < kLutBase + lut_size_) {
	// return lut_[address - kLutBase];
	// }
	// if (kWbufBase <= address && address < kWbufBase + wbuf_size_) {
	// return wbuf_[address - kWbufBase];
	// }
	// fprintf(stderr, "Read invalid address %x\n", address);
	// assert(false && "Invalid address");
	// return 0;
// }

// void SNACCCore::WriteMemory8(uint32_t address, uint8_t value) {
	// if (address < kInstBase + inst_size_) {
	// assert(false && "Write to instruction memory is unlikely to happen");
	// inst_[address - kInstBase] = value;
	// return;
	// }
	// if (kDataBase <= address && address < kDataBase + data_size_) {
	// data_[address - kDataBase] = value;
	// return;
	// }
	// if (kRbufBase <= address && address < kRbufBase + rbuf_size_) {
	// rbuf_[address - kRbufBase] = value;
	// return;
	// }
	// if (kLutBase <= address && address < kLutBase + lut_size_) {
	// lut_[address - kLutBase] = value;
	// return;
	// }
	// if (kWbufBase <= address && address < kWbufBase + wbuf_size_) {
	// wbuf_[address - kWbufBase] = value;
	// return;
	// }
	// fprintf(stderr, "Write invalid address [%x] = %x\n", address, value);
	// assert(false && "Invalid address");
// }
