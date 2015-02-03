#include <config.h>
extern "C" {
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif
#ifdef HAVE_LIBBZ2
#include <bzlib.h>
#endif
#include <string.h>
#include <ipfixcol/verbose.h>
#include <errno.h>
#include <libxml/parser.h>
}

#include "ipfixcol_fastbit.h"
#include "compression.h"

column_writer *column_writer::create(const char *name, xmlDoc *doc, xmlNode *node)
{
	column_writer *result;

	if (!strcmp(name, "none")) {
		result = new plain_writer();
#ifdef HAVE_LIBZ
	} else if (!strcmp(name, "gzip")) {
		result = new gzip_writer;
#endif
#ifdef HAVE_LIBBZ2
	} else if (!strcmp(name, "bzip2")) {
		result = new bzip_writer;
#endif
	} else {
		result = NULL;
	}

	if (result) {
		result->conf_init(doc, node);
	}
	return result;
}

bool plain_writer::write(const char *column_file, size_t size, const void *data)
{
	int column_fd;
	size_t written = 0;
	ssize_t n;

	column_fd = ::open(column_file, O_WRONLY | O_APPEND | O_CREAT, 0664);

	if (column_fd == -1) {
		MSG_ERROR(MSG_MODULE, "couldn't open file '%s': %s", column_file, strerror(errno));
		return false;
	}

	while (written < size) {
		//n = write(column_fd, ((char *) data) + written, (size-written < BUFSIZ) ? (size-written) : BUFSIZ);
		n = ::write(column_fd, ((char *) data) + written, size-written);
		written += n;
	}
	::close(column_fd);

	return true;
}

#ifdef HAVE_LIBZ

void gzip_writer::conf_init(xmlDoc *doc, xmlNode *node)
{
	xmlNode *cur;
	xmlChar *text;
	unsigned int ulevel;

	if (!doc || !node) {
		return;
	}
	
	cur = node->xmlChildrenNode;
	while (cur != NULL) {
		if (!xmlStrcmp(cur->name, (const xmlChar *) "level")) {
			if (!xml_get_uint(doc, cur, &ulevel) || ulevel > 9 || ulevel < 1) {
				MSG_WARNING(MSG_MODULE, "invalid gzip compression level, using default value");
				level = Z_DEFAULT_COMPRESSION;
			} else {
				level = ulevel;
			}
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "strategy")) {
			text = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (text == NULL) {
				continue;
			}
			if (!xmlStrcmp(text, (const xmlChar *) "default")) {
				strategy = Z_DEFAULT_STRATEGY;
			} else if (!xmlStrcmp(text, (const xmlChar *) "filtered")) {
				strategy = Z_FILTERED;
			} else if (!xmlStrcmp(text, (const xmlChar *) "huffman")) {
				strategy = Z_HUFFMAN_ONLY;
			} else if (!xmlStrcmp(text, (const xmlChar *) "rle")) {
				strategy = Z_RLE;
			} else if (!xmlStrcmp(text, (const xmlChar *) "fixed")) {
				strategy = Z_FIXED;
			} else {
				MSG_WARNING(MSG_MODULE, "unknown gzip strategy '%s'", text);
				strategy = Z_DEFAULT_STRATEGY;
			}
		} else {
			MSG_ERROR(MSG_MODULE, "invalid gzip option '%s'", cur->name);
		}
		cur = cur->next;
	}
}

bool gzip_writer::write(const char *column_file, size_t size, const void *data)
{
	gzFile file;
	size_t written;
	int n;

	MSG_DEBUG(MSG_MODULE, "writing file using gzip: %s", column_file);

	file = gzopen(column_file, "ab");
	if (file == NULL) {
		MSG_ERROR(MSG_MODULE, "failed to open file: %s", column_file);
		return false;
	}

	MSG_DEBUG(MSG_MODULE, "gzip file opened: %s", column_file);

	gzsetparams(file, level, strategy);

	written = 0;
	while (written < size) {
		n = gzwrite(file, ((const char *) data) + written, size - written);
		if (n == 0) {
			gzclose_w(file);
			return false;
		}
		written += n;
	}

	gzclose_w(file);
	return true;
}
#endif

#ifdef HAVE_LIBBZ2

#define BZIP2_BUFFER INT_MAX /* uses int size */

void bzip_writer::conf_init(xmlDoc *doc, xmlNode *node)
{
	xmlNode *cur;
	unsigned int uvalue;

	if (!doc || !node) {
		return;
	}
	
	cur = node->xmlChildrenNode;
	while (cur != NULL) {
		if (!xmlStrcmp(cur->name, (const xmlChar *) "blockSize")) {
			if (!xml_get_uint(doc, cur, &uvalue) || uvalue > 9 || uvalue < 1) {
				MSG_WARNING(MSG_MODULE, "invalid bzip2 block size, using default value");
			} else {
				block_size = uvalue;
			}
		} else if (!xmlStrcmp(cur->name, (const xmlChar *) "workFactor")) {
			if (!xml_get_uint(doc, cur, &uvalue) || uvalue > 9 || uvalue < 1) {
				MSG_WARNING(MSG_MODULE, "invalid bzip2 work factor, using default value");
			} else {
				work_factor = uvalue;
			}
			MSG_ERROR(MSG_MODULE, "invalid bzip2 option '%s'", cur->name);
		}
		cur = cur->next;
	}
}

bool bzip_writer::write(const char *filename, size_t size, const void *data)
{
	FILE *file;
	BZFILE *bzfile;
	int bzerror;
	unsigned int n_in, n_out;
	bool retval;
	size_t written;

	file = fopen(filename, "ab");
	if (file == NULL) {
		MSG_ERROR(MSG_MODULE, "failed to open file '%s': %s", filename, strerror(errno));
		return false;
	}

	bzfile = BZ2_bzWriteOpen(&bzerror, file, block_size, 0, work_factor);
	if (bzerror != BZ_OK) {
		MSG_ERROR(MSG_MODULE, "failed to open bz2 stream");
		fclose(file);
		return false;
	}

	written = 0;
	retval = true;
	while (written < size) {
		BZ2_bzWrite(&bzerror, bzfile, ((char *) data) + written, (size-written < BZIP2_BUFFER) ? (size-written) : BZIP2_BUFFER);
		if (bzerror != BZ_OK) {
			MSG_ERROR(MSG_MODULE, "failed to write bz2 stream: %d", bzerror);
			retval = false;
			break;
		}
		written += BZIP2_BUFFER;
	}

	BZ2_bzWriteClose(&bzerror, bzfile, 0, &n_in, &n_out);
	fclose(file);

	return retval;
}
#endif

