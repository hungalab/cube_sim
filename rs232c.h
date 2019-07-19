#ifndef _RS232C_H_
#define _RS232C_H_


#include "deviceint.h"
#include "devicemap.h"
#include <new>

class Rs232c : public DeviceMap {
 public: 
  Rs232c();
  
 public:
  virtual uint32 fetch_word(uint32 offset, int mode, DeviceExc *client);
  virtual void store_word(uint32 offset, uint32 data, DeviceExc *client);

};

#endif //_RS232C_H_
