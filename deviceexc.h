/* Definitions to support devices that can handle exceptions.
   Copyright 2001, 2003 Brian R. Gaeke.

This file is part of VMIPS.

VMIPS is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

VMIPS is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with VMIPS; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

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
	bool catch_exception;

	// Control-flow methods.
	virtual void step () = 0;
	virtual void reset () = 0;

	/*notification of stall*/
	virtual void stall(int cause) {
		stall_stat = true;
		stall_cause = cause;
	};

private:
	/* Device status*/
	// DeviceState *state;
	bool stall_stat;
	int stall_cause;



};

#endif /* _DEVICEEXC_H_ */
