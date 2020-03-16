#ifndef _SNACCCORE_H_
#define _SNACCCORE_H_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>
#include <accelerator.h>
#include <acceleratorcore.h>
#include <memolymodule.h>

class AcceleratorCore;

class SNACCCore : public AcceleratorCore{
private:
  SNACCComponents:SNACCMemoryModule *dmem,*rbuf,*inst,*lut;


//////////////////////////////
  void Unknown();

  void RTypeMemory();
  void RTypeArithmetic();
  void RTypeSimd();
  void Loadi();
  void Bneq();
  void Jump();
  void Mad();
  void Madlp();
  void Setcr();
  void Addi();
  void Subi();
  void Sll();
  void Srl();
  void Sra();

  // RType 0: Arithmetic instructions
  void Nop();
  void Mov();
  void Add();
  void Sub();
  void Mul();
  void And();
  void Or();
  void Xor();
  void Neg();

  // RType 1: Memory instructions
  void Halt();
  void Loadw();
  void Storew();
  void Loadh();
  void Storeh();
  void Readcr();
  void Dbchange();
  void Dma();

  // RType 2: SIMD instructions
  void Loadv();

  void DoMad(Fixed32* tr0, Fixed32* tr1);
  void DoMadApplyToRegisters(const Fixed32& tr0, const Fixed32& tr1);
  void DoMadLoop();

  // Memory Read / Write based on fixed virtual mapping
  uint8_t ReadMemory8(uint32_t address);
  void WriteMemory8(uint32_t address, uint8_t value);
////////////////////////////////


public:
  void Coresetup();
  void step();
};

#endif //_SNACCCORE_H_
