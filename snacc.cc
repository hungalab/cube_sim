#include "snacc.h"
using namespace SNACCComponents;

SNACC::SNACC(uint32 node_ID, Router* upperRouter) : CubeAccelerator(node_ID,upperRouter)
{
  node=node_ID;
}
SNACC::~SNACC(){

}

void SNACC::setup() override{

  ###SNACC全体のモジュール生成(ここでは複数コアを包括するSNACCPACKモジュールの生成だけ)
  CorePack = new SNACCPACK();//Constracter

  ###//その他処理

  SNACCPACK::Packsetup(); //処理後SNACCPACKクラスのsetup()関数を
}

void SNACC::Reset(){

  SNACCPACK::PackReset();

}
