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
	void buf_switch();
};


#endif /* _DOUBLEBUFFER_H_ */