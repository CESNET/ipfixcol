extern "C" {
#include <ipfixcol/verbose.h>
}

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>

#include "util.h"

growing_buffer::growing_buffer() : allocated(0), size(0), data(NULL)
{
}

growing_buffer::growing_buffer(const growing_buffer& other) throw(std::bad_alloc) : allocated(other.size), size(other.size), data(NULL)
{
	if (size > 0) {
		data = (char *) malloc(allocated);
		if (data == NULL) {
			throw (std::bad_alloc());
		}
		memcpy(data, other.data, size);
	}
}

growing_buffer::~growing_buffer()
{
	if (data) {
		free(data);
	}
}

char *growing_buffer::append(size_t size, const void *data) throw(std::bad_alloc)
{
	char *cur;

	cur = this->append_blank(size);
	memcpy(cur, data, size);

	return cur;
}

char *growing_buffer::append_blank(size_t size) throw(std::bad_alloc)
{
	size_t cur;

	cur = this->size;

	if (size == 0) {
		return &this->data[cur];
	}

	while (this->size + size > allocated) {
		if (allocated == 0) {
			allocated = (size > default_size) ? size : default_size;
			this->data = NULL;
		} else {
			/* TODO: better allocation strategy */
			if (allocated * 2 > this->size + size) {
				allocated *= 2;
			} else {
				allocated += size;
			}
		}
		this->data = (char *) realloc(this->data, this->allocated);
		if (this->data == NULL) {
			throw std::bad_alloc();
		}

	}
	this->size += size;

	return &this->data[cur];
}

void growing_buffer::empty()
{
	size = 0;
}

void growing_buffer::allocate(size_t new_size) throw(std::bad_alloc) {
	assert(new_size >= this->size);

	if (allocated >= new_size) {
		return;
	}

	allocated = new_size;
	this->data = (char *) realloc(this->data, this->allocated);
	if (this->data == NULL) {
		throw std::bad_alloc();
	}
}

char *growing_buffer::access(size_t offset)
{
	return &data[offset];
}

size_t growing_buffer::get_size()
{
	return size;
}

bool mkdir_parents(const char *pathname, mode_t mode)
{
	char *path;
	bool result = true;
	size_t end, len;

	path = strdup(pathname);
	if (!path) {
		return false;
	}

	len = strlen(pathname);
	end = len;

	while (1) {
		if (mkdir(path, mode) == 0 || errno == EEXIST) {
			// go to subdirectory
			result = true;
			if (end == len) {
				break;
			}
			path[end] = '/';
			end = strlen(path);
		} else if (errno == ENOENT) {
			// go to parent directory
			while (path[end] != '/' && end > 0) end--;
			if (end == 0) {
				result = false;
			}
			path[end] = 0;
		} else {
			result = false;
		}
	}

	free(path);
	return result;
}

