/* MIPS R3000 CPU emulation.
   Copyright 2001, 2002, 2003, 2004 Brian R. Gaeke.

This file is part of VMIPS.

VMIPS is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

VMIPS is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with VMIPS; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <string.h>
#include "cpu.h"
#include "cpzero.h"
#include "debug.h"
#include "fpu.h"
#include "mapper.h"
#include "vmips.h"
#include "options.h"
#include "excnames.h"
#include "cpzeroreg.h"
#include "error.h"
#include "remotegdb.h"
#include "fileutils.h"
#include "stub-dis.h"
#include <cstring>
#include "state.h"
#include "cache.h"
#include "ISA.h"

/* states of the delay-slot state machine -- see CPU::step() */
static const int NORMAL = 0, DELAYING = 1, DELAYSLOT = 2;

/* certain fixed register numbers which are handy to know */
static const int reg_zero = 0;  /* always zero */
static const int reg_sp = 29;   /* stack pointer */
static const int reg_ra = 31;   /* return address */

#include "emuls.cpp"

/* pointer to CPU method returning void and taking two uint32's */
typedef void (CPU::*emulate_funptr)(uint32, uint32);

static const char *strdelaystate(const int state) {
	static const char *const statestr[] = {
		"NORMAL", "DELAYING", "DELAYSLOT"
	};
	return statestr[state];
}

PipelineRegs::PipelineRegs(uint32 pc_, uint32 instr_):
	pc(pc_), instr(instr_)
{
	alu_src_a = &(this->shamt);
	alu_src_b = &(this->shamt);
	w_reg_data = w_mem_data = lwrl_reg_prev = NULL;
	mem_read_op = false;
	src_a = src_b = dst = NONE_REG;
	result = r_mem_data = imm = shamt = 0;
}



CPU::CPU (Mapper &m, IntCtrl &i)
  : tracing (false), last_epc (0), last_prio (0), mem (&m),
    cpzero (new CPZero (this, &i)), fpu (0), delay_state (NORMAL)
{
	opt_fpu = machine->opt->option("fpu")->flag;
	if (opt_fpu)
		fpu = new FPU (this);
	reg[reg_zero] = 0;
	opt_excmsg = machine->opt->option("excmsg")->flag;
	opt_reportirq = machine->opt->option("reportirq")->flag;
	opt_excpriomsg = machine->opt->option("excpriomsg")->flag;
	opt_haltbreak = machine->opt->option("haltbreak")->flag,
	opt_haltibe = machine->opt->option("haltibe")->flag;
	opt_instdump = machine->opt->option("instdump")->flag;
	opt_tracing = machine->opt->option("tracing")->flag;
	opt_tracesize = machine->opt->option("tracesize")->num;
	opt_tracestartpc = machine->opt->option("tracestartpc")->num;
	opt_traceendpc = machine->opt->option("traceendpc")->num;
	opt_bigendian = machine->opt->option("bigendian")->flag;
	if (opt_tracing)
		open_trace_file ();
	/* Caches are 2way-set associative, physically indexed, physically tagged,
	 * with 1-word lines. */
	opt_icacheway = machine->opt->option("icacheway")->num;
	opt_dcacheway = machine->opt->option("dcacheway")->num;
	opt_icachebnum = machine->opt->option("icachebnum")->num;
	opt_dcachebnum = machine->opt->option("dcachebnum")->num;
	opt_icachebsize = machine->opt->option("icachebsize")->num;
	opt_dcachebsize = machine->opt->option("dcachebsize")->num;

}

CPU::~CPU ()
{
	if (opt_tracing)
		close_trace_file ();
	delete icache;
	delete dcache;
	for (int i = 0; i < PIPELINE_STAGES; i++) {
		if (PL_REGS[i]) delete PL_REGS[i];
	}
}

void
CPU::reset(void)
{
#ifdef INTENTIONAL_CONFUSION
	int r;
	for (r = 0; r < 32; r++) {
		reg[r] = random();
	}
#endif /* INTENTIONAL_CONFUSION */
	reg[reg_zero] = 0;
	pc = 0xbfc00000 - 4; //+4 later
	cpzero->reset();
	//generate cache
	icache = new Cache(opt_icachebnum, opt_icachebsize, opt_icacheway);	/* 64Byte * 64Block * 2way = 8KB*/
	dcache = new Cache(opt_dcachebnum, opt_dcachebsize, opt_dcacheway);
	//fill NOP for each Pipeline stage
	volatilize_pipeline();
}

void CPU::volatilize_pipeline()
{
	for (int i = 0; i < PIPELINE_STAGES; i++) {
		PL_REGS[i] = new PipelineRegs(0, NOP_INSTR);
	}
}

void
CPU::dump_regs(FILE *f)
{
	int i;

	fprintf(f,"Reg Dump: [ PC=%08x  LastInstr=%08x  HI=%08x  LO=%08x\n",
		PL_REGS[WB_STAGE]->pc,PL_REGS[WB_STAGE]->instr,hi,lo);
	// fprintf(f,"            DelayState=%s  DelayPC=%08x  NextEPC=%08x\n",
	// 	strdelaystate(delay_state), delay_pc, next_epc);
	for (i = 0; i < 32; i++) {
		fprintf(f," R%02d=%08x ",i,reg[i]);
		if (i % 5 == 4) {
			fputc('\n',f);
		} else if (i == 31) {
			fprintf(f, " ]\n");
		}
	}
}

void
CPU::dump_stack(FILE *f)
{
	uint32 stackphys;
	if (cpzero->debug_tlb_translate(reg[reg_sp], &stackphys)) {
		mem->dump_stack(f, stackphys);
	} else {
		fprintf(f, "Stack: (not mapped in TLB)\n");
	}
}

void
CPU::dump_mem(FILE *f, uint32 addr)
{
	uint32 phys;
        fprintf(f, "0x%08x: ", addr);
	if (cpzero->debug_tlb_translate(addr, &phys)) {
		mem->dump_mem(f, phys);
	} else {
		fprintf(f, "(not mapped in TLB)");
	}
	fprintf(f, "\n");
}

/* Disassemble a word at addr and print the result to f.
 * FIXME: currently, f must be stderr, for the disassembler.
 */
void
CPU::dis_mem(FILE *f, uint32 addr)
{
	uint32 phys;
	uint32 instr;
	if (cpzero->debug_tlb_translate(addr, &phys)) {
		fprintf(f,"PC=0x%08x [%08x]      %s",addr,phys,(addr==PL_REGS[WB_STAGE]->pc?"=>":"  "));
		instr = mem->fetch_word(phys,INSTFETCH, this);
		fprintf(f,"%08x ",instr);
		machine->disasm->disassemble(addr,instr);
	} else {
		fprintf(f,"PC=0x%08x [%08x]      %s",addr,phys,(addr==PL_REGS[WB_STAGE]->pc?"=>":"  "));
		fprintf(f, "(not mapped in TLB)\n");
	}
}

void
CPU::cpzero_dump_regs_and_tlb(FILE *f)
{
	cpzero->dump_regs_and_tlb(f);
}

/* exception handling */
static const char *strexccode(const uint16 excCode) {
	static const char *const exception_strs[] =
	{
		/* 0 */ "Interrupt",
		/* 1 */ "TLB modification exception",
		/* 2 */ "TLB exception (load or instr fetch)",
		/* 3 */ "TLB exception (store)",
		/* 4 */ "Address error exception (load or instr fetch)",
		/* 5 */ "Address error exception (store)",
		/* 6 */ "Instruction bus error",
		/* 7 */ "Data (load or store) bus error",
		/* 8 */ "SYSCALL exception",
		/* 9 */ "Breakpoint32 exception (BREAK instr)",
		/* 10 */ "Reserved instr exception",
		/* 11 */ "Coprocessor Unusable",
		/* 12 */ "Arithmetic Overflow",
		/* 13 */ "Trap (R4k/R6k only)",
		/* 14 */ "LDCz or SDCz to uncached address (R6k)",
		/* 14 */ "Virtual Coherency Exception (instr) (R4k)",
		/* 15 */ "Machine check exception (R6k)",
		/* 15 */ "Floating-point32 exception (R4k)",
		/* 16 */ "Exception 16 (reserved)",
		/* 17 */ "Exception 17 (reserved)",
		/* 18 */ "Exception 18 (reserved)",
		/* 19 */ "Exception 19 (reserved)",
		/* 20 */ "Exception 20 (reserved)",
		/* 21 */ "Exception 21 (reserved)",
		/* 22 */ "Exception 22 (reserved)",
		/* 23 */ "Reference to WatchHi/WatchLo address detected (R4k)",
		/* 24 */ "Exception 24 (reserved)",
		/* 25 */ "Exception 25 (reserved)",
		/* 26 */ "Exception 26 (reserved)",
		/* 27 */ "Exception 27 (reserved)",
		/* 28 */ "Exception 28 (reserved)",
		/* 29 */ "Exception 29 (reserved)",
		/* 30 */ "Exception 30 (reserved)",
		/* 31 */ "Virtual Coherency Exception (data) (R4k)"
	};

	return exception_strs[excCode];
}

static const char *strmemmode(const int memmode) {
	static const char *const memmode_strs[] =
	{
		"instruction fetch", /* INSTFETCH */
		"data load", /* DATALOAD */
		"data store", /* DATASTORE */
		"not applicable" /* ANY */
	};

	return memmode_strs[memmode];
}

int
CPU::exception_priority(uint16 excCode, int mode) const
{
	/* See doc/excprio for an explanation of this table. */
	static const struct excPriority prio[] = {
		{1, AdEL, INSTFETCH},
		{2, TLBL, INSTFETCH}, {2, TLBS, INSTFETCH},
		{3, IBE, ANY},
		{4, Ov, ANY}, {4, Tr, ANY}, {4, Sys, ANY},
		{4, Bp, ANY}, {4, RI, ANY}, {4, CpU, ANY},
		{5, AdEL, DATALOAD}, {5, AdES, ANY},
		{6, TLBL, DATALOAD}, {6, TLBS, DATALOAD},
		{6, TLBL, DATASTORE}, {6, TLBS, DATASTORE},
		{7, Mod, ANY},
		{8, DBE, ANY},
		{9, Int, ANY},
		{0, ANY, ANY} /* catch-all */
	};
	const struct excPriority *p;

	for (p = prio; p->priority != 0; p++) {
		if (excCode == p->excCode || p->excCode == ANY) {
			if (mode == p->mode || p->mode == ANY) {
				return p->priority;
			} else if (opt_excpriomsg) {
				fprintf(stderr,
					"exception code matches but mode %d != table %d\n",
					mode,p->mode);
			}
		} else if (opt_excpriomsg) {
			fprintf(stderr, "exception code %d != table %d\n", excCode,
				p->excCode);
		}
	}
	return 0;
}

void
CPU::exception(uint16 excCode, int mode /* = ANY */, int coprocno /* = -1 */)
{
	//just buffer exception signal in this method
	//handling buffered signals in exe_handle()
	exc_signal = new ExcInfo();
	exc_signal->excCode = excCode;
	exc_signal->mode = mode;
	exc_signal->coprocno = coprocno;
	exception_pending = false; //will be removed
	catch_exception = true;
}

void CPU::exc_handle(PipelineRegs *preg)
{
	int prio;
	int max_prio;
	uint16 excCode;
	int mode;
	int coprocno;
	uint32 base, vector, epc;

	if (preg->excBuf.size() == 0) {
		return;
	}

	/* Prioritize exception -- if the last exception to occur _also_ was
	 * caused by this EPC, only report this exception if it has a higher
	 * priority.  Otherwise, exception handling terminates here,
	 * because only one exception will be reported per instruction
	 * (as per MIPS RISC Architecture, p. 6-35). Note that this only
	 * applies IFF the previous exception was caught during the current
	 * _execution_ of the instruction at this EPC, so we check that
	 * EXCEPTION_PENDING is true before aborting exception handling.
	 * (This flag is reset by each call to step().)
	 */
	max_prio = -1;
	if (opt_excpriomsg & (preg->excBuf.size() > 1)) {
		fprintf(stderr,
			"(Ignoring additional lower priority exception...)\n");
	}
	//get highest priority of excCode
	for (int i = 0; i < preg->excBuf.size(); i++) {
		prio = exception_priority((preg->excBuf[i])->excCode, (preg->excBuf[i])->mode);
		if (prio > max_prio) {
			//update
			excCode = (preg->excBuf[i])->excCode;
			mode = (preg->excBuf[i])->mode;
			coprocno = (preg->excBuf[i])->coprocno;
		}
	}

	if (opt_haltbreak) {
		if (excCode == Bp) {
			fprintf(stderr,"* BREAK instruction reached -- HALTING *\n");
			machine->halt();
		}
	}
	if (opt_haltibe) {
		if (excCode == IBE) {
			fprintf(stderr,"* Instruction bus error occurred -- HALTING *\n");
			machine->halt();
		}
	}

	/* step() ensures that next_epc will always contain the correct
	 * ne whenever exception() is called.
	 */
	epc = preg->pc;

	/* Set processor to Kernel mode, disable interrupts, and save 
	 * exception PC.
	 */
	cpzero->enter_exception(epc,excCode,coprocno,false);

	/* Calculate the exception handler address; this is of the form BASE +
	 * VECTOR. The BASE is determined by whether we're using boot-time
	 * exception vectors, according to the BEV bit in the CP0 Status register.
	 */
	if (cpzero->use_boot_excp_address()) {
		base = 0xbfc00100;
	} else {
		base = 0x80000000;
	}

	/* Do we have a User TLB Miss exception? If so, jump to the
	 * User TLB Miss exception vector, otherwise jump to the
	 * common exception vector.
	 */
	if ((excCode == TLBL || excCode == TLBS) && (cpzero->tlb_miss_user)) {
		vector = 0x000;
	} else {
		vector = 0x080;
	}

	if (opt_excmsg && (excCode != Int || opt_reportirq)) {
		fprintf(stderr,"Exception %d (%s) triggered, EPC=%08x\n", excCode, 
			strexccode(excCode), epc);
		fprintf(stderr,
			"Priority is %d; mem access mode is %s\n",
			prio, strmemmode(mode));
		if (excCode == Int) {
		  uint32 status, bad, cause;
		  cpzero->read_debug_info(&status, &bad, &cause);
		  fprintf (stderr, " Interrupt cause = %x, status = %x\n", cause, status);
		}
		//PC error
		// fprintf(stderr,
		// 	"** PC address translation caused the exception! **\n");
	}
    if (opt_tracing) {
		if (tracing) {
			current_trace.exception_happened = true;
			current_trace.last_exception_code = excCode;
		}
	}
	pc = base + vector - 4; //+4 later
	volatilize_pipeline();
}

void
CPU::open_trace_file ()
{
	char tracefilename[80];
	for (unsigned i = 0; ; ++i) {
		sprintf (tracefilename, "traceout%d.txt", i);
		if (!can_read_file (tracefilename))
			break;
	}
	traceout = fopen (tracefilename, "w");
}

void
CPU::close_trace_file ()
{
	fclose (traceout);
}

void
CPU::write_trace_to_file ()
{
	for (Trace::record_iterator ri = current_trace.rbegin (), re =
         current_trace.rend (); ri != re; ++ri) {
        Trace::Record &tr = *ri;
		fprintf (traceout, "(TraceRec (PC #x%08x) (Insn #x%08x) ", tr.pc, tr.instr);

		if (! tr.inputs.empty ()) {

		fprintf (traceout, "(Inputs (");
		for (std::vector<Trace::Operand>::iterator oi = tr.inputs.begin (),
			oe = tr.inputs.end (); oi != oe; ++oi) {
			Trace::Operand &op = *oi;
			if (op.regno != -1) {
				fprintf (traceout, "(%s %d #x%08x) ", op.tag, op.regno, op.val);
			} else {
				fprintf (traceout, "(%s #x%08x) ", op.tag, op.val);
			}
		}
		fprintf (traceout, "))");

		}

		if (!tr.outputs.empty ()) {

		fprintf (traceout, "(Outputs (");
		for (std::vector<Trace::Operand>::iterator oi = tr.outputs.begin (),
			oe = tr.outputs.end (); oi != oe; ++oi) {
			Trace::Operand &op = *oi;
			if (op.regno != -1) {
				fprintf (traceout, "(%s %d #x%08x) ", op.tag, op.regno, op.val);
			} else {
				fprintf (traceout, "(%s #x%08x) ", op.tag, op.val);
			}
		}
		fprintf (traceout, "))");

		}
		fprintf (traceout, ")\n");
	}
	// print lastchanges
	fprintf (traceout, "(LastChanges (");
	for (unsigned i = 0; i < 32; ++i) { 
		if (current_trace.last_change_for_reg.find (i) !=
				current_trace.last_change_for_reg.end ()) {
			last_change &lc = current_trace.last_change_for_reg[i];
			fprintf (traceout,
				"(Reg %d PC %x Instr %x OldValue %x CurValue %x) ",
				i, lc.pc, lc.instr, lc.old_value, reg[i]);
		}
	}
	fprintf (traceout, "))\n");
}

void
CPU::flush_trace()
{
	if (!opt_tracing) return;
	write_trace_to_file ();
	current_trace.clear ();
}

void
CPU::start_tracing()
{
	if (!opt_tracing) return;
	tracing = true;
	current_trace.clear ();
}

void
CPU::write_trace_instr_inputs (uint32 instr)
{
    // rs,rt:  add,addu,and,beq,bne,div,divu,mult,multu,nor,or,sb,sh,sllv,slt,
    //         sltu,srav,srlv,sub,subu,sw,swl,swr,xor
    // rs:     addi,addiu,andi,bgez,bgezal,bgtz,blez,bltz,bltzal,jal,jr,lb,lbu,
    //         lh,lhu,lw,lwl,lwr,mthi,mtlo,ori,slti,sltiu,xori
    // hi:     mfhi
    // lo:     mflo
    // rt:     mtc0
    // rt,shamt: sll,sra,srl
	// NOT HANDLED: syscall,break,jalr,cpone,cpthree,cptwo,cpzero,
	//              j,lui,lwc1,lwc2,lwc3,mtc0,swc1,swc2,swc3
	switch(opcode(instr))
	{
		case 0:
			switch(funct(instr))
			{
				case 4: //sllv
				case 6: //srlv
				case 7: //srav
				case 24: //mult
				case 25: //multu
				case 26: //div
				case 27: //divu
				case 32: //add
				case 33: //addu
				case 34: //sub
				case 35: //subu
				case 36: //and
				case 37: //or
				case 38: //xor
				case 39: //nor
				case 42: //slt
				case 43: //sltu
					current_trace_record.inputs_push_back_op("rs",
                        rs(instr), reg[rs(instr)]);
					current_trace_record.inputs_push_back_op("rt",
                        rt(instr), reg[rt(instr)]);
					break;
				case 0: //sll
				case 2: //srl
				case 3: //sra
					current_trace_record.inputs_push_back_op("rt",
                        rt(instr), reg[rt(instr)]);
					break;
				case 8: //jr
				case 17: //mthi
				case 19: //mtlo
					current_trace_record.inputs_push_back_op("rs",
                        rs(instr), reg[rs(instr)]);
					break;
				case 16: //mfhi
					current_trace_record.inputs_push_back_op("hi",
                        hi);
					break;
				case 18: //mflo
					current_trace_record.inputs_push_back_op("lo",
                        lo);
					break;
			}
			break;
		case 1:
			switch(rt(instr)) {
				case 0: //bltz
				case 1: //bgez
				case 16: //bltzal
				case 17: //bgezal
					current_trace_record.inputs_push_back_op("rs",
                        rs(instr), reg[rs(instr)]);
					break;
			}
			break;
		case 3: //jal
		case 6: //blez
		case 7: //bgtz
		case 8: //addi
		case 9: //addiu
		case 12: //andi
		case 32: //lb
		case 33: //lh
		case 35: //lw
		case 36: //lbu
		case 37: //lhu
		case 40: //sb
		case 41: //sh
		case 42: //swl
		case 43: //sw
		case 46: //swr
			current_trace_record.inputs_push_back_op("rs",
				rs(instr), reg[rs(instr)]);
			current_trace_record.inputs_push_back_op("rt",
				rt(instr), reg[rt(instr)]);
			break;
		case 4: //beq
		case 5: //bne
		case 10: //slti
		case 11: //sltiu
		case 13: //ori
		case 14: //xori
		case 34: //lwl
		case 38: //lwr
			current_trace_record.inputs_push_back_op("rs",
				rs(instr), reg[rs(instr)]);
			break;
	}
}


void
CPU::write_trace_instr_outputs (uint32 instr)
{
    // hi,lo: div,divu,mult,multu
    // hi:    mthi
    // lo:    mtlo
    // rt:    addi,addiu,andi,lb,lbu,lh,lhu,lui,lw,lwl,lwr,mfc0,ori,slti,sltiu,
    //        xori
    // rd:    add,addu,and,jalr,mfhi,mflo,nor,or,sllv,slt,sltu,sll,sra,srav,srl,
    //        srlv,sub,subu,xor
    // r31:   jal
	// NOT HANDLED: syscall,break,jalr,cpone,cpthree,cptwo,cpzero,j,lwc1,lwc2,
    //              lwc3,mtc0,swc1,swc2,swc3,jr,jalr,mfc0,bgtz,blez,sb,sh,sw,
    //              swl,swr,beq,bne
	switch(opcode(instr))
	{
		case 0:
			switch(funct(instr))
			{
				case 26: //div
				case 27: //divu
				case 24: //mult
				case 25: //multu
					current_trace_record.outputs_push_back_op("lo",
                        lo);
					current_trace_record.outputs_push_back_op("hi",
                        hi);
					break;
				case 17: //mthi
					current_trace_record.outputs_push_back_op("hi",
                        hi);
					break;
				case 19: //mtlo
					current_trace_record.outputs_push_back_op("lo",
                        lo);
					break;
				case 32: //add
				case 33: //addu
				case 36: //and
				case 39: //nor
				case 37: //or
				case 4: //sllv
				case 42: //slt
				case 43: //sltu
				case 7: //srav
				case 6: //srlv
				case 34: //sub
				case 35: //subu
				case 38: //xor
				case 0: //sll
				case 3: //sra
				case 2: //srl
				case 16: //mfhi
				case 18: //mflo
					current_trace_record.outputs_push_back_op("rd",
                        rd(instr),reg[rd(instr)]);
					break;
			}
			break;
		case 8: //addi
		case 9: //addiu
		case 12: //andi
		case 32: //lb
		case 36: //lbu
		case 33: //lh
		case 37: //lhu
		case 35: //lw
		case 34: //lwl
		case 38: //lwr
		case 13: //ori
		case 10: //slti
		case 11: //sltiu
		case 14: //xori
		case 15: //lui
			current_trace_record.outputs_push_back_op("rt",rt(instr),
                reg[rt(instr)]);
			break;
		case 3: //jal
			current_trace_record.outputs_push_back_op("ra",
                reg[reg_ra]);
			break;
	}
}

void
CPU::write_trace_record_1(uint32 pc, uint32 instr)
{
	current_trace_record.clear ();
	current_trace_record.pc = pc;
	current_trace_record.instr = instr;
	write_trace_instr_inputs (instr);
	std::copy (&reg[0], &reg[32], &current_trace_record.saved_reg[0]);
}

void
CPU::write_trace_record_2 (uint32 pc, uint32 instr)
{
	write_trace_instr_outputs (instr);
	// which insn was the last to change each reg?
	for (unsigned i = 0; i < 32; ++i) {
		if (current_trace_record.saved_reg[i] != reg[i]) {
			current_trace.last_change_for_reg[i] = last_change::make (pc, instr,
				current_trace_record.saved_reg[i]);
		}
	}
	current_trace.push_back_record (current_trace_record);
    if (current_trace.record_size () > opt_tracesize)
		flush_trace ();
}

void
CPU::stop_tracing()
{
	tracing = false;
}

void CPU::fetch()
{

	bool cacheable;
	uint32 real_pc;
	uint32 fetch_instr;

	

	pc += 4;
	real_pc = cpzero->address_trans(pc,INSTFETCH,&cacheable, this);

	if (catch_exception) {
		PL_REGS[IF_STAGE] = new PipelineRegs(pc, NOP_INSTR);
		PL_REGS[IF_STAGE]->excBuf.emplace_back(exc_signal);
		//reset signal
		catch_exception = false;
	} else {
		// Fetch next instruction.
		if (cacheable) {
			if (cpzero->caches_swapped()) {
				fetch_instr = dcache->fetch_word(mem, real_pc,INSTFETCH, this);
			} else {
				fetch_instr = icache->fetch_word(mem, real_pc,INSTFETCH, this);
			}
		} else {
			fetch_instr = mem->fetch_word(real_pc,INSTFETCH,this);
		}

		PL_REGS[IF_STAGE] = new PipelineRegs(pc, fetch_instr);

		// Disassemble the instruction, if the user requested it.
		if (opt_instdump) {
			fprintf(stderr,"PC=0x%08x [%08x]\t%08x ",pc,real_pc,fetch_instr);
			machine->disasm->disassemble(pc,fetch_instr);
		}
	}

}


void CPU::decode()
{
	static alu_funcpr execTable[64] = {
		&CPU::funct_ctrl, &CPU::bcond_exec, &CPU::j_exec, &CPU::jal_exec, &CPU::beq_exec, &CPU::bne_exec, &CPU::blez_exec, &CPU::bgtz_exec,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	};

	/* To forward from ex_stage & mem_stage to this for control OPs
	mem_stage, ex_stage must be processed before this*/

	PipelineRegs *preg = PL_REGS[ID_STAGE];
	uint32 dec_instr = preg->instr;
	uint16 dec_opcode = opcode(dec_instr);

	bool control_flag = execTable[dec_opcode] != NULL;

	//decode first source
	switch (dec_opcode) {
		case OP_SPECIAL:
			preg->src_a = firstSrcDecSpecialTable[funct(dec_instr)](dec_instr);
			break;
		case OP_BCOND:
			preg->src_a = firstSrcDecBcondTable[rt(dec_instr)](dec_instr);
			break;
		case OP_COP0:
		case OP_COP1:
		case OP_COP2:
		case OP_COP3:
			preg->src_a = rs(dec_instr) == COP_OP_MTC ? rt(dec_instr) : NONE_REG;
			break;
		default:
			preg->src_a = firstSrcDecTable[dec_opcode](dec_instr);
	}

	//decode second source
	switch (dec_opcode) {
		case OP_SPECIAL:
			preg->src_b = secondSrcDecSpecialTable[funct(dec_instr)](dec_instr);
			break;
		default:
			preg->src_b = secondSrcDecTable[dec_opcode](dec_instr);
	}

	//decode destination
	switch (dec_opcode) {
		case OP_SPECIAL:
			preg->dst = dstDecSpecialTable[funct(dec_instr)](dec_instr);
			break;
		case OP_COP0:
		case OP_COP1:
		case OP_COP2:
		case OP_COP3:
			preg->dst = rs(dec_instr) == COP_OP_MFC ? rt(dec_instr) : NONE_REG;
			break;
		case OP_BCOND:
			preg->dst = (rt(dec_instr) == BCONDE_BLTZAL
							|| rt(dec_instr) == BCONDE_BGEZAL) ? RA_REG : NONE_REG;
			break;
		default:
			preg->dst = dstDecTable[dec_opcode](dec_instr);
	}
	if (preg->dst == 0) preg->dst = NONE_REG;

	//decode value A
	bool data_hazard = false;
	if (preg->src_a != NONE_REG) {
		//forwarding for A
		if (preg->src_a == PL_REGS[EX_STAGE]->dst) {
			if (PL_REGS[EX_STAGE]->mem_read_op) data_hazard = true;
			preg->alu_src_a = PL_REGS[EX_STAGE]->w_reg_data;
		} else if (preg->src_a == PL_REGS[MEM_STAGE]->dst) {
			preg->alu_src_a = PL_REGS[MEM_STAGE]->w_reg_data;
		} else if (preg->src_a == PL_REGS[WB_STAGE]->dst) {
			preg->alu_src_a = PL_REGS[WB_STAGE]->w_reg_data;
		} else {
			preg->alu_src_a = &reg[preg->src_a];
		}
	}

	//decode shamt & imm
	if (dec_opcode == OP_SPECIAL) {
		if (shamt_flag[funct(dec_instr)]) {
			preg->shamt = (uint32)shamt(dec_instr);
		}
	} else {
		if (imm_flag[dec_opcode]) {
			preg->imm = (uint32)immed(dec_instr);
		} else if (s_imm_flag[dec_opcode]) {
			preg->imm = (uint32)s_immed(dec_instr);
		}
	}

	//decode value B
	if (preg->src_b != NONE_REG) {
		//forwarding for B
		if (preg->src_b == PL_REGS[EX_STAGE]->dst) {
			preg->alu_src_b = PL_REGS[EX_STAGE]->w_reg_data;
			if (PL_REGS[EX_STAGE]->mem_read_op) data_hazard = true;
		} else if (preg->src_b == PL_REGS[MEM_STAGE]->dst) {
			preg->alu_src_b = PL_REGS[MEM_STAGE]->w_reg_data;
		} else if (preg->src_b == PL_REGS[WB_STAGE]->dst) {
			preg->alu_src_b = PL_REGS[WB_STAGE]->w_reg_data;
		} else {
			preg->alu_src_b = &reg[preg->src_b];
		}
	} else {
		if (dec_opcode == OP_SPECIAL) {
			if (shamt_flag[funct(dec_instr)]) {
				preg->alu_src_b = &(preg->shamt);
			}
		} else {
			if (imm_flag[dec_opcode] || s_imm_flag[dec_opcode]) {
				preg->alu_src_b = &(preg->imm);
			}
		}
	}

	//decode LWL/LWR
	if (dec_opcode == OP_LWL || dec_opcode == OP_LWR) {
		if (preg->dst == PL_REGS[EX_STAGE]->dst) {
			//forwarding
			preg->lwrl_reg_prev = PL_REGS[EX_STAGE]->w_reg_data;
		} else {
			preg->lwrl_reg_prev = &reg[preg->dst];
		}
	}

	//decode write reg data
	if (preg->dst != NONE_REG) {
		if (mem_read_flag[dec_opcode]) {
			//load operation
			preg->mem_read_op = true;
			preg->w_reg_data = &(preg->r_mem_data);
		} else {
			preg->w_reg_data = &(preg->result);
		}
	}

	//decode write mem data
	if (mem_write_flag[dec_opcode]) {
		if (rt(dec_instr) == PL_REGS[EX_STAGE]->dst) {
			//fowrding
			preg->w_mem_data = PL_REGS[EX_STAGE]->w_reg_data;
		} else {
			preg->w_mem_data = &reg[rt(dec_instr)];
		}
	}

	if (data_hazard) {
		fprintf(stderr, "Data Hazard stall one cycle\n");
	}

	// execute control step
	if (control_flag) {
		(this->*execTable[dec_opcode])();
	}

}

void CPU::execute()
{
	static alu_funcpr execTable[64] = {
		&CPU::funct_exec, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		&CPU::add_exec, &CPU::addu_exec, &CPU::slt_exec, &CPU::sltu_exec, &CPU::and_exec, &CPU::or_exec, &CPU::xor_exec, &CPU::lui_exec,
		&CPU::cpzero_exec, &CPU::cpone_exec, &CPU::cptwo_exec, &CPU::cpthree_exec, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		&CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, NULL,
		&CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, NULL, NULL, &CPU::addu_exec, NULL,
		&CPU::lwc1_exec, &CPU::lwc2_exec, &CPU::lwc3_exec, NULL, NULL, NULL, NULL,
		&CPU::swc1_exec, &CPU::swc2_exec, &CPU::swc3_exec, NULL, NULL, NULL, NULL
	};
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	uint32 exec_instr = preg->instr;
	if (execTable[opcode(exec_instr)]) {
		(this->*execTable[opcode(exec_instr)])();
	}
	//store exception
	if (catch_exception) {
		preg->excBuf.emplace_back(exc_signal);
		//reset signal
		catch_exception = false;
	}
}

void CPU::mem_access()
{
	PipelineRegs *preg = PL_REGS[MEM_STAGE];
	uint32 mem_instr = preg->instr;
	uint16 mem_opcode = opcode(mem_instr);

	uint32 phys;
	bool cacheable;
	Cache *cache = cpzero->caches_swapped() ? icache : dcache;
	uint8 which_byte;
	uint32 wordvirt;

	uint32 vaddr = preg->result;

	//Address Error check
	switch (mem_opcode) {
		case OP_LH:
		case OP_LHU:
		case OP_SH:
			if (vaddr % 2 != 0) {
				preg->excCode = AdEL;
			}
			break;
		case OP_LW:
		case OP_SW:
			if (vaddr % 4 != 0) {
				preg->excCode = AdEL;
			}
		break;
	}

	if (mem_write_flag[mem_opcode]) {
		//Store
		uint32 data = *(preg->w_mem_data);
		phys = cpzero->address_trans(vaddr, DATASTORE, &cacheable, this);
		if (exception_pending) { fprintf(stderr, "not imple\n");}
		if (cacheable) {
			//store from cache
			switch (mem_opcode) {
				case OP_SB:
					data &= 0x0FF;
					cache->store_byte(mem, phys, data, this);
					break;
				case OP_SH:
					data &= 0x0FFFF;
					cache->store_halfword(mem, phys, data, this);
					break;
				case OP_SWL:
					wordvirt = vaddr & ~0x03UL;
					which_byte = vaddr & 0x03UL;
					fprintf(stderr, "not imple\n");
					break;
				case OP_SW:
					cache->store_word(mem, phys, data, this);
					break;
				case OP_SWR:
					wordvirt = vaddr & ~0x03UL;
					which_byte = vaddr & 0x03UL;
					fprintf(stderr, "not imple\n");
					break;
			}
		} else {
			//store from mapper
			switch (mem_opcode) {
				case OP_SB:
					mem->store_byte(phys, data, this);
					break;
				case OP_SH:
					mem->store_halfword(phys, data, this);
					break;
				case OP_SWL:
				case OP_SW:
				case OP_SWR:
					mem->store_word(phys, data, this);
				break;
			}
		}
	} else if (mem_read_flag[mem_opcode]) {
		//Load
		/* Translate virtual address to physical address. */
		phys = cpzero->address_trans(vaddr, DATALOAD, &cacheable, this);
		if (exception_pending) { 
			fprintf(stderr, "not imple ld %X\n", vaddr);
			fprintf(stderr, "PC  0x%X\n", preg->pc);
		}
		if (cacheable) {
			//load from cache
			switch (mem_opcode) {
				case OP_LB:
				case OP_LBU:
					preg->r_mem_data = cache->fetch_byte(mem, phys, this);
					break;
				case OP_LH:
				case OP_LHU:
					preg->r_mem_data = cache->fetch_halfword(mem, phys, this);
					break;
				case OP_LWL:
				case OP_LW:
				case OP_LWR:
					preg->r_mem_data = cache->fetch_word(mem, phys, DATALOAD, this);
				break;
			}
		} else {
			//load from mapper
			switch (mem_opcode) {
				case OP_LB:
				case OP_LBU:
					preg->r_mem_data = mem->fetch_byte(phys, this);
					break;
				case OP_LH:
				case OP_LHU:
					preg->r_mem_data = (int16)mem->fetch_halfword(phys, this);
					break;
				case OP_LWL:
				case OP_LW:
				case OP_LWR:
					preg->r_mem_data = mem->fetch_word(phys, DATALOAD, this);
				break;
			}
		}
		//Check Exception
		//Finalize loaded data
		switch (mem_opcode) {
			case OP_LB:
				preg->r_mem_data = (int8)preg->r_mem_data;
				break;
			case OP_LH:
				preg->r_mem_data = (int16)preg->r_mem_data;
				break;
			case OP_LWL:
				wordvirt = vaddr & ~0x03UL;
				which_byte = vaddr & 0x03;
				preg->r_mem_data = lwl(*(preg->lwrl_reg_prev), preg->r_mem_data, which_byte);
				break;
			case OP_LWR:
				wordvirt = vaddr & ~0x03UL;
				which_byte = vaddr & 0x03;
				preg->r_mem_data = lwr(*(preg->lwrl_reg_prev), preg->r_mem_data, which_byte);
				break;
			case OP_LBU:
				preg->r_mem_data &= 0x000000FF;
				break;
			case OP_LHU:
				preg->r_mem_data &= 0x0000FFFF;
				break;
		}
	}
}

void CPU::reg_commit()
{
	PipelineRegs *preg = PL_REGS[WB_STAGE];

	if (preg->w_reg_data != NULL) {
		reg[preg->dst] = *(preg->w_reg_data);
	}
}

void CPU::step()
{
	static int call_count = 0;
	static Disassembler *disasm = new Disassembler(machine->host_bigendian, stderr);

	// Decrement Random register every clock cycle.
	cpzero->adjust_random();

	//if (call_count > 300) return;
	if (machine->state == vmips::HALT) return;

	/* ################ DECODE STAGE ################ */
	exc_handle(PL_REGS[WB_STAGE]);
	//fprintf(stderr, "\tWB\n");
	reg_commit();
	//fprintf(stderr, "\tMEM\n");
	mem_access();
	//fprintf(stderr, "\tEX\n");
	execute();
	//fprintf(stderr, "\tID\n");
	decode();

	//go ahead pipeline
	for (int i = PIPELINE_STAGES - 1; i > 0; i--) {
		PL_REGS[i] = PL_REGS[i - 1];
	}

	fetch();

	// fprintf(stderr,"PC=0x%08x [%08x]\t%08x ",pc,real_pc,instr);
	// disasm->disassemble(PL_REGS[IF_STAGE]->pc, PL_REGS[IF_STAGE]->instr);

};

// /* dispatching */
// void
// CPU::mystep()
// {
// 	// Table of emulation functions.
// 	static const emulate_funptr opcodeJumpTable[] = {
//         &CPU::funct_emulate, &CPU::regimm_emulate,  &CPU::j_emulate,
//         &CPU::jal_emulate,   &CPU::beq_emulate,     &CPU::bne_emulate,
//         &CPU::blez_emulate,  &CPU::bgtz_emulate,    &CPU::addi_emulate,
//         &CPU::addiu_emulate, &CPU::slti_emulate,    &CPU::sltiu_emulate,
//         &CPU::andi_emulate,  &CPU::ori_emulate,     &CPU::xori_emulate,
//         &CPU::lui_emulate,   &CPU::cpzero_emulate,  &CPU::cpone_emulate,
//         &CPU::cptwo_emulate, &CPU::cpthree_emulate, &CPU::RI_emulate,
//         &CPU::RI_emulate,    &CPU::RI_emulate,      &CPU::RI_emulate,
//         &CPU::RI_emulate,    &CPU::RI_emulate,      &CPU::RI_emulate,
//         &CPU::RI_emulate,    &CPU::RI_emulate,      &CPU::RI_emulate,
//         &CPU::RI_emulate,    &CPU::RI_emulate,      &CPU::lb_emulate,
//         &CPU::lh_emulate,    &CPU::lwl_emulate,     &CPU::lw_emulate,
//         &CPU::lbu_emulate,   &CPU::lhu_emulate,     &CPU::lwr_emulate,
//         &CPU::RI_emulate,    &CPU::sb_emulate,      &CPU::sh_emulate,
//         &CPU::swl_emulate,   &CPU::sw_emulate,      &CPU::RI_emulate,
//         &CPU::RI_emulate,    &CPU::swr_emulate,     &CPU::RI_emulate,
//         &CPU::RI_emulate,    &CPU::lwc1_emulate,    &CPU::lwc2_emulate,
//         &CPU::lwc3_emulate,  &CPU::RI_emulate,      &CPU::RI_emulate,
//         &CPU::RI_emulate,    &CPU::RI_emulate,      &CPU::RI_emulate,
//         &CPU::swc1_emulate,  &CPU::swc2_emulate,    &CPU::swc3_emulate,
//         &CPU::RI_emulate,    &CPU::RI_emulate,      &CPU::RI_emulate,
//         &CPU::RI_emulate
//     };

// 	// Clear exception_pending flag if it was set by a prior instruction.
// 	exception_pending = false;

// 	// Decrement Random register every clock cycle.
// 	cpzero->adjust_random();

//     // Check whether we should start tracing with this instruction.
// 	if (opt_tracing && !tracing && pc == opt_tracestartpc)
// 		start_tracing();

// 	// Save address of instruction responsible for exceptions which may occur.
// 	if (delay_state != DELAYSLOT)
// 		next_epc = pc;

// 	// Get physical address of next instruction.
// 	bool cacheable;
// 	uint32 real_pc = cpzero->address_trans(pc,INSTFETCH,&cacheable, this);
// 	if (exception_pending) {
// 		if (opt_excmsg)
// 			fprintf(stderr,
// 				"** PC address translation caused the exception! **\n");
// 		goto out;
// 	}

// 	// Fetch next instruction.
// 	if (cacheable) {
// 		if (cpzero->caches_swapped()) {
// 			instr = dcache->fetch_word(mem, real_pc,INSTFETCH, this);
// 		} else {
// 			instr = icache->fetch_word(mem, real_pc,INSTFETCH, this);
// 		}
// 	} else {
// 		instr = mem->fetch_word(real_pc,INSTFETCH,this);
// 	}

// 	if (exception_pending) {
// 		if (opt_excmsg)
// 			fprintf(stderr, "** Instruction fetch caused the exception! **\n");
// 		goto out;
// 	}

// 	// Disassemble the instruction, if the user requested it.
// 	if (opt_instdump) {
// 		fprintf(stderr,"PC=0x%08x [%08x]\t%08x ",pc,real_pc,instr);
// 		machine->disasm->disassemble(pc,instr);
// 	}

// 	// Perform first half of trace recording for this instruction.
// 	if (tracing)
// 		write_trace_record_1 (pc, instr);

// 	// Check for a (hardware or software) interrupt.
// 	if (cpzero->interrupt_pending()) {
// 		exception(Int);
// 		goto out;
// 	}

// 	// Emulate the instruction by jumping to the appropriate emulation method.
// 	(this->*opcodeJumpTable[opcode(instr)])(instr, pc);

// out:
// 	// Force register zero to contain zero.
// 	reg[reg_zero] = 0;

// 	// Perform second half of trace recording for this instruction.
// 	if (tracing) {
// 		write_trace_record_2 (pc, instr);
// 		if (pc == opt_traceendpc)
// 			stop_tracing();
// 	}

// 	// If an exception is pending, then the PC has already been changed to
// 	// contain the exception vector.  Return now, so that we don't clobber it.
// 	if (exception_pending) {
// 		// Instruction at beginning of exception handler is NOT in delay slot,
// 		// no matter what the last instruction was.
// 		delay_state = NORMAL;
// 		return;
// 	}

// 	// Recall the delay_state values: 0=NORMAL, 1=DELAYING, 2=DELAYSLOT.
// 	// This is what the delay_state values mean (at this point in the code):
// 	// DELAYING: The last instruction caused a branch to be taken.
// 	//  The next instruction is in the delay slot.
// 	//  The next instruction EPC will be PC - 4.
// 	// DELAYSLOT: The last instruction was executed in a delay slot.
// 	//  The next instruction is on the other end of the branch.
// 	//  The next instruction EPC will be PC.
// 	// NORMAL: No branch was executed; next instruction is at PC + 4.
// 	//  Next instruction EPC is PC.

// 	// Update the pc and delay_state values.
// 	pc += 4;
// 	if (delay_state == DELAYSLOT)
// 		pc = delay_pc;
// 	delay_state = (delay_state << 1) & 0x03; // 0->0, 1->2, 2->0
// }

// void
// CPU::funct_emulate(uint32 instr, uint32 pc)
// {
// 	static const emulate_funptr functJumpTable[] = {
// 		&CPU::sll_emulate,     &CPU::RI_emulate,
// 		&CPU::srl_emulate,     &CPU::sra_emulate,
// 		&CPU::sllv_emulate,    &CPU::RI_emulate,
// 		&CPU::srlv_emulate,    &CPU::srav_emulate,
// 		&CPU::jr_emulate,      &CPU::jalr_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::syscall_emulate, &CPU::break_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::mfhi_emulate,    &CPU::mthi_emulate,
// 		&CPU::mflo_emulate,    &CPU::mtlo_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::mult_emulate,    &CPU::multu_emulate,
// 		&CPU::div_emulate,     &CPU::divu_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::add_emulate,     &CPU::addu_emulate,
// 		&CPU::sub_emulate,     &CPU::subu_emulate,
// 		&CPU::and_emulate,     &CPU::or_emulate,
// 		&CPU::xor_emulate,     &CPU::nor_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::slt_emulate,     &CPU::sltu_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate,
// 		&CPU::RI_emulate,      &CPU::RI_emulate
// 	};
// 	(this->*functJumpTable[funct(instr)])(instr, pc);
// }

// void
// CPU::regimm_emulate(uint32 instr, uint32 pc)
// {
// 	switch(rt(instr))
// 	{
// 		case 0: bltz_emulate(instr, pc); break;
// 		case 1: bgez_emulate(instr, pc); break;
// 		case 16: bltzal_emulate(instr, pc); break;
// 		case 17: bgezal_emulate(instr, pc); break;
// 		default: exception(RI); break; /* reserved instruction */
// 	}
// }

/* Debug functions.
 *
 * These functions are primarily intended to support the Debug class,
 * which interfaces with GDB's remote serial protocol.
 */

/* Copy registers into an ASCII-encoded packet of hex numbers to send
 * to the remote GDB, and return the packet (allocated with malloc).
 */
char *
CPU::debug_registers_to_packet(void)
{
	char *packet = new char [PBUFSIZ];
	int i, r;

	/* order of regs:  (gleaned from gdb/gdb/config/mips/tm-mips.h)
	 * 
	 * cpu->reg[0]...cpu->reg[31]
	 * cpzero->reg[Status]
	 * cpu->lo
	 * cpu->hi
	 * cpzero->reg[BadVAddr]
	 * cpzero->reg[Cause]
	 * cpu->pc
	 * fpu stuff: 35 zeroes (Unimplemented registers read as
	 *  all bits zero.)
	 */
	packet[0] = '\0';
	r = 0;
	for (i = 0; i < 32; i++) {
		debug_packet_push_word(packet, reg[i]); r++;
	}
	uint32 sr, bad, cause;
	cpzero->read_debug_info(&sr, &bad, &cause);
	debug_packet_push_word(packet, sr); r++;
	debug_packet_push_word(packet, lo); r++;
	debug_packet_push_word(packet, hi); r++;
	debug_packet_push_word(packet, bad); r++;
	debug_packet_push_word(packet, cause); r++;
	debug_packet_push_word(packet, pc); r++;
	for (; r < 90; r++) { /* unimplemented regs at end */
		debug_packet_push_word(packet, 0);
	}
	return packet;
}

/* Copy the register values in the ASCII-encoded hex PACKET received from
 * the remote GDB and store them in the appropriate registers.
 */
void
CPU::debug_packet_to_registers(char *packet)
{
	int i;

	for (i = 0; i < 32; i++)
		reg[i] = machine->dbgr->packet_pop_word(&packet);
	uint32 sr, bad, cause;
	sr = machine->dbgr->packet_pop_word(&packet);
	lo = machine->dbgr->packet_pop_word(&packet);
	hi = machine->dbgr->packet_pop_word(&packet);
	bad = machine->dbgr->packet_pop_word(&packet);
	cause = machine->dbgr->packet_pop_word(&packet);
	pc = machine->dbgr->packet_pop_word(&packet);
	cpzero->write_debug_info(sr, bad, cause);
}

/* Returns the exception code of any pending exception, or 0 if no
 * exception is pending.
 */
uint8
CPU::pending_exception(void)
{
	uint32 sr, bad, cause;

	if (! exception_pending) return 0;
	cpzero->read_debug_info(&sr, &bad, &cause);
	return ((cause & Cause_ExcCode_MASK) >> 2);
}

/* Sets the program counter register to the value given in NEWPC. */
void
CPU::debug_set_pc(uint32 newpc)
{
	pc = newpc;
}

/* Returns the current program counter register. */
uint32
CPU::debug_get_pc(void)
{
	return pc;
}

void
CPU::debug_packet_push_word(char *packet, uint32 n)
{
	if (!opt_bigendian) {
		n = Mapper::swap_word(n);
	}
	char packetpiece[10];
	sprintf(packetpiece, "%08x", n);
	strcat(packet, packetpiece);
}

void
CPU::debug_packet_push_byte(char *packet, uint8 n)
{
	char packetpiece[4];
	sprintf(packetpiece, "%02x", n);
	strcat(packet, packetpiece);
}

/* Fetch LEN bytes starting from virtual address ADDR into the packet
 * PACKET, sending exceptions (if any) to CLIENT. Returns -1 if an exception
 * was encountered, 0 otherwise.  If an exception was encountered, the
 * contents of PACKET are undefined, and CLIENT->exception() will have
 * been called.
 */
int
CPU::debug_fetch_region(uint32 addr, uint32 len, char *packet,
	DeviceExc *client)
{
	uint8 byte;
	uint32 real_addr;
	bool cacheable = false;

	for (; len; addr++, len--) {
		real_addr = cpzero->address_trans(addr, DATALOAD, &cacheable, client);
		/* Stop now and return an error code if translation
		 * caused an exception.
		 */
		if (client->exception_pending) {
			return -1;
		}
		byte = mem->fetch_byte(real_addr, client);
		/* Stop now and return an error code if the fetch
		 * caused an exception.
		 */
		if (client->exception_pending) {
			return -1;
		}
		debug_packet_push_byte(packet, byte);
	}
	return 0;
}

/* Store LEN bytes starting from virtual address ADDR from data in the packet
 * PACKET, sending exceptions (if any) to CLIENT. Returns -1 if an exception
 * was encountered, 0 otherwise.  If an exception was encountered, the
 * contents of the region of virtual memory from ADDR to ADDR+LEN are undefined,
 * and CLIENT->exception() will have been called.
 */
int
CPU::debug_store_region(uint32 addr, uint32 len, char *packet,
	DeviceExc *client)
{
	uint8 byte;
	uint32 real_addr;
	bool cacheable = false;

	for (; len; addr++, len--) {
		byte = machine->dbgr->packet_pop_byte(&packet);
		real_addr = cpzero->address_trans(addr, DATALOAD, &cacheable, client);
		if (client->exception_pending) {
			return -1;
		}
		mem->store_byte(real_addr, byte, client);
		if (client->exception_pending) {
			return -1;
		}
	}
	return 0;
}
