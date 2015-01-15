/** @file
 */
#ifndef COMPRESSION_H
#define COMPRESSION_H

extern "C" {
#include <libxml/parser.h>
}

class column_writer {
public:
	const char *name;

	column_writer() : name("none") {};
	column_writer(const char *name) : name(name) {};
	virtual ~column_writer() {};
	virtual bool write(const char *filename, size_t size, const void *data) = 0;
	virtual void conf_init(xmlDoc *doc, xmlNode *node) {} ;

	static column_writer *create(const char *name, xmlDoc *doc, xmlNode *node);
};

class plain_writer : public column_writer {
public:
	plain_writer() : column_writer("none") {};
	bool write(const char *filename, size_t size, const void *data);
};


#ifdef HAVE_LIBZ
class gzip_writer : public column_writer {
public:
	gzip_writer() : column_writer("gzip"), level(Z_DEFAULT_COMPRESSION), strategy(Z_DEFAULT_STRATEGY) {};
	bool write(const char *filename, size_t size, const void *data);
	void conf_init(xmlDoc *doc, xmlNode *node);
private:
	int level;
	int strategy;
};
#endif

#ifdef HAVE_LIBBZ2
class bzip_writer : public column_writer {
public:
	bzip_writer() : column_writer("bzip2"), block_size(6), work_factor(0) {};
	bool write(const char *filename, size_t size, const void *data);
	void conf_init(xmlDoc *doc, xmlNode *node);
private:
	int block_size;
	int work_factor;
};
#endif

#endif
