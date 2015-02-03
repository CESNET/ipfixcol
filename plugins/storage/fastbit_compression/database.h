/** @file
 */
#ifndef DATABASE_H
#define DATABASE_H

extern "C" {
#include <string.h>
}

#include <string>
#include <map>

#include <fastbit/ibis.h>
#include <fastbit/table.h>
#include <fastbit/tafel.h>

#include "types.h"

#include "util.h"
#include "compression.h"
#include "configuration.h"


#define PART_FILE_NAME "-part.txt"

/* column name in the form "e<enterprise_id>id<element_id>" */
#define COLUMN_NAME_LEN (1 + 9 /* uint32_t digits */ + 2 + 5 /* uint16_t digits */ + 2 + 1)

struct fb_column {
	ibis::TYPE_T type; /** Fastbit data type of the column */
	size_t size; /** size of the data type, zero for variable size */
	uint64_t row; /** Row counter */
	column_writer *writer; /** Writer used for this column */
	char name[COLUMN_NAME_LEN]; /** Column name */
	growing_buffer data; /** Buffer for column data */
	growing_buffer spfile; /** Buffer for .sp file if needed */
	uint64_t length_prev; /** Total number of bytes written to column file
				using this structure */
	bool build_index; /** Whether to build index for this column */
};

struct information_element {
	uint32_t enterprise; /** Enterprise number */
	uint16_t id; /** Element id */
	ipfix_type_t type; /** Data type of the element */
	uint16_t length; /** Length of the element, zero for variable length */
	struct fb_column *column; /** Link to corresponding fastbit column.
				    When multiple columns are used for one
				    element, this points to the first element
				    of an array. */
};

/**
 * Structure that holds the information contained in the header section of the
 * -part.txt file
 */
struct fb_table_header {
	char *name; /** Name */
	char *description; /** Description */
	uint64_t nrows; /** Number_of_rows */
	uint64_t ncolumns; /** Number_of_columns */
	time_t timestamp; /** Timestamp */
};

/**
 * Parse -part.txt file.
 * @param file Stream opened for reading.
 * @param header Pointer to a header structure, where the header will be stored.
 * @param columns List of column structures. For every column in the -part.txt
 * file, a new item will be added to this list.
 * @return true on success.
 */
bool parse_part_file(FILE *file, struct fb_table_header *header, std::vector<struct fb_column> &columns);

/**
 * @brief Store numeric data into column buffer. Input data are expected in network byte order
 * and converted to host byte order.
 */
void store_numeric(struct fb_column *column, const void *data, size_t size);

/**
 * @brief Store BLOB data into column.
 */
void store_blob(struct fb_column *column, const void *data, size_t size);

/**
 * @brief An interface for storing data defined by an IPFIX template in a fastbit database table.
 */
class fb_table {
public:
	fb_table();

	/**
	 * @brier Initialize fastbit table for storing data according to given IPFIX template
	 */
	void set_template(const struct ipfix_template *tmpl, struct fastbit_plugin_conf *conf);

	/**
	 * @brief Set directory where the table will be stored. When a new
	 * directory is set, the old one is flushed.
	 * @param base_dir Path to a directory that will be parent directory to
	 * this table, e.g. for base directory /base/dir template will be stored
	 * in /base/dir/<template-number>
	 */
	void set_dir(const char *base_dir);

	/**
	 * @brief Increment row counter
	 */
	void next_row();

	/**
	 */
	uint64_t get_row();

	/**
	 * @brief Append data to column.
	 * @param field Information element number in template
	 * @param data Pointer to data.
	 * @param lengt Number of bytes to be stored pointed by @a data
	 */
	void store(uint16_t field, const void *data, size_t length);

	/**
	 * @brief Get nuber of elements in this template.
	 */
	size_t get_element_count();

	/**
	 * @brief Write data to disk.
	 */
	void flush();

	void build_indexes();
	
	~fb_table();
private:
	/**
	 * @brief Return path to a file in table directory.
	 * @param name Name of the requested file.
	 * @returns Full path to a file in table directory; should be
	 * deallocated with delete[].
	 */
	char *get_file_path(const char *name);

	/**
	 * @brief Return path to a file in table directory. Name of the file is
	 * specified as two parts since this is useful for accessing different
	 * files related to one column.
	 * @param name First part of the file name.
	 * @param suffix Second part of the file name.
	 * @returns Full path to a file in table directory; should be
	 * deallocated with delete[].
	 */
	char *get_file_path(const char *name, const char *suffix);

	char *dir;
	uint16_t template_id;
	uint64_t row;
	size_t max_rows;
	struct fb_column *columns;
	size_t ncolumns;
	size_t nelements;
	struct information_element *elements;
};

/**
 * @brief Class representing a directory containing number of FastBit tables.
 */
class dbslot {
public:
	dbslot();
	dbslot(const dbslot &other);
	dbslot(int timeslot, const char *dir);
	~dbslot();

	/**
	 * @brief Change database directory
	 * @param dir path to directory
	 * @return true if directory was actually changed
	 */
	bool change_dir(const char *dir);
	void set_timeslot(int timeslot);

	/**
	 * @brief Store data set.
	 * @param tmpl IPFIX template
	 * @param data_set Pointer to data set
	 * @return Number of records written.
	 */
	uint32_t store_set(const struct ipfix_template *tmpl, const struct ipfix_data_set *data_set, struct fastbit_plugin_conf *conf);

	/**
	 * @brief Write all tables to disk.
	 */
	void flush();

	/**
	 * @brief Get number of this slot
	 */
	int get_timeslot();

	/**
	 * @brief Write statistics file for database. These statistics consist
	 * of:
	 *  * Received flows: total number of received flows for this
	 *  observation domain
	 *  * Stored flows: total number of flows stored in this database
	 *  * Lost flows: difference of the upper two
	 * @param received_flows Total number of received records for this
	 * observation domain.
	 */
	void write_stats();

	uint64_t exported_flows; /** number of exported flows since last directory change */
	uint32_t seq_last; /** sequence number of last exported packet */
private:
	int timeslot;
	char *dir;
	uint64_t stored_flows; /** number of flows stored in current directory */

	std::map<uint16_t, fb_table > tables;
	std::map<uint16_t, uint64_t> rows;

};

#endif
