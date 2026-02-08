#include <z_ringbuffer.h>
struct ring_buffer;

/**
 * A circular buffer that uses atomic variables internally
 * to provide thread-safety.
 */
class CircularBuffer {
public:
	ring_buffer* rb;
	CircularBuffer(unsigned int size, bool preserveBoundaries = false);
	~CircularBuffer();
	/**
	 * Write to the circular buffer. Call this from a single thread.
	 * @return `size` on success or -1 on error
	 */
	int write(void const* data, size_t size);
	/**
	 * Get how many bytes can be written.
	 *
	 * @return the number of bytes that can be written
	 */
	int availableToWrite();
	/**
	 * Read from the circular buffer. Call this from a single thread.
	 *
	 * @param data the buffer where read data is written
	 * @size how many bytes to read. If `preserveBoundaries` was set,
	 * it is the maximum message length to be retrieved, otherwise it is the
	 * exact length that will be retrieved.
	 *
	 * @return the number of retrieved bytes on success or -1 on error
	 */
	int read(void* data, size_t size);
	/**
	 * Get how many bytes can be read from the next message stored in the buffer.
	 *
	 * @return the number of bytes that can be read
	 */
	int availableToRead();
private:
	bool preserveBoundaries;
};
