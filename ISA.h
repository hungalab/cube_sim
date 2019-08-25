#include "cpu.h"
#include <map>

using operand_decodepr = uint16 (*)(uint32);
typedef void (CPU::*alu_funcpr)(void);

/* OPCODEs for R3000 */
#define OP_SPECIAL	0
#define OP_BCOND	1
#define OP_J		2
#define OP_JAL		3
#define OP_BEQ		4
#define OP_BNE		5
#define OP_BLEZ		6
#define OP_BGTZ		7
#define OP_ADDI		8
#define OP_ADDIU	9
#define OP_SLTI		10
#define OP_SLTIU	11
#define OP_ANDI		12
#define OP_ORI		13
#define OP_XORI		14
#define OP_LUI		15
#define OP_COP0		16
#define OP_COP1		17
#define OP_COP2		18
#define OP_COP3		19
/* OPCODE 20~31 are reserved */
#define OP_LB		32
#define OP_LH		33
#define OP_LWL		34
#define OP_LW		35
#define OP_LBU		36
#define OP_LHU		37
#define OP_LWR		38
/* OPCODE 39 is reserved */
#define OP_SB		40
#define OP_SH		41
#define OP_SWL		42
#define OP_SW		43
#define OP_SWR		46
/* OPCODE 44,45,47 are reserved */
#define OP_LWC0		48
#define OP_LWC1		49
#define OP_LWC2		50
#define OP_LWC3		51
/* OPCODE 52~55 are reserved */
#define OP_SWC0		56
#define OP_SWC1		57
#define OP_SWC2		58
#define OP_SWC3		59
/* OPCODE 60~63 are reserved*/

/* Funct Code for SPECIAL */
#define FUNCT_SLL	0
#define FUNCT_SRL	2
#define FUNCT_SRA	3
#define FUNCT_SLLV	4
#define FUNCT_SRLV	6
#define FUNCT_SRAV	7
/* FUNCT 1 and 5 are reserved */
#define FUNCT_JR		8
#define FUNCT_JALR		9
#define FUNCT_SYSCALL	12
#define FUNCT_BREAK		13
/* FUNCT 10,11,14,15 are reserved */
#define FUNCT_MFHI		16
#define FUNCT_MTHI		17
#define FUNCT_MFLO		18
#define FUNCT_MTLO		19
/* FUNCT 20~23 are reserved */
#define FUNCT_MULT		24
#define FUNCT_MULTU		25
#define FUNCT_DIV		26
#define FUNCT_DIVU		27
/* FUNCT 28~31 are reserved */
#define FUNCT_ADD		32
#define FUNCT_ADDU		33
#define FUNCT_SUB		34
#define FUNCT_SUBU		35
#define FUNCT_AND		36
#define FUNCT_OR		37
#define FUNCT_XOR		38
#define FUNCT_NOR		39
#define FUNCT_SLT		42
#define FUNCT_SLTU		43
/* FUNCT 40,41, 44~63 are reserved */

/* FUNCT for CP0 OP (rs > 15) */
#define FUNCT_TLBR	1
#define FUNCT_TLBWI	2
#define FUNCT_TLBWR	6
#define FUNCT_TLBP	8
#define FUNCT_RFE	16

/* BCOND TYPE */
#define BCOND_BLTZ		0
#define BCOND_BGEZ		1
#define BCONDE_BLTZAL	16
#define BCONDE_BGEZAL	17

/* OPCODEs for CP OP (rs) (rs < 16) */
#define COP_OP_MFC	0
#define COP_OP_MTC	4
#define COP_OP_BC	8
/* for CP OP Branch (rt)*/
#define COP_BCF		0
#define COP_BCT		1

static operand_decodepr firstSrcDecSpecialTable[64] = {
	CPU::rt, CPU::no_reg, CPU::rt, CPU::rt, CPU::rt, CPU::no_reg, CPU::rt, CPU::rt,
	CPU::rs, CPU::rs, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::rd, CPU::no_reg, CPU::rd, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::rs,
	CPU::no_reg, CPU::no_reg, CPU::rs, CPU::rs, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg
};

static operand_decodepr firstSrcDecBcondTable[32] = {
	CPU::rs, CPU::rs, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::rs, CPU::rs, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg
};

static operand_decodepr firstSrcDecTable[64] = {
	NULL, NULL, CPU::no_reg, CPU::no_reg, CPU::rs, CPU::rs, CPU::rs, CPU::rs,
	CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::no_reg,
	NULL, NULL, NULL, NULL, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::rs, CPU::rs,CPU::no_reg,
	CPU::rs, CPU::rs, CPU::rs, CPU::rs,CPU::no_reg,CPU::no_reg, CPU::rs, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg
};

static operand_decodepr secondSrcDecSpecialTable[64] = {
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::rs, CPU::no_reg, CPU::rs, CPU::rs,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt,
	CPU::no_reg, CPU::no_reg, CPU::rt, CPU::rt,  CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg
};

static operand_decodepr secondSrcDecTable[64] = {
	NULL, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::rt, CPU::rt, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg
};

static operand_decodepr dstDecSpecialTable[64] = {
	CPU::rd, CPU::no_reg, CPU::rd, CPU::rd, CPU::rd, CPU::no_reg, CPU::rd, CPU::rd,
	CPU::no_reg, CPU::ra_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::rd, CPU::no_reg, CPU::rd, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::rd, CPU::rd, CPU::rd, CPU::rd, CPU::rd, CPU::rd, CPU::rd, CPU::rd,
	CPU::no_reg, CPU::no_reg, CPU::rd, CPU::rd, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg
};

static operand_decodepr dstDecTable[64] = {
	NULL, CPU::no_reg, CPU::no_reg, CPU::ra_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt,
	NULL, NULL, NULL, NULL, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::rt, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg,
	CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg, CPU::no_reg
};


//indexed by funct
//flag of using shamt
static bool shamt_flag[64] = {
	true , false, true , true , false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false
};

//indexed by opcode
//flag of signed imm
static bool s_imm_flag[64] = {
	false, true , false, false, true , true , true , true ,
	true , true , true , true , false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	true , true , true , true , true , true , true , false,
	true , true , true , true , false, false, true , false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false
};

//indexed by opcode
//flag of unsigned imm
static bool imm_flag[64] = {
	false, false, false, false, false, false, false, false,
	false, false, false, false, true , true , true , true ,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false
};

//indexed by opcode
//flag of mem write
static bool mem_write_flag[64] = {
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	true , true , true , true , false, false, true , false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false
};

//indexed by opcode
//flag of mem write
static bool mem_read_flag[64] = {
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	true , true , true , true , true , true , true , false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false
};

//Reserved Instruction Table
//indexed by opcode
static bool RI_flag[64] = {
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, true , true , true , true ,
	true , true , true , true , true , true , true , true ,
	false, false, false, false, false, false, false, true ,
	false, false, false, false, true , true , false, true ,
	false, false, false, false, true , true , true , true ,
	false, false, false, false, true , true , true , true
};

//indexed by funct
static bool RI_special_flag[64] = {
	false, true , false, false, false, true , false, false,
	false, false, true , true , false, false, true , true ,
	false, false, false, false, true , true , true , true ,
	false, false, false, false, true , true , true , true ,
	false, false, false, false, false, false, false, false,
	true , true , false, false, true , true , true , true ,
	true , true , true , true , true , true , true , true ,
	true , true , true , true , true , true , true , true
};

//indexed by funct
static bool mul_div_flag[64] = {
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	true , false, true , false, false, false, false, false,
	true , true , true , true , false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false,
	false, false, false, false, false, false, false, false
};

std::map<int, int> mul_div_delay = {
            {FUNCT_MULTU, 1},
            {FUNCT_MULT, 1},
            {FUNCT_DIV, 1},
            {FUNCT_DIVU, 1}
};