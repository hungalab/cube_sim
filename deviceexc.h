/* Definitions to support devices that can handle exceptions modified to consider pipelining.
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

#ifndef _DEVICEEXC_H_
#define _DEVICEEXC_H_

#include "accesstypes.h"
#include "types.h"
#include "state.h"

/* An abstract class which describes a device that can handle exceptions. */

class DeviceExc {

public:
	/* This message notifies the device that an exception of type EXCCODE
	   has been generated. The memory access (if any) is of type MODE,
	   and the coprocessor that generated it (if any) is COPROCNO. */
    virtual void exception(uint16 excCode, int mode = ANY,
		int coprocno = -1) = 0;
    virtual ~DeviceExc() { }

	/* A flag which says whether an exception is ready to be handled. */
	bool exception_pending;

	// Control-flow methods.
	virtual void step () = 0;
	virtual void reset () = 0;

};

#endif /* _DEVICEEXC_H_ */
