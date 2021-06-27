/*  Headers for the double buffer class
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

#ifndef _DOUBLEBUFFER_H_
#define _DOUBLEBUFFER_H_

#include "range.h"
#include "types.h"
#include "fileutils.h"


class DoubleBuffer : public Range {
private:
	uint32 mask;
	uint32 *front;
	uint32 *back;
	bool front_connected;
public:
	DoubleBuffer(size_t size, uint32 mask = 0xFFFFFFFF, FILE *init_data  = NULL);
	~DoubleBuffer();

	void store_word(uint32 offset, uint32 data, DeviceExc *client);
	uint32 fetch_word_from_inner(uint32 offset);
	void store_word_from_inner(uint32 offset, uint32 data);
	uint16 fetch_half_from_inner(uint32 offset);
	void store_half_from_inner(uint32 offset, uint16 data);
	uint8 fetch_byte_from_inner(uint32 offset);
	void store_byte_from_inner(uint32 offset, uint8 data);
	void buf_switch();
};


#endif /* _DOUBLEBUFFER_H_ */