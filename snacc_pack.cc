#include "snacc_pack.h"

using namespace SNACCComponents;
using namespace std;

SNACCPACK::SNACCPACK():SNACC(){

  wbuf = new SNACCMemoryModule(SNACC_DBANK0_SIZE,SNACC_DWORD_MASK);
  std::vector<SNACCCore> cores(CORE_NUM,SNACCCore(###));
}

SNACCPACK::~SNACCPACK(){


}

void SNACCPACK::Packsetup(){
  SNACCCore
}
