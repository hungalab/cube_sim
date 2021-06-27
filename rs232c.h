/*  Headers for the RS-232c serial port class
    Copyright (c) 2021 Amano laboratory, Keio University.
        Author: Takuya Kojima

    This file is part of CubeSim, a cycle accurate simulator for 3-D stacked system.

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
