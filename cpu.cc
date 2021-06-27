/* MIPS R3000 CPU emulation modified to simulate traditional 5-stage pipeline, memory access stall (due to e.g., cache miss)
    Original work Copyright 2001, 2002, 2003, 2004 Brian R. Gaeke.
    Modified work Copyright (c) 2021 Amano laboratory, Keio University.
        Modifier: Takuya Kojima

    This file is part of CubeSim, a cycle accurate simulator for 3-D stacked system.
    It is derived from a source code of VMIPS project under GPLv2.

    CubeSim is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    CubeSim is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CubeSim.  If not, see <https://www.gnu.org/licenses/>.
*/

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
	mem_read_op = delay_slot = false;
	src_a = src_b = dst = NONE_REG;
	result = r_mem_data = imm = shamt = 0;
}

PipelineRegs::~PipelineRegs()
{
	excBuf.clear();
}

CPU::CPU (Mapper &m, IntCtrl &i, int cpuid)
  : tracing (false), last_epc (0), last_prio (0), mem (&m),
    cpzero (new CPZero (this, &i, cpuid)), fpu (0), delay_state (NORMAL),
    mul_div_remain(0), suspend(false), icache(NULL), dcache(NULL)
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
	mem_bandwidth = machine->opt->option("mem_bandwidth")->num;

	exception_pending = false;
	volatilize_pipeline();
}

CPU::~CPU ()
{
	if (opt_tracing)
		close_trace_file ();
	if (icache) delete icache;
	if (dcache) delete dcache;
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
	icache = new Cache(mem, opt_icachebnum, opt_icachebsize, opt_icacheway);	/* 64Byte * 64Block * 2way = 8KB*/
	dcache = new Cache(mem, opt_dcachebnum, opt_dcachebsize, opt_dcacheway);
	//fill NOP for each Pipeline stage
	volatilize_pipeline();
}

void CPU::volatilize_pipeline()
{
	for (int i = 0; i < PIPELINE_STAGES; i++) {
		PL_REGS[i] = new PipelineRegs(pc + 4, NOP_INSTR);
	}
	late_preg = new PipelineRegs(pc + 4, NOP_INSTR);
	late_late_preg = new PipelineRegs(pc + 4, NOP_INSTR);
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
	exception_pending = true;
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
	if (preg->delay_slot) {
		epc = preg->pc - 4;
	} else {
		epc = preg->pc;
	}

	/* reset cache status */
	icache->reset_stat();
	dcache->reset_stat();
	mem->release_bus(this);

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

void CPU::fetch(bool& fetch_miss, bool data_miss)
{
	bool cacheable;
	uint32 real_pc;
	uint32 fetch_instr;
	Cache *cache = cpzero->caches_swapped() ? dcache : icache;

	fetch_miss = true;

	if (pc % 4 != 0) {
		//Addr Error
		exception(AdEL);
	} else {
		real_pc = cpzero->address_trans(pc,INSTFETCH,&cacheable, this);
	}

	if (exception_pending) {
		PL_REGS[IF_STAGE] = new PipelineRegs(pc, NOP_INSTR);
		PL_REGS[IF_STAGE]->excBuf.emplace_back(exc_signal);
		//reset signal
		exception_pending = false;
	} else {
		// Fetch next instruction.
		if (cacheable) {
			fetch_miss = !cache->ready(real_pc);
			if (fetch_miss & !data_miss) {
				cache->request_block(real_pc, INSTFETCH, this);
			}
		} else {
			if (mem->acquire_bus(this)) {
				fetch_miss = !mem->ready(real_pc,INSTFETCH,this);
				if (fetch_miss & !data_miss) {
					mem->request_word(real_pc,INSTFETCH,this);
				}
			}
		}
		// in case of no stall
		if (!fetch_miss) {
			if (cacheable) {
				fetch_instr = cache->fetch_word(real_pc,INSTFETCH, this);
			} else {
				fetch_instr = mem->fetch_word(real_pc,INSTFETCH,this);
				mem->release_bus(this);
			}

			PL_REGS[IF_STAGE] = new PipelineRegs(pc, fetch_instr);

			// Disassemble the instruction, if the user requested it.
			if (opt_instdump) {
				fprintf(stderr,"PC=0x%08x [%08x]\t%08x ",pc,real_pc,fetch_instr);
				machine->disasm->disassemble(pc,fetch_instr);
			}
			//check exception
			if (exception_pending) {
				PL_REGS[IF_STAGE] = new PipelineRegs(pc, NOP_INSTR);
				PL_REGS[IF_STAGE]->excBuf.emplace_back(exc_signal);
				//reset signal
				exception_pending = false;
			}
		}
	}

}


void CPU::pre_decode(bool& data_hazard)
{

	PipelineRegs *preg = PL_REGS[ID_STAGE];
	uint32 dec_instr = preg->instr;
	uint16 dec_opcode = opcode(dec_instr);

	// check if it is Reserved Instr
	//decode first source
	switch (dec_opcode) {
		case OP_SPECIAL:
			if (RI_special_flag[funct(dec_instr)]) {
				exception(RI);
				preg->instr = NOP_INSTR;
			}
			break;
		case OP_CACHE:
			if (RI_cache_op_flag[rt(dec_instr)]) {
				exception(RI);
				preg->instr = NOP_INSTR;
			}
		default:
			if (RI_flag[dec_opcode]) {
				exception(RI);
				preg->instr = NOP_INSTR;
			}
	}

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
	data_hazard = false;
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

	//store exception
	if (exception_pending) {
		preg->excBuf.emplace_back(exc_signal);
		//reset signal
		exception_pending = false;
	}
}

void CPU::decode()
{
	static alu_funcpr execTable[64] = {
		&CPU::funct_ctrl, &CPU::bcond_exec, &CPU::j_exec, &CPU::jal_exec, &CPU::beq_exec, &CPU::bne_exec, &CPU::blez_exec, &CPU::bgtz_exec,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		&CPU::cpzero_exec, &CPU::cpone_exec, &CPU::cptwo_exec, &CPU::cpthree_exec, NULL, NULL, NULL, NULL,
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
	uint16 dec_opcode = opcode(preg->instr);

	// execution on ID stage
	// branch & cop instr
	bool exec_flag = (execTable[dec_opcode] != NULL);

	if (suspend & (cop_remain == 0)) {
		// finish cop instr
		suspend = false;
	} else if (cop_flag[dec_opcode] & !suspend) {
		// start cop instr
		suspend = true;
		cop_remain = CPZERO_OP_SUSPEND;
		return;
	} else if (suspend) {
		return;
	}

	// execute control step
	if (exec_flag) {
		(this->*execTable[dec_opcode])();
	}

	//store exception (cpzero)
	if (exception_pending) {
		preg->excBuf.emplace_back(exc_signal);
		//reset signal
		exception_pending = false;
	}

}

void CPU::pre_execute(bool& interlock)
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	uint32 exec_instr = preg->instr;

	//detect interlock
	interlock = ((opcode(exec_instr) == OP_SPECIAL)
					& mul_div_flag[funct(exec_instr)] & (mul_div_remain > 0));

}

void CPU::execute()
{
	static alu_funcpr execTable[64] = {
		&CPU::funct_exec, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		&CPU::add_exec, &CPU::addu_exec, &CPU::slt_exec, &CPU::sltu_exec, &CPU::and_exec, &CPU::or_exec, &CPU::xor_exec, &CPU::lui_exec,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		&CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, NULL,
		&CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, &CPU::addu_exec, NULL, NULL, &CPU::addu_exec, &CPU::addu_exec,
		&CPU::lwc1_exec, &CPU::lwc2_exec, &CPU::lwc3_exec, NULL, NULL, NULL, NULL,
		&CPU::swc1_exec, &CPU::swc2_exec, &CPU::swc3_exec, NULL, NULL, NULL, NULL
	};
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	uint32 exec_instr = preg->instr;
	if (execTable[opcode(exec_instr)]) {
		(this->*execTable[opcode(exec_instr)])();
	}

	// setup delay count
	if ((opcode(exec_instr) == OP_SPECIAL) & mul_div_flag[funct(exec_instr)]) {
		mul_div_remain = mul_div_delay[funct(exec_instr)];
	}

	//store exception
	if (exception_pending) {
		preg->excBuf.emplace_back(exc_signal);
		//reset signal
		exception_pending = false;
	}

}

void CPU::pre_mem_access(bool& data_miss)
{
	PipelineRegs *preg = PL_REGS[MEM_STAGE];
	uint32 mem_instr = preg->instr;
	uint16 mem_opcode = opcode(mem_instr);
	uint16 cache_opcode;

	uint32 phys;
	bool cacheable;
	Cache *cache = cpzero->caches_swapped() ? icache : dcache;
	uint32 vaddr = preg->result;
	int mode;

	data_miss = false;

	//Address Error check
	switch (mem_opcode) {
		case OP_LH:
		case OP_LHU:
		case OP_SH:
			if (vaddr % 2 != 0) {
				exception(AdEL);
			}
			break;
		case OP_LW:
		case OP_SW:
			if (vaddr % 4 != 0) {
				exception(AdEL);
			}
		case OP_LWL:
		case OP_LWR:
		case OP_SWL:
		case OP_SWR:
			vaddr &= ~0x03UL; //make word alignment
			break;
	}

	//Exception may be occured
	if (mem_write_flag[mem_opcode]) {
		mode = DATASTORE;
	} else if (mem_read_flag[mem_opcode]) {
		mode = DATALOAD;
	}

	if (mem_opcode == OP_CACHE) {
		phys = cpzero->address_trans(vaddr, ANY, &cacheable, this);
		if (!cacheable) {
			exception(AdEL);
		} else {
			cache_opcode = rt(mem_instr);
			cache = cache_op_mux(cache_opcode);
			data_miss = !cache->exec_cache_op(cache_opcode, phys, this);
		}
	} else if (mem_write_flag[mem_opcode] || mem_read_flag[mem_opcode]) {
		phys = cpzero->address_trans(vaddr, mode, &cacheable, this);
		//check stall
		if (cacheable) {
			Cache *cache = cpzero->caches_swapped() ? icache : dcache;
			data_miss = !cache->ready(phys);
			if (data_miss) {
				cache->request_block(phys, mode, this);
			}
		} else {
			if (mem->acquire_bus(this)) {
				data_miss = !mem->ready(phys, mode, this);
				if (data_miss) {
					mem->request_word(phys, mode, this);
				}
			} else {
				data_miss = true;
			}
		}
	}

	//store exception
	if (exception_pending) {
		preg->excBuf.emplace_back(exc_signal);
		//reset signal
		exception_pending = false;
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
	uint32 byte;

	uint32 vaddr = preg->result;

	if (mem_write_flag[mem_opcode]) {
		//if pending execption exists
		if (preg->excBuf.size() > 0) {
			mem->release_bus(this);
			return;
		}
		//Store
		uint32 data = *(preg->w_mem_data);
		phys = cpzero->address_trans(vaddr, DATASTORE, &cacheable, this);
		if (cacheable) {
			//store to cache
			switch (mem_opcode) {
				case OP_SB:
					data &= 0x0FF;
					cache->store_byte(phys, data, this);
					break;
				case OP_SH:
					data &= 0x0FFFF;
					cache->store_halfword(phys, data, this);
					break;
				case OP_SWL:
					wordvirt = vaddr & ~0x03UL;
					which_byte = vaddr & 0x03UL;
					if (opt_bigendian) {
						for(int i = which_byte; i < 4; i++) {
							byte = (data >> (8 * (3 - i))) & 0xFF;
							cache->store_byte(wordvirt + i, byte, this);
						}
					} else {
						//little endian
						for(int i = 0; i <= which_byte; i++) {
							byte = (data >> (8 * i - 8 * which_byte + 24)) & 0xFF;
							cache->store_byte(wordvirt + i, byte, this);
						}
					}
					break;
				case OP_SW:
					cache->store_word(phys, data, this);
					break;
				case OP_SWR:
					wordvirt = vaddr & ~0x03UL;
					which_byte = vaddr & 0x03UL;
					if (opt_bigendian) {
						for(int i = 0; i <= which_byte; i++) {
							byte = (data >> (8 * (which_byte - i))) & 0xFF;
							cache->store_byte(wordvirt + i, byte, this);
						}
					} else {
						//little endian
						for(int i = which_byte; i < 4; i++) {
							byte = (data >> (8 * (i - which_byte))) & 0xFF;
							cache->store_byte(wordvirt + i, byte, this);
						}
					}

					break;
			}
		} else {
			//store to mapper
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
			mem->release_bus(this);
		}
	} else if (mem_read_flag[mem_opcode]) {
		//Load
		/* Translate virtual address to physical address. */
		phys = cpzero->address_trans(vaddr, DATALOAD, &cacheable, this);
		if (cacheable) {
			//load from cache
			switch (mem_opcode) {
				case OP_LB:
				case OP_LBU:
					preg->r_mem_data = cache->fetch_byte(phys, this);
					break;
				case OP_LH:
				case OP_LHU:
					preg->r_mem_data = cache->fetch_halfword(phys, this);
					break;
				case OP_LWL:
				case OP_LWR:
					phys &= ~0x03UL;
				case OP_LW:
					preg->r_mem_data = cache->fetch_word(phys, DATALOAD, this);
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
				case OP_LWR:
					phys &= ~0x03UL;
				case OP_LW:
					preg->r_mem_data = mem->fetch_word(phys, DATALOAD, this);
				break;
			}
			mem->release_bus(this);
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
	// control signals
	bool data_hazard, interlock, fetch_miss, data_miss;

	// Decrement Random register every clock cycle.
	cpzero->adjust_random();

	// Check for a (hardware or software) interrupt.
	if (cpzero->interrupt_pending()) {
		exception(Int);
		PL_REGS[WB_STAGE]->excBuf.emplace_back(exc_signal);
		//reset signal
		exception_pending = false;
	}

	//check exception & stall
	pre_decode(data_hazard);
	pre_mem_access(data_miss);
	pre_execute(interlock);

	//try to fetch next instr
	if (PL_REGS[IF_STAGE] == NULL) {
		fetch(fetch_miss, data_miss);
	} else {
		fetch_miss = false;
	}

	exc_handle(PL_REGS[WB_STAGE]);

	// emulate mult/div unit stall
	if (mul_div_remain > 0) {
		if (--mul_div_remain == 0) {
			//update regs
			hi = hi_write ? hi_temp : hi;
			lo = lo_write ? lo_temp : lo;
			hi_write = lo_write = false;
		}
	}
	// emulate cop unit stall
	if (cop_remain > 0) {
		cop_remain--;
	}

	for (int i = 0; i < mem_bandwidth; i++) {
		dcache->step();
	}

	// dcache brings exceptions
	if (exception_pending) {
		PL_REGS[WB_STAGE]->excBuf.emplace_back(exc_signal);
		exception_pending = false;
		exc_handle(PL_REGS[WB_STAGE]);
	}

	for (int i = 0; i < mem_bandwidth; i++) {
		icache->step();
	}

	// icache brings exceptions
	if (exception_pending) {
		PL_REGS[WB_STAGE]->excBuf.emplace_back(exc_signal);
		exception_pending = false;
		exc_handle(PL_REGS[WB_STAGE]);
	}

	//if no stall/exception, process each stage
	if (!interlock && !data_miss && !fetch_miss) {
		//without any stall, update hardware status like regfile and memory
		reg_commit();
		mem_access();
		execute();
		if (!data_hazard) {
			decode(); //decode must be processed after execute/mem_access due to forwarding
		}
		if (suspend == true || data_hazard == true) {
			//IF stage stay until suspension
			delete late_late_preg;
			late_late_preg = late_preg;
			late_preg = PL_REGS[WB_STAGE];
			for (int i = PIPELINE_STAGES - 1; i > EX_STAGE; i--) {
				PL_REGS[i] = PL_REGS[i - 1];
			}
			PL_REGS[EX_STAGE] = new PipelineRegs(pc, NOP_INSTR);
		} else {
			//go ahead pipeline
			delete late_late_preg;
			late_late_preg = late_preg;
			late_preg = PL_REGS[WB_STAGE];
			for (int i = PIPELINE_STAGES - 1; i > 0; i--) {
				PL_REGS[i] = PL_REGS[i - 1];
			}
			//next pc
			pc += 4;
			//prepare next fetch
			PL_REGS[IF_STAGE] = NULL;
		}

	} else {
		machine->stall_count++;
	}

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
	debug_packet_push_word(packet, PL_REGS[WB_STAGE]->pc); r++;

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
	return PL_REGS[WB_STAGE]->pc;
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

	mem->enable_debug_mode();
	for (; len; addr++, len--) {
		real_addr = cpzero->address_trans(addr, DATALOAD, &cacheable, client);
		/* Stop now and return an error code if translation
		 * caused an exception.
		 */
		if (client->exception_pending) {
			mem->disable_debug_mode();
			return -1;
		}

		if (cacheable) {
			if (icache->ready(real_addr)) {
				byte = icache->fetch_byte(real_addr, client);
			} else if (dcache->ready(real_addr)) {
				byte = dcache->fetch_byte(real_addr, client);
			} else {
				// access memory without cache
				byte = mem->fetch_byte(real_addr, client);
			}
		} else {
			byte = mem->fetch_byte(real_addr, client);
		}

		/* Stop now and return an error code if the fetch
		 * caused an exception.
		 */
		if (client->exception_pending) {
			mem->disable_debug_mode();
			return -1;
		}
		debug_packet_push_byte(packet, byte);
	}
	mem->disable_debug_mode();

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

	mem->enable_debug_mode();
	for (; len; addr++, len--) {
		byte = machine->dbgr->packet_pop_byte(&packet);
		real_addr = cpzero->address_trans(addr, DATALOAD, &cacheable, client);
		if (client->exception_pending) {
			mem->disable_debug_mode();
			return -1;
		}

		if (cacheable) {
			if (icache->ready(real_addr)) {
				icache->store_byte(real_addr,byte, client);
			} else if (dcache->ready(real_addr)) {
				dcache->store_byte(real_addr, byte, client);
			} else {
				// access memory without cache
				mem->store_byte(real_addr, byte, client);
			}
		} else {
			mem->store_byte(real_addr, byte, client);
		}

		if (client->exception_pending) {
			mem->disable_debug_mode();
			return -1;
		}
	}
	mem->disable_debug_mode();
	return 0;
}

/*  ######## emulation each functionality ########   */

// just used for cpzero branch
void
CPU::branch(uint32 instr, uint32 current_pc)
{
    pc = current_pc + (s_immed(instr) << 2); //+4 later
    PL_REGS[IF_STAGE]->delay_slot = true;
}


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


/* Called when the program wants to use coprocessor COPROCNO, and there
 * isn't any implementation for that coprocessor.
 * Results in a Coprocessor Unusable exception, along with an error
 * message being printed if the coprocessor is marked usable in the
 * CP0 Status register.
 */
void
CPU::cop_unimpl (int coprocno, uint32 instr, uint32 pc)
{
	if (cpzero->cop_usable (coprocno)) {
		/* Since they were expecting this to work, the least we
		 * can do is print an error message. */
		fprintf (stderr, "CP%d instruction %x not implemented at pc=0x%x:\n",
				 coprocno, instr, pc);
		machine->disasm->disassemble (pc, instr);
		exception (CpU, ANY, coprocno);
	} else {
		/* It's fair game to just throw an exception, if they
		 * haven't even bothered to twiddle the status bits first. */
		exception (CpU, ANY, coprocno);
	}
}

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
	PL_REGS[IF_STAGE]->delay_slot = true;
}

void CPU::jal_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	pc = (((preg->pc + 4) & 0xf0000000) | (jumptarg(preg->instr) << 2) - 4); //+4 later
	preg->result = preg->pc + 8;
	PL_REGS[IF_STAGE]->delay_slot = true;
}

void CPU::beq_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if (*(preg->alu_src_a) == *(preg->alu_src_b)) {
		pc = preg->pc + (preg->imm << 2); //+4 later
		PL_REGS[IF_STAGE]->delay_slot = true;
	}
}

void CPU::bne_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if (*(preg->alu_src_a) != *(preg->alu_src_b)) {
		pc = preg->pc + (preg->imm << 2); //+4 later
		PL_REGS[IF_STAGE]->delay_slot = true;
	}
}

void CPU::blez_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if (*(preg->alu_src_a) == 0 ||
		 *(preg->alu_src_a) & 0x80000000) {
		pc = preg->pc + (preg->imm << 2); //+4 later
		PL_REGS[IF_STAGE]->delay_slot = true;
	}
}

void CPU::bgtz_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if (*(preg->alu_src_a) != 0 &&
		(*(preg->alu_src_a) & 0x80000000) == 0) {
		pc = preg->pc + (preg->imm << 2); //+4 later
		PL_REGS[IF_STAGE]->delay_slot = true;
	}
}

void CPU::lui_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = *(preg->alu_src_b) << 16;
}

void CPU::cpzero_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	cpzero->cpzero_emulate(preg);
	dcache->cache_isolate(cpzero->caches_isolated());
}

void CPU::cpone_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if (opt_fpu) {
		// FIXME: check cpzero->cop_usable
		fpu->cpone_emulate (preg->instr, preg->pc);
	} else {
		 /*If it's a cfc1 <reg>, $0 then we copy 0 into reg,
		 * which is supposed to mean there is NO cp1...
		 * for now, though, ANYTHING else asked of cp1 results
		 * in the default "unimplemented" behavior. */
		if (cpzero->cop_usable (1) && rs (preg->instr) == 2
                    && rd (preg->instr) == 0) {
			preg->dst = rt (instr);
			preg->result = 0;
			preg->w_reg_data = &(preg->result);
		} else {
			cop_unimpl (1, instr, pc);
		}
    }
}

void CPU::cptwo_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	cop_unimpl (2, preg->instr, preg->pc);
}

void CPU::cpthree_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	cop_unimpl (3, preg->instr, preg->pc);
}

void CPU::lwc1_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	cop_unimpl (1, preg->instr, preg->pc);
}

void CPU::lwc2_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	cop_unimpl (2, preg->instr, preg->pc);
}

void CPU::lwc3_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	cop_unimpl (3, preg->instr, preg->pc);
}

void CPU::swc1_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	cop_unimpl (1, preg->instr, preg->pc);
}

void CPU::swc2_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	cop_unimpl (2, preg->instr, preg->pc);
}

void CPU::swc3_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	cop_unimpl (3, preg->instr, preg->pc);
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
	PL_REGS[IF_STAGE]->delay_slot = true;
}

void CPU::jalr_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	pc = *(preg->alu_src_a) - 4; //+4 later
	preg->result = preg->pc + 8;
	PL_REGS[IF_STAGE]->delay_slot = true;
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
	hi_temp = *(preg->alu_src_a);
	hi_write = true;
}

void CPU::mflo_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	preg->result = lo;
}

void CPU::mtlo_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	lo_temp = *(preg->alu_src_a);
	lo_write = true;
}

void CPU::mult_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	mult64s(&hi_temp, &lo_temp, *(preg->alu_src_a), *(preg->alu_src_b));
	hi_write = true;
	lo_write = true;
}

void CPU::multu_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	mult64(&hi_temp, &lo_temp, *(preg->alu_src_a), *(preg->alu_src_b));
	hi_write = true;
	lo_write = true;
}

void CPU::div_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	int32 signed_a = (int32)(*(preg->alu_src_a));
	int32 signed_b = (int32)(*(preg->alu_src_b));
	lo_temp = signed_a / signed_b;
	hi_temp = signed_a % signed_b;
	hi_write = true;
	lo_write = true;
}

void CPU::divu_exec()
{
	PipelineRegs *preg = PL_REGS[EX_STAGE];
	lo_temp = *(preg->alu_src_a) / *(preg->alu_src_b);
	hi_temp = *(preg->alu_src_a) % *(preg->alu_src_b);
	hi_write = true;
	lo_write = true;
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
		PL_REGS[IF_STAGE]->delay_slot = true;
	}
}

void CPU::bgez_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if ((int32)*(preg->alu_src_a) >= 0) {
		pc = preg->pc + (preg->imm << 2); //+4 later
		PL_REGS[IF_STAGE]->delay_slot = true;
	}
}

// /* As with JAL, BLTZAL and BGEZAL cause RA to get the address of the
//  * instruction two words after the current one (pc + 8).
//  */
void CPU::bltzal_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if ((int32)*(preg->alu_src_a) < 0) {
		pc = preg->pc + 4 + (preg->imm << 2); //+4 later
		PL_REGS[IF_STAGE]->delay_slot = true;
	}
	preg->result = preg->pc + 8;
}

void CPU::bgezal_exec()
{
	PipelineRegs *preg = PL_REGS[ID_STAGE];
	if ((int32)*(preg->alu_src_a) >= 0) {
		pc = preg->pc + (preg->imm << 2); //+4 later
		PL_REGS[IF_STAGE]->delay_slot = true;
	}
	preg->result = preg->pc + 8;
}

Cache* CPU::cache_op_mux(uint32 opcode)
{
	if (cpzero->caches_swapped()) {
		return opcode >= DCACHE_OPCODE_START ? icache : dcache;
	} else {
		return opcode >= DCACHE_OPCODE_START ? dcache : icache;
	}

}
