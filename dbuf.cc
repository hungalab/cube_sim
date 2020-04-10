#include "dbuf.h"
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
	if (front_connected) {
		return ((uint32 *)back)[offset / 4];
	} else {
		return ((uint32 *)front)[offset / 4];
	}

}
void DoubleBuffer::store_word_from_inner(uint32 offset, uint32 data)
{
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