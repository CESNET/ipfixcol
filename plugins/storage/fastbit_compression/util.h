/** @file
 */
#ifndef UTIL_H
#define UTIL_H

extern "C" {
#include <stdint.h>
}

#include <new>

#ifndef ntohll
#define ntohll_macro(x) ( (((uint64_t) ((unsigned char *) (&x))[7]) << 0*8) | \
                          (((uint64_t) ((unsigned char *) (&x))[6]) << 1*8) | \
                          (((uint64_t) ((unsigned char *) (&x))[5]) << 2*8) | \
                          (((uint64_t) ((unsigned char *) (&x))[4]) << 3*8) | \
                          (((uint64_t) ((unsigned char *) (&x))[3]) << 4*8) | \
                          (((uint64_t) ((unsigned char *) (&x))[2]) << 5*8) | \
                          (((uint64_t) ((unsigned char *) (&x))[1]) << 6*8) | \
                          (((uint64_t) ((unsigned char *) (&x))[0]) << 7*8) )
#endif

/**
 * @brief Convert a 64bit number from network byte order to host byte order.
 */
inline uint64_t ntohll(uint64_t arg) {
	return ntohll_macro(arg);
}

/**
 * General purpose variable size array of bytes.
 */
class growing_buffer {
public:
	/**
	 * @brief Construct empty buffer. Memory is allocated after first append.
	 */
	growing_buffer();
	/**
	 * @brief copy constructor
	 */
	growing_buffer(const growing_buffer& other) throw(std::bad_alloc);
	~growing_buffer();

	/**
	 * @brief Append data to buffer.
	 * @param size Number of bytes to add.
	 * @param data Array of @a size bytes to be append to buffer
	 * @return Pointer to the first byte of newly appended data.
	 */
	char *append(size_t size, const void *data) throw(std::bad_alloc);

	/**
	 * @brief Append newly allocated bytes to the buffer. Value of these bytes is undefined.
	 * @param size Number of bytes to add.
	 * @return Pointer to the first byte of newly appended data.
	 */
	char *append_blank(size_t size) throw(std::bad_alloc);

	/**
	 * @brief Change buffer size to zero.
	 */
	void empty();

	/**
	 * @brief Allocate buffer at once.
	 * @param new_size New size of the buffer. Must be greater than the old size.
	 */
	void allocate(size_t new_size) throw(std::bad_alloc);

	/**
	 * @brief Get pointer to buffer data.
	 * @param index Position in the buffer. Must be less than buffer size.
	 * @return Pointer to the requested byte.
	 */
	char *access(size_t index);

	/**
	 * @brief Get current size of the buffer.
	 * @return Current size of the buffer.
	 */
	size_t get_size();
private:
	static const size_t default_size = 128;
	size_t allocated;
	size_t size;
	char *data;
};

/**
 * @brief Create directory with all its parents if needed.
 * @param path Path to the directory to be created.
 * @param mode Access mode for the directory.
 * @return true if the directory was created.
 */
bool mkdir_parents(const char *path, mode_t mode);

#endif
