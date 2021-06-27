/* Definitions to support the memory module wrapper class, modified to be able to initilize memory with a binary file.
    Original work Copyright 2001, 2003 Brian R. Gaeke.
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

#ifndef _MEMORYMODULE_H_
#define _MEMORYMODULE_H_

#include "range.h"
#include "memorymodule.h"
#include "fileutils.h"
#include "mmapglue.h"
#include <cstring>

class MemoryModule : public Range {
private:
    int latency;
public:
    uint32 *myaddr;
    MemoryModule(size_t size, int latency_, FILE *init_data = NULL) 
    : Range (0, size, 0, MEM_READ_WRITE), latency(latency_) {
        myaddr = new uint32[size / 4]();
        if (init_data != NULL) {
            if (get_file_size(init_data) > size) {
                static char msg[] = "Initial memory data size exceeds the memory size";
                throw msg;
            } else {
            	std::memcpy((void*)myaddr,
        				mmap(0, extent, PROT_READ, MAP_PRIVATE, fileno (init_data), ftell (init_data)),
        				get_file_size (init_data));
            }
        }
        address = static_cast<void *> (myaddr);
    }
    ~MemoryModule() {
        delete [] myaddr;
    }
    virtual int extra_latency() { return latency; };
};

#endif /* _MEMORYMODULE_H_ */
