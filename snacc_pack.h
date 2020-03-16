#ifndef _SNACCPACK_H_
#define _SNACCPACK_H_

#include "snacc.h"


class SNACC;


class SNACCPACK : AcceleratorCore{
private:
  SNACCComponents::SNACCMemoryModule *wbuf;
  SNACCCore *core0,*core1,*core2,*core3;
  void step();
public:
  SNACCPACK(){};
  ~SNACCPACK();
  void Packsetup();
  void TaskDistribute(); //ただデータを書くコアに分散させるだけ
  void resultGather(); //結果を回収

};

#endif //_SNACCPACK_H_
