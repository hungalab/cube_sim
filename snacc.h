#ifndef _SNACC_H_
#define _SNACC_H_

#define CORE_NUM 4

#include "accelerator.h"
#include "snacccore.h"

class CubeAccelerator;


namespace SNACCComponents {
  class SNACCMemoryModule : public MemoryModule {
    private:
      int mask;

    public:
      SNACCMemoryModule(size_t size, int mask_) : MemoryModule(size),mask(mask_) {};
      void store_word(uint32 offset, uint32 data, DeviceExc *client);

  };

  class ConfigController{

  };

  class SNACCPACK : public SNACC{
  private:
    SNACCComponents::SNACCMemoryModule *wbuf;
    //SNACCCore *core0,*core1,*core2,*core3;
    //void step();
  public:
    SNACCPACK(){};
    ~SNACCPACK();
    void Packsetup();
    void TaskDistribute(); //ただデータを各コアに分配
    void GetResult(); //結果を回収
  };

}

class SNACC : public CubeAccelerator{
  private:
    SNACCComponents::SNACCPACK *CorePack;
    int node;
  public:
    SNACC::SNACC(uint32 node_ID, Router* upperRouter):CubeAccelerator(node_ID,upperRouter);
    ~SNACC();
    void setup();
    const char *accelerator_name() { return "SNACC"; }
    void Reset();
}


#endif //_SNACC_H
