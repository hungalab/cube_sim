/*  Macros for pipeline status of CPU
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


#ifndef _STATE_H_
#define _STATE_H_

#include "types.h"

#define UNCACHED_ACCESS_STALL	0
#define CACHED_ACCESS_STALL		1
#define INTERLOCKED 			2

// struct DeviceState
// {
// 	bool stall;
// 	int  stall_cause;
// };

#endif /*_STATE_H_*/