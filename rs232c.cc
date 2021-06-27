/*  An emurator of serial port RS-232C
    Copyright (c) 2021 Amano laboratory, Keio University.
        Author: Takuya Kojima, Tetsui Okubo

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

#include "rs232c.h"
#include "vmips.h"
#include "mapper.h"

Rs232c::Rs232c(): DeviceMap(0x10000) {}

uint32 Rs232c::fetch_word(uint32 offset, int mode, DeviceExc *client) {
  // Read from register[0] tells the device is ready to write.
  return 2;
}

void Rs232c::store_word(uint32 offset, uint32 data, DeviceExc *client) {
  
  // Write to register[2] is character output.
  if (offset == 0 && (data & 0x00FF0000) > 0) {
      putc((data & 0x00FF0000) >> 16, stderr);
      fflush(stderr);
  }

  // // HACK
  // if (((data & 0x00FF0000) >> 16) == '.') {
  //     printf("\n[HACK] exit emulator because '.' is printed from RS232C\n");
  //     printf("Number of instructions run on Geyser: %d\n", machine->num_instrs);
  //     printf("Number of cycles consumed on Neurochips: %d\n", machine->num_neuro_inst\
  // 			 rs);
  //     printf("Number of words transferred to Neurochips: %d\n", machine->num_transfer\
  // 			 red_words);
  //     printf("neurochip[0].wbuf[0] content: \n");
  //     HexDumpMemory(g_neuro0->wbuf_[0], 512);
  //     exit(EXIT_SUCCESS);
  //   }
  
}


