/*  Double buffer for accelerator's memory module
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

#include "dbuf.h"
#include "vmips.h"
#include "options.h"
#include "mmapglue.h"
#include <cstring>

DoubleBuffer::DoubleBuffer(size_t size, uint32 mask_, FILE *init_data)
	: Range (0, size, 0, MEM_READ_WRITE), mask(mask_) {
	front = new uint32[size / 4]();
	back = new uint32[size / 4]();
	if (init_data != NULL) {
		std::memcpy((void*)front,
					mmap(0, extent, PROT_READ, MAP_PRIVATE, fileno (init_data), ftell (init_data)),
					get_file_size (init_data));
		std::memcpy((void*)front,
					mmap(0, extent, PROT_READ, MAP_PRIVATE, fileno (init_data), ftell (init_data)),
					get_file_size (init_data));
	}
	address = static_cast<void *> (front);
	front_connected = true;
}

DoubleBuffer::~DoubleBuffer() {
	delete [] front;
	delete [] back;
}

void DoubleBuffer::store_word(uint32 offset, uint32 data, DeviceExc *client)
{
	uint32 *werd;
	/* calculate address */
	werd = ((uint32 *) address) + (offset / 4);
	/* store word */
	*werd = data & mask;
}

void DoubleBuffer::buf_switch() {
	if (front_connected) {
		address = static_cast<void *> (back);
		front_connected = false;
	} else {
		address = static_cast<void *> (front);
		front_connected = true;
	}
}

uint32 DoubleBuffer::fetch_word_from_inner(uint32 offset)
{
	if (offset / 4 >= extent) {
		if (machine->opt->option("dbemsg")->flag) {
			fprintf(stderr, "Internal access exceeds the mapped range at: 0x%X\n", offset);
		}
		return 0;
	}
	if (front_connected) {
		return ((uint32 *)back)[offset / 4];
	} else {
		return ((uint32 *)front)[offset / 4];
	}

}
void DoubleBuffer::store_word_from_inner(uint32 offset, uint32 data)
{
	if (offset / 4 >= extent) {
		if (machine->opt->option("dbemsg")->flag) {
			fprintf(stderr, "Internal access exceeds the mapped range at: 0x%X\n", offset);
		}
		return ;
	}
	uint32 *werd;
	/* calculate address */
	if (front_connected) {
		werd = ((uint32 *) back) + (offset / 4);
	} else {
		werd = ((uint32 *) front) + (offset / 4);
	}

	/* store word */
	*werd = data & mask;

}

uint16 DoubleBuffer::fetch_half_from_inner(uint32 offset)
{
	if (offset / 4 >= extent) {
		if (machine->opt->option("dbemsg")->flag) {
			fprintf(stderr, "Internal access exceeds the mapped range at: 0x%X\n", offset);
		}
		return 0;
	}
	if (front_connected) {
		return ((uint16 *)back)[offset / 2];
	} else {
		return ((uint16 *)front)[offset / 2];
	}

}
void DoubleBuffer::store_half_from_inner(uint32 offset, uint16 data)
{
	if (offset / 4 >= extent) {
		if (machine->opt->option("dbemsg")->flag) {
			fprintf(stderr, "Internal access exceeds the mapped range at: 0x%X\n", offset);
		}
		return ;
	}
	uint16 *werd;
	/* calculate address */
	if (front_connected) {
		werd = ((uint16 *) back) + (offset / 2);
	} else {
		werd = ((uint16 *) front) + (offset / 2);
	}

	/* store word */
	*werd = data & mask;

}

uint8 DoubleBuffer::fetch_byte_from_inner(uint32 offset)
{
	if (offset / 4 >= extent) {
		if (machine->opt->option("dbemsg")->flag) {
			fprintf(stderr, "Internal access exceeds the mapped range at: 0x%X\n", offset);
		}
		return 0;
	}
	if (front_connected) {
		return ((uint8 *)back)[offset];
	} else {
		return ((uint8 *)front)[offset];
	}

}
void DoubleBuffer::store_byte_from_inner(uint32 offset, uint8 data)
{
	if (offset / 4 >= extent) {
		if (machine->opt->option("dbemsg")->flag) {
			fprintf(stderr, "Internal access exceeds the mapped range at: 0x%X\n", offset);
		}
		return ;
	}
	uint8 *werd;
	/* calculate address */
	if (front_connected) {
		werd = ((uint8 *) back) + offset;
	} else {
		werd = ((uint8 *) front) + offset;
	}

	/* store word */
	*werd = data & mask;

}