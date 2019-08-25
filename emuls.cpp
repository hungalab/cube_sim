

/* emulation of instructions */
// void
// CPU::cpzero_emulate(uint32 instr, uint32 pc)
// {
// 	cpzero->cpzero_emulate(instr, pc);
// 	dcache->cache_isolate(cpzero->caches_isolated());
// }

/* Called when the program wants to use coprocessor COPROCNO, and there
 * isn't any implementation for that coprocessor.
 * Results in a Coprocessor Unusable exception, along with an error
 * message being printed if the coprocessor is marked usable in the
 * CP0 Status register.
 */
// void
// CPU::cop_unimpl (int coprocno, uint32 instr, uint32 pc)
// {
// 	if (cpzero->cop_usable (coprocno)) {
// 		/* Since they were expecting this to work, the least we
// 		 * can do is print an error message. */
// 		fprintf (stderr, "CP%d instruction %x not implemented at pc=0x%x:\n",
// 				 coprocno, instr, pc);
// 		machine->disasm->disassemble (pc, instr);
// 		exception (CpU, ANY, coprocno);
// 	} else {
// 		/* It's fair game to just throw an exception, if they
// 		 * haven't even bothered to twiddle the status bits first. */
// 		exception (CpU, ANY, coprocno);
// 	}
// }

// void
// CPU::cpone_emulate(uint32 instr, uint32 pc)
// {
// 	if (opt_fpu) {
// 		// FIXME: check cpzero->cop_usable
// 		fpu->cpone_emulate (instr, pc);
// 	} else {
// 		 /*If it's a cfc1 <reg>, $0 then we copy 0 into reg,
// 		 * which is supposed to mean there is NO cp1... 
// 		 * for now, though, ANYTHING else asked of cp1 results
// 		 * in the default "unimplemented" behavior. */
// 		if (cpzero->cop_usable (1) && rs (instr) == 2
//                     && rd (instr) == 0) {
// 			reg[rt (instr)] = 0; /* No cp1. */
// 		} else {
// 			cop_unimpl (1, instr, pc);
// 		}
//         }
// }

// void
// CPU::cptwo_emulate(uint32 instr, uint32 pc)
// {
// 	cop_unimpl (2, instr, pc);
// }

// void
// CPU::cpthree_emulate(uint32 instr, uint32 pc)
// {
// 	cop_unimpl (3, instr, pc);
//}

// void
// CPU::control_transfer (uint32 new_pc)
// {
// 	if (!new_pc) warning ("Jumping to zero (PC = 0x%x)\n", pc);
// 	delay_state = DELAYING;
//     delay_pc = new_pc;
// }

// /// calc_jump_target - Calculate the address to jump to as a result of
// /// the J-format (jump) instruction INSTR at address PC.  (PC is the address
// /// of the jump instruction, and INSTR is the jump instruction word.)
// ///
// uint32
// CPU::calc_jump_target (uint32 instr, uint32 pc)
// {
//     // Must use address of delay slot (pc + 4) to calculate.
// 	return ((pc + 4) & 0xf0000000) | (jumptarg(instr) << 2);
// }

// void
// CPU::jump(uint32 instr, uint32 pc)
// {
//     control_transfer (calc_jump_target (instr, pc));
// }

// void
// CPU::j_emulate(uint32 instr, uint32 pc)
// {
// 	jump (instr, pc);
// }

// void
// CPU::jal_emulate(uint32 instr, uint32 pc)
// {
//     jump (instr, pc);
// 	// RA gets addr of instr after delay slot (2 words after this one).
// 	reg[reg_ra] = pc + 8;
// }

// /// calc_branch_target - Calculate the address to jump to for the
// /// PC-relative branch for which the offset is specified by the immediate field
// /// of the branch instruction word INSTR, with the program counter equal to PC.
// /// 
// uint32
// CPU::calc_branch_target(uint32 instr, uint32 pc)
// {
// 	return (pc + 4) + (s_immed(instr) << 2);
// }

// void
// CPU::branch(uint32 instr, uint32 pc)
// {
//     control_transfer (calc_branch_target (instr, pc));
// }

void
CPU::branch(uint32 instr, uint32 pc)
{
    fprintf(stderr, "Dont use CPU::branch\n");
}

// void
// CPU::beq_emulate(uint32 instr, uint32 pc)
// {
// 	if (reg[rs(instr)] == reg[rt(instr)])
//         branch (instr, pc);
// }

// void
// CPU::bne_emulate(uint32 instr, uint32 pc)
// {
// 	if (reg[rs(instr)] != reg[rt(instr)])
//         branch (instr, pc);
// }

// void
// CPU::blez_emulate(uint32 instr, uint32 pc)
// {
// 	if (rt(instr) != 0) {
// 		exception(RI);
// 		return;
//     }
// 	if (reg[rs(instr)] == 0 || (reg[rs(instr)] & 0x80000000))
// 		branch(instr, pc);
// }

// void
// CPU::bgtz_emulate(uint32 instr, uint32 pc)
// {
// 	if (rt(instr) != 0) {
// 		exception(RI);
// 		return;
// 	}
// 	if (reg[rs(instr)] != 0 && (reg[rs(instr)] & 0x80000000) == 0)
// 		branch(instr, pc);
// }

// void
// CPU::addi_emulate(uint32 instr, uint32 pc)
// {
// 	int32 a, b, sum;

// 	a = (int32)reg[rs(instr)];
// 	b = s_immed(instr);
// 	sum = a + b;
// 	if ((a < 0 && b < 0 && !(sum < 0)) || (a >= 0 && b >= 0 && !(sum >= 0))) {
// 		exception(Ov);
// 		return;
// 	} else {
// 		reg[rt(instr)] = (uint32)sum;
// 	}
// }

// void
// CPU::addiu_emulate(uint32 instr, uint32 pc)
// {
// 	int32 a, b, sum;

// 	a = (int32)reg[rs(instr)];
// 	b = s_immed(instr);
// 	sum = a + b;
// 	reg[rt(instr)] = (uint32)sum;
// }

// void
// CPU::slti_emulate(uint32 instr, uint32 pc)
// {
// 	int32 s_rs = reg[rs(instr)];

// 	if (s_rs < s_immed(instr)) {
// 		reg[rt(instr)] = 1;
// 	} else {
// 		reg[rt(instr)] = 0;
// 	}
// }

// void
// CPU::sltiu_emulate(uint32 instr, uint32 pc)
// {
// 	if (reg[rs(instr)] < (uint32)(int32)s_immed(instr)) {
// 		reg[rt(instr)] = 1;
// 	} else {
// 		reg[rt(instr)] = 0;
// 	}
// }

// void
// CPU::andi_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rt(instr)] = (reg[rs(instr)] & 0x0ffff) & immed(instr);
// }

// void
// CPU::ori_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rt(instr)] = reg[rs(instr)] | immed(instr);
// }

// void
// CPU::xori_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rt(instr)] = reg[rs(instr)] ^ immed(instr);
// }

// void
// CPU::lui_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rt(instr)] = immed(instr) << 16;
// }

// void
// CPU::lb_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, base;
// 	int8 byte;
// 	int32 offset;
// 	bool cacheable;

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(virt, DATALOAD, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Fetch byte.
// 	 * Because it is assigned to a signed variable (int32 byte)
// 	 * it will be sign-extended.
// 	 */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			byte = icache->fetch_byte(mem, phys, this);
// 		} else {
// 			byte = dcache->fetch_byte(mem, phys, this);
// 		}
// 	} else {
// 		byte = mem->fetch_byte(phys, this);
// 	}

// 	if (exception_pending) return;

// 	/* Load target register with data. */
// 	reg[rt(instr)] = byte;
// }

// void
// CPU::lh_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, base;
// 	int16 halfword;
// 	int32 offset;
// 	bool cacheable;

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;

// 	/* This virtual address must be halfword-aligned. */
// 	if (virt % 2 != 0) {
// 		exception(AdEL,DATALOAD);
// 		return;
// 	}

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(virt, DATALOAD, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Fetch halfword.
// 	 * Because it is assigned to a signed variable (int32 halfword)
// 	 * it will be sign-extended.
// 	 */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			halfword = icache->fetch_halfword(mem, phys, this);
// 		} else {
// 			halfword = dcache->fetch_halfword(mem, phys, this);
// 		}
// 	} else {
// 		halfword = mem->fetch_halfword(phys, this);
// 	}

// 	if (exception_pending) return;

// 	/* Load target register with data. */
// 	reg[rt(instr)] = halfword;
// }

/* The lwr and lwl algorithms here are taken from SPIM 6.0,
 * since I didn't manage to come up with a better way to write them.
 * Improvements are welcome.
 */
uint32
CPU::lwr(uint32 regval, uint32 memval, uint8 offset)
{
	if (opt_bigendian) {
		switch (offset)
		{
			case 0: return (regval & 0xffffff00) |
						((unsigned)(memval & 0xff000000) >> 24);
			case 1: return (regval & 0xffff0000) |
						((unsigned)(memval & 0xffff0000) >> 16);
			case 2: return (regval & 0xff000000) |
						((unsigned)(memval & 0xffffff00) >> 8);
			case 3: return memval;
		}
	} else /* if MIPS target is little endian */ {
		switch (offset)
		{
			/* The SPIM source claims that "The description of the
			 * little-endian case in Kane is totally wrong." The fact
			 * that I ripped off the LWR algorithm from them could be
			 * viewed as a sort of passive assumption that their claim
			 * is correct.
			 */
			case 0: /* 3 in book */
				return memval;
			case 1: /* 0 in book */
				return (regval & 0xff000000) | ((memval & 0xffffff00) >> 8);
			case 2: /* 1 in book */
				return (regval & 0xffff0000) | ((memval & 0xffff0000) >> 16);
			case 3: /* 2 in book */
				return (regval & 0xffffff00) | ((memval & 0xff000000) >> 24);
		}
	}
	fatal_error("Invalid offset %x passed to lwr\n", offset);
}

uint32
CPU::lwl(uint32 regval, uint32 memval, uint8 offset)
{
	if (opt_bigendian) {
		switch (offset)
		{
			case 0: return memval;
			case 1: return (memval & 0xffffff) << 8 | (regval & 0xff);
			case 2: return (memval & 0xffff) << 16 | (regval & 0xffff);
			case 3: return (memval & 0xff) << 24 | (regval & 0xffffff);
		}
	} else /* if MIPS target is little endian */ {
		switch (offset)
		{
			case 0: return (memval & 0xff) << 24 | (regval & 0xffffff);
			case 1: return (memval & 0xffff) << 16 | (regval & 0xffff);
			case 2: return (memval & 0xffffff) << 8 | (regval & 0xff);
			case 3: return memval;
		}
	}
	fatal_error("Invalid offset %x passed to lwl\n", offset);
}

// void
// CPU::lwl_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, wordvirt, base, memword;
// 	uint8 which_byte;
// 	int32 offset;
// 	bool cacheable;

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;
// 	/* We request the word containing the byte-address requested. */
// 	wordvirt = virt & ~0x03UL;

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(wordvirt, DATALOAD, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Fetch word. */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			memword = icache->fetch_word(mem, phys, DATALOAD, this);
// 		} else {
// 			memword = dcache->fetch_word(mem, phys, DATALOAD, this);
// 		}
// 	} else {
// 		memword = mem->fetch_word(phys, DATALOAD, this);
// 	}

// 	if (exception_pending) return;
	
// 	/* Insert bytes into the left side of the register. */
// 	which_byte = virt & 0x03;
// 	reg[rt(instr)] = lwl(reg[rt(instr)], memword, which_byte);
// }

// void
// CPU::lw_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, base, word;
// 	int32 offset;
// 	bool cacheable;

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;

// 	/* This virtual address must be word-aligned. */
// 	if (virt % 4 != 0) {
// 		exception(AdEL,DATALOAD);
// 		return;
// 	}

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(virt, DATALOAD, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Fetch word. */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			word = icache->fetch_word(mem, phys, DATALOAD, this);
// 		} else {
// 			word = dcache->fetch_word(mem, phys, DATALOAD, this);
// 		}
// 	} else {
// 		word = mem->fetch_word(phys, DATALOAD, this);
// 	}

// 	if (exception_pending) return;

// 	/* Load target register with data. */
// 	reg[rt(instr)] = word;
// }

// void
// CPU::lbu_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, base, byte;
// 	int32 offset;
// 	bool cacheable;

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(virt, DATALOAD, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Fetch byte.  */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			byte = icache->fetch_byte(mem, phys, this) & 0x000000ff;
// 		} else {
// 			byte = dcache->fetch_byte(mem, phys, this) & 0x000000ff;
// 		}
// 	} else {
// 		byte = mem->fetch_byte(phys, this) & 0x000000ff;
// 	}

// 	if (exception_pending) return;

// 	/* Load target register with data. */
// 	reg[rt(instr)] = byte;
// }

// void
// CPU::lhu_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, base, halfword;
// 	int32 offset;
// 	bool cacheable;

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;

// 	/* This virtual address must be halfword-aligned. */
// 	if (virt % 2 != 0) {
// 		exception(AdEL,DATALOAD);
// 		return;
// 	}

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(virt, DATALOAD, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Fetch halfword.  */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			halfword = icache->fetch_halfword(mem, phys, this) & 0x0000ffff;
// 		} else {
// 			halfword = dcache->fetch_halfword(mem, phys, this) & 0x0000ffff;
// 		}
		
// 	} else {
// 		halfword = mem->fetch_halfword(phys, this) & 0x0000ffff;
// 	}

// 	if (exception_pending) return;

// 	/* Load target register with data. */
// 	reg[rt(instr)] = halfword;
// }

// void
// CPU::lwr_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, wordvirt, base, memword;
// 	uint8 which_byte;
// 	int32 offset;
// 	bool cacheable;

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;
// 	/* We request the word containing the byte-address requested. */
// 	wordvirt = virt & ~0x03UL;

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(wordvirt, DATALOAD, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Fetch word. */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			memword = icache->fetch_word(mem, phys, DATALOAD, this);
// 		} else {
// 			memword = dcache->fetch_word(mem, phys, DATALOAD, this);
// 		}
// 	} else {
// 		memword = mem->fetch_word(phys, DATALOAD, this);
// 	}

// 	if (exception_pending) return;
	
// 	/* Insert bytes into the left side of the register. */
// 	which_byte = virt & 0x03;
// 	reg[rt(instr)] = lwr(reg[rt(instr)], memword, which_byte);
// }

// void
// CPU::sb_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, base;
// 	uint8 data;
// 	int32 offset;
// 	bool cacheable;

// 	/* Load data from register. */
// 	data = reg[rt(instr)] & 0x0ff;

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(virt, DATASTORE, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Store byte. */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			icache->store_byte(mem, phys, data, this);
// 		} else {
// 			dcache->store_byte(mem, phys, data, this);
// 		}
// 	} else {
// 		mem->store_byte(phys, data, this);
// 	}

// }

// void
// CPU::sh_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, base;
// 	uint16 data;
// 	int32 offset;
// 	bool cacheable;

// 	/* Load data from register. */
// 	data = reg[rt(instr)] & 0x0ffff;

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;

// 	/* This virtual address must be halfword-aligned. */
// 	if (virt % 2 != 0) {
// 		exception(AdES,DATASTORE);
// 		return;
// 	}

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(virt, DATASTORE, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Store halfword. */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			icache->store_halfword(mem, phys, data, this);
// 		} else {
// 			dcache->store_halfword(mem, phys, data, this);
// 		}
// 	} else {
// 		mem->store_halfword(phys, data, this);
// 	}

// }

uint32
CPU::swl(uint32 regval, uint32 memval, uint8 offset)
{
	if (opt_bigendian) {
		switch (offset) {
			case 0: return regval; 
			case 1: return (memval & 0xff000000) | (regval >> 8 & 0xffffff); 
			case 2: return (memval & 0xffff0000) | (regval >> 16 & 0xffff); 
			case 3: return (memval & 0xffffff00) | (regval >> 24 & 0xff); 
		}
	} else /* if MIPS target is little endian */ {
		switch (offset) {
			case 0: return (memval & 0xffffff00) | (regval >> 24 & 0xff); 
			case 1: return (memval & 0xffff0000) | (regval >> 16 & 0xffff); 
			case 2: return (memval & 0xff000000) | (regval >> 8 & 0xffffff); 
			case 3: return regval; 
		}
	}
	fatal_error("Invalid offset %x passed to swl\n", offset);
}

uint32
CPU::swr(uint32 regval, uint32 memval, uint8 offset)
{
	if (opt_bigendian) {
		switch (offset) {
			case 0: return ((regval << 24) & 0xff000000) | (memval & 0xffffff); 
			case 1: return ((regval << 16) & 0xffff0000) | (memval & 0xffff); 
			case 2: return ((regval << 8) & 0xffffff00) | (memval & 0xff); 
			case 3: return regval; 
		}
	} else /* if MIPS target is little endian */ {
		switch (offset) {
			case 0: return regval; 
			case 1: return ((regval << 8) & 0xffffff00) | (memval & 0xff); 
			case 2: return ((regval << 16) & 0xffff0000) | (memval & 0xffff); 
			case 3: return ((regval << 24) & 0xff000000) | (memval & 0xffffff); 
		}
	}
	fatal_error("Invalid offset %x passed to swr\n", offset);
}

// void
// CPU::swl_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, wordvirt, base, regdata, memdata;
// 	int32 offset;
// 	uint8 which_byte;
// 	bool cacheable;

// 	/* Load data from register. */
// 	regdata = reg[rt(instr)];

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;
// 	/* We request the word containing the byte-address requested. */
// 	wordvirt = virt & ~0x03UL;

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(wordvirt, DATASTORE, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Read data from memory. */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			memdata = icache->fetch_word(mem, phys, DATASTORE, this);
// 		} else {
// 			memdata = dcache->fetch_word(mem, phys, DATASTORE, this);
// 		}
// 	} else {
// 		memdata = mem->fetch_word(phys, DATASTORE, this);
// 	}

// 	if (exception_pending) return;

// 	/* Write back the left side of the register. */
// 	which_byte = virt & 0x03UL;
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			icache->store_word(mem, phys, swl(regdata, memdata, which_byte), this);
// 		} else {
// 			dcache->store_word(mem, phys, swl(regdata, memdata, which_byte), this);
// 		}
// 	} else {
// 		mem->store_word(phys, swl(regdata, memdata, which_byte), this);
// 	}

// }

// void
// CPU::sw_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, base, data;
// 	int32 offset;
// 	bool cacheable;

// 	/* Load data from register. */
// 	data = reg[rt(instr)];

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;

// 	/* This virtual address must be word-aligned. */
// 	if (virt % 4 != 0) {
// 		exception(AdES,DATASTORE);
// 		return;
// 	}

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(virt, DATASTORE, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Store word. */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			icache->store_word(mem, phys, data, this);
// 		} else {
// 			dcache->store_word(mem, phys, data, this);
// 		}
// 	} else {
// 		mem->store_word(phys, data, this);
// 	}

// }

// void
// CPU::swr_emulate(uint32 instr, uint32 pc)
// {
// 	uint32 phys, virt, wordvirt, base, regdata, memdata;
// 	int32 offset;
// 	uint8 which_byte;
// 	bool cacheable;

// 	/* Load data from register. */
// 	regdata = reg[rt(instr)];

// 	/* Calculate virtual address. */
// 	base = reg[rs(instr)];
// 	offset = s_immed(instr);
// 	virt = base + offset;
// 	/* We request the word containing the byte-address requested. */
// 	wordvirt = virt & ~0x03UL;

// 	/* Translate virtual address to physical address. */
// 	phys = cpzero->address_trans(wordvirt, DATASTORE, &cacheable, this);
// 	if (exception_pending) return;

// 	/* Read data from memory. */
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			memdata = icache->fetch_word(mem, phys, DATASTORE, this);
// 		} else {
// 			memdata = dcache->fetch_word(mem, phys, DATASTORE, this);
// 		}
// 	} else {
// 		memdata = mem->fetch_word(phys, DATASTORE, this);
// 	}

// 	if (exception_pending) return;

// 	/* Write back the right side of the register. */
// 	which_byte = virt & 0x03UL;
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			icache->store_word(mem, phys, swr(regdata, memdata, which_byte), this);
// 		} else {
// 			dcache->store_word(mem, phys, swr(regdata, memdata, which_byte), this);
// 		}
// 	} else {
// 		mem->store_word(phys, swr(regdata, memdata, which_byte), this);
// 	}

// }

// void
// CPU::lwc1_emulate(uint32 instr, uint32 pc)
// {
// 	if (opt_fpu) {
// 		uint32 phys, virt, base, word;
// 		int32 offset;
// 		bool cacheable;

// 		/* Calculate virtual address. */
// 		base = reg[rs(instr)];
// 		offset = s_immed(instr);
// 		virt = base + offset;

// 		/* This virtual address must be word-aligned. */
// 		if (virt % 4 != 0) {
// 			exception(AdEL,DATALOAD);
// 			return;
// 		}

// 		/* Translate virtual address to physical address. */
// 		phys = cpzero->address_trans(virt, DATALOAD, &cacheable, this);
// 		if (exception_pending) return;

// 		/* Fetch word. */
// 		if (cacheable) {
// 			if (cpzero->caches_swapped()) {
// 				word = icache->fetch_word(mem, phys, DATALOAD, this);
// 			} else {
// 				word = dcache->fetch_word(mem, phys, DATALOAD, this);
// 			}
// 		} else {
// 			word = mem->fetch_word(phys, DATALOAD, this);
// 		}

// 		if (exception_pending) return;

// 		/* Load target register with data. */
// 		fpu->write_reg (rt (instr), word);
// 	} else {
// 		cop_unimpl (1, instr, pc);
// 	}
// }

// void
// CPU::lwc2_emulate(uint32 instr, uint32 pc)
// {
// 	cop_unimpl (2, instr, pc);
// }

// void
// CPU::lwc3_emulate(uint32 instr, uint32 pc)
// {
// 	cop_unimpl (3, instr, pc);
// }

// void
// CPU::swc1_emulate(uint32 instr, uint32 pc)
// {
// 	if (opt_fpu) {
// 		uint32 phys, virt, base, data;
// 		int32 offset;
// 		bool cacheable;

// 		/* Load data from register. */
// 		data = fpu->read_reg (rt (instr));

// 		/* Calculate virtual address. */
// 		base = reg[rs(instr)];
// 		offset = s_immed(instr);
// 		virt = base + offset;

// 		/* This virtual address must be word-aligned. */
// 		if (virt % 4 != 0) {
// 			exception(AdES,DATASTORE);
// 			return;
// 		}

// 		/* Translate virtual address to physical address. */
// 		phys = cpzero->address_trans(virt, DATASTORE, &cacheable, this);
// 		if (exception_pending) return;

// 		/* Store word. */
// 		if (cacheable) {
// 			if (cpzero->caches_swapped()) {
// 				icache->store_word(mem, phys, data, this);
// 			} else {
// 				dcache->store_word(mem, phys, data, this);
// 			}
// 		} else {
// 			mem->store_word(phys, data, this);
// 		}

// 	} else {
// 		cop_unimpl (1, instr, pc);
// 	}
// }

// void
// CPU::swc2_emulate(uint32 instr, uint32 pc)
// {
// 	cop_unimpl (2, instr, pc);
// }

// void
// CPU::swc3_emulate(uint32 instr, uint32 pc)
// {
// 	cop_unimpl (3, instr, pc);
// }

// void
// CPU::sll_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = reg[rt(instr)] << shamt(instr);
// }

int32
srl(int32 a, int32 b)
{
	if (b == 0) {
		return a;
	} else if (b == 32) {
		return 0;
	} else {
		return (a >> b) & ((1 << (32 - b)) - 1);
	}
}

int32
sra(int32 a, int32 b)
{
	if (b == 0) {
		return a;
	} else {
		return (a >> b) | (((a >> 31) & 0x01) * (((1 << b) - 1) << (32 - b)));
	}
}

// void
// CPU::srl_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = srl(reg[rt(instr)], shamt(instr));
// }

// void
// CPU::sra_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = sra(reg[rt(instr)], shamt(instr));
// }

// void
// CPU::sllv_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = reg[rt(instr)] << (reg[rs(instr)] & 0x01f);
// }

// void
// CPU::srlv_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = srl(reg[rt(instr)], reg[rs(instr)] & 0x01f);
// }

// void
// CPU::srav_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = sra(reg[rt(instr)], reg[rs(instr)] & 0x01f);
// }

// void
// CPU::jr_emulate(uint32 instr, uint32 pc)
// {
// 	if (reg[rd(instr)] != 0) {
// 		exception(RI);
// 		return;
// 	}
// 	control_transfer (reg[rs(instr)]);
// }

// void
// CPU::jalr_emulate(uint32 instr, uint32 pc)
// {
// 	control_transfer (reg[rs(instr)]);
// 	 RA gets addr of instr after delay slot (2 words after this one). 
// 	reg[rd(instr)] = pc + 8;
// }

// void
// CPU::syscall_emulate(uint32 instr, uint32 pc)
// {
// 	exception(Sys);
// }

// void
// CPU::break_emulate(uint32 instr, uint32 pc)
// {
// 	exception(Bp);
// }

// void
// CPU::mfhi_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = hi;
// }

// void
// CPU::mthi_emulate(uint32 instr, uint32 pc)
// {
// 	if (rd(instr) != 0) {
// 		exception(RI);
// 		return;
// 	}
// 	hi = reg[rs(instr)];
// }

// void
// CPU::mflo_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = lo;
// }

// void
// CPU::mtlo_emulate(uint32 instr, uint32 pc)
// {
// 	if (rd(instr) != 0) {
// 		exception(RI);
// 		return;
// 	}
// 	lo = reg[rs(instr)];
// }

// void
// CPU::mult_emulate(uint32 instr, uint32 pc)
// {
// 	if (rd(instr) != 0) {
// 		exception(RI);
// 		return;
// 	}
// 	mult64s(&hi, &lo, reg[rs(instr)], reg[rt(instr)]);
// }

void
CPU::mult64(uint32 *hi, uint32 *lo, uint32 n, uint32 m)
{
#ifdef HAVE_LONG_LONG
	uint64 result;
	result = ((uint64)n) * ((uint64)m);
	*hi = (uint32) (result >> 32);
	*lo = (uint32) result;
#else /* HAVE_LONG_LONG */
	/*           n = (w << 16) | x ; m = (y << 16) | z
	 *     w x   g = a + e ; h = b + f ; p = 65535
	 *   X y z   c = (z * x) mod p
	 *   -----   b = (z * w + ((z * x) div p)) mod p
	 *   a b c   a = (z * w + ((z * x) div p)) div p
	 * d e f     f = (y * x) mod p
	 * -------   e = (y * w + ((y * x) div p)) mod p
	 * i g h c   d = (y * w + ((y * x) div p)) div p
	 */
	uint16 w,x,y,z,a,b,c,d,e,f,g,h,i;
	uint32 p;
	p = 65536;
	w = (n >> 16) & 0x0ffff;
	x = n & 0x0ffff;
	y = (m >> 16) & 0x0ffff;
	z = m & 0x0ffff;
	c = (z * x) % p;
	b = (z * w + ((z * x) / p)) % p;
	a = (z * w + ((z * x) / p)) / p;
	f = (y * x) % p;
	e = (y * w + ((y * x) / p)) % p;
	d = (y * w + ((y * x) / p)) / p;
	h = (b + f) % p;
	g = ((a + e) + ((b + f) / p)) % p;
	i = d + (((a + e) + ((b + f) / p)) / p);
	*hi = (i << 16) | g;
	*lo = (h << 16) | c;
#endif /* HAVE_LONG_LONG */
}

void
CPU::mult64s(uint32 *hi, uint32 *lo, int32 n, int32 m)
{
#ifdef HAVE_LONG_LONG
	int64 result;
	result = ((int64)n) * ((int64)m);
	*hi = (uint32) (result >> 32);
	*lo = (uint32) result;
#else /* HAVE_LONG_LONG */
	int32 result_sign = (n<0)^(m<0);
	int32 n_abs = n;
	int32 m_abs = m;

	if (n_abs < 0) n_abs = -n_abs;
	if (m_abs < 0) m_abs = -m_abs;

	mult64(hi,lo,n_abs,m_abs);
	if (result_sign)
	{
		*hi = ~*hi;
		*lo = ~*lo;
		if (*lo & 0x80000000)
		{
			*lo += 1;
			if (!(*lo & 0x80000000))
			{
				*hi += 1;
			}
		}
		else
		{
			*lo += 1;
		}
	}
#endif /* HAVE_LONG_LONG */
}

// void
// CPU::multu_emulate(uint32 instr, uint32 pc)
// {
// 	if (rd(instr) != 0) {
// 		exception(RI);
// 		return;
// 	}
// 	mult64(&hi, &lo, reg[rs(instr)], reg[rt(instr)]);
// }

// void
// CPU::div_emulate(uint32 instr, uint32 pc)
// {
// 	int32 signed_rs = (int32)reg[rs(instr)];
// 	int32 signed_rt = (int32)reg[rt(instr)];
// 	lo = signed_rs / signed_rt;
// 	hi = signed_rs % signed_rt;
// }

// void
// CPU::divu_emulate(uint32 instr, uint32 pc)
// {
// 	lo = reg[rs(instr)] / reg[rt(instr)];
// 	hi = reg[rs(instr)] % reg[rt(instr)];
// }

// void
// CPU::add_emulate(uint32 instr, uint32 pc)
// {
// 	int32 a, b, sum;
// 	a = (int32)reg[rs(instr)];
// 	b = (int32)reg[rt(instr)];
// 	sum = a + b;
// 	if ((a < 0 && b < 0 && !(sum < 0)) || (a >= 0 && b >= 0 && !(sum >= 0))) {
// 		exception(Ov);
// 		return;
// 	} else {
// 		reg[rd(instr)] = (uint32)sum;
// 	}
// }

// void
// CPU::addu_emulate(uint32 instr, uint32 pc)
// {
// 	int32 a, b, sum;
// 	a = (int32)reg[rs(instr)];
// 	b = (int32)reg[rt(instr)];
// 	sum = a + b;
// 	reg[rd(instr)] = (uint32)sum;
// }

// void
// CPU::sub_emulate(uint32 instr, uint32 pc)
// {
// 	int32 a, b, diff;
// 	a = (int32)reg[rs(instr)];
// 	b = (int32)reg[rt(instr)];
// 	diff = a - b;
// 	if ((a < 0 && !(b < 0) && !(diff < 0)) || (!(a < 0) && b < 0 && diff < 0)) {
// 		exception(Ov);
// 		return;
// 	} else {
// 		reg[rd(instr)] = (uint32)diff;
// 	}
// }

// void
// CPU::subu_emulate(uint32 instr, uint32 pc)
// {
// 	int32 a, b, diff;
// 	a = (int32)reg[rs(instr)];
// 	b = (int32)reg[rt(instr)];
// 	diff = a - b;
// 	reg[rd(instr)] = (uint32)diff;
// }

// void
// CPU::and_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = reg[rs(instr)] & reg[rt(instr)];
// }

// void
// CPU::or_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = reg[rs(instr)] | reg[rt(instr)];
// }

// void
// CPU::xor_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = reg[rs(instr)] ^ reg[rt(instr)];
// }

// void
// CPU::nor_emulate(uint32 instr, uint32 pc)
// {
// 	reg[rd(instr)] = ~(reg[rs(instr)] | reg[rt(instr)]);
// }

// void
// CPU::slt_emulate(uint32 instr, uint32 pc)
// {
// 	int32 s_rs = (int32)reg[rs(instr)];
// 	int32 s_rt = (int32)reg[rt(instr)];
// 	if (s_rs < s_rt) {
// 		reg[rd(instr)] = 1;
// 	} else {
// 		reg[rd(instr)] = 0;
// 	}
// }

// void
// CPU::sltu_emulate(uint32 instr, uint32 pc)
// {
// 	if (reg[rs(instr)] < reg[rt(instr)]) {
// 		reg[rd(instr)] = 1;
// 	} else {
// 		reg[rd(instr)] = 0;
// 	}
// }

// void
// CPU::bltz_emulate(uint32 instr, uint32 pc)
// {
// 	if ((int32)reg[rs(instr)] < 0)
// 		branch(instr, pc);
// }

// void
// CPU::bgez_emulate(uint32 instr, uint32 pc)
// {
// 	if ((int32)reg[rs(instr)] >= 0)
// 		branch(instr, pc);
// }

// /* As with JAL, BLTZAL and BGEZAL cause RA to get the address of the
//  * instruction two words after the current one (pc + 8).
//  */
// void
// CPU::bltzal_emulate(uint32 instr, uint32 pc)
// {
// 	reg[reg_ra] = pc + 8;
// 	if ((int32)reg[rs(instr)] < 0)
// 		branch(instr, pc);
// }

// void
// CPU::bgezal_emulate(uint32 instr, uint32 pc)
// {
// 	reg[reg_ra] = pc + 8;
// 	if ((int32)reg[rs(instr)] >= 0)
// 		branch(instr, pc);
// }

// /* reserved instruction */
// void
// CPU::RI_emulate(uint32 instr, uint32 pc)
// {
// 	exception(RI);
// }

void CPU::funct_ctrl()
{
	static alu_funcpr execSpecialTable[64] = {
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		&CPU::jr_exec, &CPU::jal_exec, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	};

	PipelineRegs *preg = PL_REGS[ID_STAGE];
	uint32 dec_instr = preg->instr;
	if (execSpecialTable[funct(dec_instr)]) {
		(this->*execSpecialTable[funct(dec_instr)])();
	}
}

void CPU::funct_exec()
{
	static alu_funcpr execSpecialTable[64] = {
		&CPU::sll_exec, NULL, &CPU::srl_exec, &CPU::sra_exec, &CPU::sll_exec, NULL, &CPU::srl_exec, &CPU::sra_exec,
		NULL, NULL, NULL, NULL, &CPU::syscall_exec, &CPU::break_exec, NULL, NULL,
		&CPU::mfhi_exec, &CPU::mthi_exec, &CPU::mflo_exec, &CPU::mtlo_exec, NULL, NULL, NULL, NULL,
		&CPU::mult_exec, &CPU::multu_exec, &CPU::div_exec, &CPU::divu_exec, NULL, NULL, NULL, NULL,
		&CPU::add_exec, &CPU::addu_exec, &CPU::sub_exec, &CPU::subu_exec, &CPU::and_exec, &CPU::or_exec, &CPU::xor_exec, &CPU::nor_exec,
		NULL, NULL, &CPU::slt_exec, &CPU::sltu_exec, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	};

	PipelineRegs *preg = PL_REGS[EX_STAGE];
	uint32 exec_instr = preg->instr;
	if (execSpecialTable[funct(exec_instr)]) {
		(this->*execSpecialTable[funct(exec_instr)])();
	}
}

void CPU::bcond_exec()
{
	static alu_funcpr  execBcondTable[32] = {
		&CPU::bltz_exec, &CPU::bgez_exec, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		&CPU::bltzal_exec, &CPU::bgezal_exec, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	};

	PipelineRegs *preg = PL_REGS[ID_STAGE];
	uint32 dec_instr = preg->instr;
	if (execBcondTable[rt(dec_instr)]) {
		(this->*execBcondTable[rt(dec_instr)])();
	}

}

void CPU::j_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	pc = (((preg->pc + 4) & 0xf0000000) | (jumptarg(preg->instr) << 2) - 4); //+4 later
	//fprintf(stderr, "go to %X\n", pc);
}

void CPU::jal_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	pc = (((preg->pc + 4) & 0xf0000000) | (jumptarg(preg->instr) << 2) - 4); //+4 later
	preg->result = preg->pc + 8;
}

void CPU::beq_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if (*(preg->alu_src_a) == *(preg->alu_src_b)) {
		pc = preg->pc + (preg->imm << 2); //+4 later
	}
	//fprintf(stderr, "\tbeq %d == %d?\n", *(preg->alu_src_a), *(preg->alu_src_b));
}

void CPU::bne_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if (*(preg->alu_src_a) != *(preg->alu_src_b)) {
		pc = preg->pc + (preg->imm << 2); //+4 later
	}
}

void CPU::blez_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if (*(preg->alu_src_a) == 0 ||
		 *(preg->alu_src_a) & 0x80000000) {
		pc = preg->pc + (preg->imm << 2); //+4 later
	}
}

void CPU::bgtz_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if (*(preg->alu_src_a) != 0 &&
		(*(preg->alu_src_a) & 0x80000000) == 0) {
		pc = preg->pc + (preg->imm << 2); //+4 later
	}
}

void CPU::lui_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = *(preg->alu_src_b) << 16;
}

void CPU::cpzero_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	cpzero->cpzero_emulate(preg->instr, preg->pc);
	dcache->cache_isolate(cpzero->caches_isolated());
}

void CPU::cpone_exec()
{

}

void CPU::cptwo_exec()
{

}

void CPU::cpthree_exec()
{

}

void CPU::lwc1_exec()
{

}

void CPU::lwc2_exec()
{

}

void CPU::lwc3_exec()
{

}

void CPU::swc1_exec()
{

}

void CPU::swc2_exec()
{

}

void CPU::swc3_exec()
{

}

void CPU::sll_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = *(preg->alu_src_a) << (*(preg->alu_src_b) & 0x1f);
}

void CPU::srl_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = srl(*(preg->alu_src_a), (*(preg->alu_src_b) & 0x1f));
}

void CPU::sra_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = sra(*(preg->alu_src_a), (*(preg->alu_src_b) & 0x1f));
}

void CPU::jr_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	pc = *(preg->alu_src_a) - 4; //+4 later
	//fprintf(stderr, "go to %X\n", pc);
}

void CPU::jalr_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	pc = *(preg->alu_src_a) - 4; //+4 later
	preg->result = preg->pc + 8;
	fprintf(stderr, "go to %X\n", pc);
}

void CPU::syscall_exec()
{
	exception(Sys);
}

void CPU::break_exec()
{
	exception(Bp);
}

void CPU::mfhi_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = hi;
}

void CPU::mthi_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	hi = *(preg->alu_src_a);
}

void CPU::mflo_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = lo;
}

void CPU::mtlo_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	lo = *(preg->alu_src_a);
}

void CPU::mult_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	mult64s(&hi, &lo, *(preg->alu_src_a), *(preg->alu_src_b));
	mul_div_remain = mul_div_delay[FUNCT_MULT];
}

void CPU::multu_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	mult64(&hi, &lo, *(preg->alu_src_a), *(preg->alu_src_b));
	mul_div_remain = mul_div_delay[FUNCT_MULTU];
}

void CPU::div_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	int32 signed_a = (int32)(*(preg->alu_src_a));
	int32 signed_b = (int32)(*(preg->alu_src_b));
	lo = signed_a / signed_b;
	hi = signed_a % signed_b;
	mul_div_remain = mul_div_delay[FUNCT_DIV];
}

void CPU::divu_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	lo = *(preg->alu_src_a) / *(preg->alu_src_b);
	hi = *(preg->alu_src_a) % *(preg->alu_src_b);
	mul_div_remain = mul_div_delay[FUNCT_DIVU];
}

void CPU::add_exec()
{
	int32 a, b, sum;
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	a = (int32)(*(preg->alu_src_a));
	b = (int32)(*(preg->alu_src_b));
	sum = a + b;
	if ((a < 0 && b < 0 && !(sum < 0)) || (a >= 0 && b >= 0 && !(sum >= 0))) {
		exception(Ov);
	} else {
		preg->result = (uint32)sum;
	}
}

void CPU::addu_exec()
{
	int32 a, b, sum;
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	a = (int32)(*(preg->alu_src_a));
	b = (int32)(*(preg->alu_src_b));
	sum = a + b;
	preg->result = (uint32)sum;
	// if (opcode(preg->instr) == OP_ADDIU)
	// 	printf("ADDIU: %X = %X + %X dst: %d\n", sum, a, b, preg->dst);
}

void CPU::sub_exec()
{
	int32 a, b, diff;
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	a = (int32)(*(preg->alu_src_a));
	b = (int32)(*(preg->alu_src_b));
	diff = a - b;
	if ((a < 0 && !(b < 0) && !(diff < 0)) || (!(a < 0) && b < 0 && diff < 0)) {
		exception(Ov);
	} else {
		preg->result = (uint32)diff;
	}
}

void CPU::subu_exec()
{
	int32 a, b, diff;
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	a = (int32)(*(preg->alu_src_a));
	b = (int32)(*(preg->alu_src_b));
	diff = a - b;
	preg->result = (uint32)diff;
}

void CPU::and_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = *(preg->alu_src_a) & *(preg->alu_src_b);
}

void CPU::or_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = *(preg->alu_src_a) | *(preg->alu_src_b);
}

void CPU::xor_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = *(preg->alu_src_a) ^ *(preg->alu_src_b);
}

void CPU::nor_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = ~(*(preg->alu_src_a) | *(preg->alu_src_b));
}

void CPU::slt_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	int32 a = (int32)(*(preg->alu_src_a));
	int32 b = (int32)(*(preg->alu_src_b));
	if (a < b) {
		preg->result = 1;
	} else {
		preg->result = 0;
	}
}

void CPU::sltu_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	if (*(preg->alu_src_a) < *(preg->alu_src_b)) {
		preg->result = 1;
	} else {
		preg->result = 0;
	}
}

void CPU::bltz_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if ((int32)*(preg->alu_src_a) < 0) {
		pc = preg->pc + (preg->imm << 2); //+4 later
	}
}

void CPU::bgez_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if ((int32)*(preg->alu_src_a) >= 0) {
		pc = preg->pc + (preg->imm << 2); //+4 later
	}
}

void CPU::bltzal_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if ((int32)*(preg->alu_src_a) < 0) {
		pc = preg->pc + 4 + (preg->imm << 2); //+4 later
	}
	preg->result = preg->pc + 8;
}

void CPU::bgezal_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if ((int32)*(preg->alu_src_a) >= 0) {
		pc = preg->pc + (preg->imm << 2); //+4 later
	}
	preg->result = preg->pc + 8;
}

