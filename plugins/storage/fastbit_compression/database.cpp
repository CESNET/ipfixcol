extern "C" {
#include <ipfixcol/storage.h>
#include <ipfixcol/verbose.h>
}
	
#include <libxml/parser.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include <string>
#include <map>

#include <fastbit/ibis.h>
#include <fastbit/index.h>
#include <fastbit/capi.h>

#include "ipfixcol_fastbit.h"
#include "database.h"
#include "util.h"
#include "compression.h"
#include "configuration.h"

uint32_t dbslot::store_set(const struct ipfix_template *tmpl, const struct ipfix_data_set *data_set, struct fastbit_plugin_conf *conf)
{
	uint16_t id;
	uint16_t length;
	size_t pos = 0;
	size_t min_record_length = 0;
	bool first = true;
	int record_count = 0;
	std::map<uint16_t, fb_table >::iterator table_it;

	table_it = tables.find(tmpl->template_id);
	if (table_it == tables.end()) {
		MSG_DEBUG(MSG_MODULE, "initializing template %d, field count %d", tmpl->template_id, tmpl->field_count);
		table_it = tables.insert(std::make_pair(tmpl->template_id, fb_table())).first;
		table_it->second.set_template(tmpl, conf);
		table_it->second.set_dir(dir);
	}
	fb_table &table = table_it->second;

	while (pos < ntohs(data_set->header.length)) {
		/* in the first iteration, find out the minimal size of the data
		 * record which is the maximum possible length of padding at the
		 * end of a data set (RFC 5101, section 3.3.1) */
		
		size_t ent_fields = 0;
		for (size_t i = 0; i < tmpl->field_count; i++) {
			id = tmpl->fields[i+ent_fields].ie.id;
			length = tmpl->fields[i+ent_fields].ie.length;
			if (id & 0x8000) {
				// enterprise field
				id &= ~0x8000;
				ent_fields++;
			}

			/* variable length element */
			if (length == VAR_IE_LENGTH) {
				length = data_set->records[pos];
				MSG_DEBUG(MSG_MODULE, "variable length element: length %d", length);
				pos += 1;
				if (first) {
					min_record_length += 1;
				}
				if (length == 255) {
					((char *) &length)[0] = data_set->records[pos];
					((char *) &length)[1] = data_set->records[pos+1];
					length = ntohs(length);
					MSG_DEBUG(MSG_MODULE, "two-byte length: %d", length);
					pos += 2;
				}
			} else {
				if (first) {
					min_record_length += length;
				}
			}

			if (table.get_row() >= conf->buffer_size) {
				MSG_DEBUG(MSG_MODULE, "buffer full, flushing template %d", tmpl->template_id);
				table.flush();
			}

			if (tmpl->field_count != table.get_element_count()) {
				MSG_ERROR(MSG_MODULE, "bad template: %d != %d", tmpl->field_count, table.get_element_count());
			}

			table.store(i, &data_set->records[pos], length);

			pos += length;
		}
		first = false;
		table.next_row();
		record_count++;
		/* check if there is enough space for next data record (RFC 5101, section 3.3.1) */
		if (pos > ntohs(data_set->header.length) - sizeof(struct ipfix_set_header) - min_record_length) {
			break;
		}
	}
	stored_flows += record_count;

	return record_count;
}

dbslot::dbslot() : exported_flows(0), seq_last(0), timeslot(0), dir(NULL), stored_flows(0), tables(), rows()
{
}

dbslot::dbslot(int timeslot, const char *dir) : exported_flows(0), seq_last(0), timeslot(timeslot), stored_flows(0), tables(), rows()
{
	this->dir = strdup(dir);
}
dbslot::dbslot(const dbslot &other) : exported_flows(other.exported_flows), seq_last(other.seq_last), timeslot(other.timeslot), stored_flows(other.stored_flows), tables(other.tables), rows(other.rows)
{
	this->dir = strdup(other.dir);
}

bool dbslot::change_dir(const char *dir)
{
	std::map<uint16_t, fb_table>::iterator table;

	if (dir == NULL) {
		return false;
	}

	flush();
	write_stats();
	
	if (!strcmp(dir, this->dir)) {
		return false;
	}

	exported_flows = 0;
	stored_flows = 0;

	if (this->dir) {
		free(this->dir);
	}
	this->dir = strdup(dir);

	for (table = tables.begin(); table != tables.end(); table++) {
		table->second.set_dir(dir);
	}

	return true;
}

#define PART_NAME_MAX 6

void dbslot::flush()
{
	std::map<uint16_t, fb_table>::iterator iter;

	for (iter = tables.begin(); iter != tables.end(); iter++) {
		iter->second.flush();
		iter->second.build_indexes();
	}
}

dbslot::~dbslot()
{
	if (this->dir) {
		free(this->dir);
	}
}

int dbslot::get_timeslot()
{
	return timeslot;
}

void dbslot::set_timeslot(int timeslot)
{
	this->timeslot = timeslot;
}

void dbslot::write_stats()
{
	char *filename;
	FILE *file;

	filename = new char[strlen(dir) + strlen(STATS_FILE_NAME) + 2];

	strcpy(filename, dir);
	strcat(filename, "/" STATS_FILE_NAME);

	file = fopen(filename, "w");
	if (!file) {
		MSG_ERROR(MSG_MODULE, "couldn't open file '%s': %s", filename, strerror(errno));
	} else {
		fprintf(file, "Exported flows: %lu\nReceived flows: %lu\nLost flows: %lu\n", exported_flows, stored_flows, exported_flows - stored_flows);
		fclose(file);
	}
	delete[] filename;
}

fb_table::fb_table() : dir(NULL), template_id(0), row(0), max_rows(0), columns(NULL), ncolumns(0), nelements(0), elements(NULL)
{
}

void fb_table::set_template(const struct ipfix_template *tmpl, struct fastbit_plugin_conf *conf)
{
	struct fb_column *column;
	struct information_element ie;
	size_t ent_fields = 0;
	size_t element_size;


	this->template_id = tmpl->template_id;
	if (elements) {
		delete[] elements;
	}
	elements = new struct information_element[tmpl->field_count];
	if (columns) {
		delete[] columns;
	}
	// there are at most two columns for every information element
	columns = new struct fb_column[2 * tmpl->field_count];
	column = columns;

	nelements = tmpl->field_count;

	ncolumns = 0;
	for (size_t i = 0; i < tmpl->field_count; i++) {
		ie.id = tmpl->fields[i+ent_fields].ie.id;
		ie.length = tmpl->fields[i+ent_fields].ie.length;
		if (ie.id & 0x8000) {
			// enterprise field
			ie.id &= ~0x8000;
			ent_fields++;
			ie.enterprise = tmpl->fields[i+ent_fields].enterprise_number;
		} else {
			ie.enterprise = 0;
		}

		if (ie.length == VAR_IE_LENGTH) {
			ie.length = 0;
		}

		MSG_DEBUG(MSG_MODULE, "adding element %d of %d: id %d, enterprise %d", i, tmpl->field_count, ie.id, ie.enterprise);

		ie.type = get_element_type_cached(ie.enterprise, ie.id, &conf->type_cache);

		column->row = 0;
		element_size = ie.length;
		column->type = ipfix_to_fastbit_type(ie.type, &element_size);
		column->size = element_size;
		column->length_prev = 0;
		if (conf) {
			if (element_size) {
				column->data.allocate(conf->buffer_size * element_size);
			}
			column->writer = get_column_writer(conf, tmpl->template_id, ie.enterprise, ie.id);
			column->build_index = get_build_index(conf, tmpl->template_id, ie.enterprise, ie.id);
		} else {
			column->writer = NULL;
			column->build_index = false;
		}
		if (ie.type == IPFIX_TYPE_ipv6Address) {
			/* two columns for 128bit ipv6 address */
			sprintf(column->name, "e%did%dp0", ie.enterprise, ie.id);
			column->type = ibis::ULONG;
			ie.column = column;
			column++;

			sprintf(column->name, "e%did%dp1", ie.enterprise, ie.id);
			column->type = ibis::ULONG;
			column->writer = (column - 1)->writer;
			column->row = (column - 1)->row;
			column->size = (column - 1)->size;
			column->build_index = (column - 1)->build_index;
			column->length_prev = (column -1)->length_prev;
			column++;
			ncolumns+=2;
		} else {
			sprintf(column->name, "e%did%d", ie.enterprise, ie.id);
			ie.column = column;
			column++;
			ncolumns++;
		}
		
		elements[i] = ie;
	}

}

void fb_table::set_dir(const char *base_dir)
{
	if (this->dir) {
		delete[] this->dir;
	}
	this->dir = new char[strlen(base_dir) + 1 + 5 + 1];
	sprintf(this->dir, "%s/%u", base_dir, template_id);
}

fb_table::~fb_table()
{
	if (columns) {
		delete[] columns;
	}
	if (elements) {
		delete[] elements;
	}
	if (dir) {
		delete[] dir;
	}
}

void fb_table::next_row()
{
	row++;
}

uint64_t fb_table::get_row()
{
	return row;
}

size_t fb_table::get_element_count()
{
	return nelements;
}

void store_numeric(struct fb_column *column, const void *data, size_t size)
{
	char *dest;
	uint32_t tmp;
	uint64_t tmp_long;

	dest = column->data.append_blank(column->size);

	switch (size) {
	case 1:
		*((uint8_t *) dest) = *((uint8_t *) data);
		break;
	case 2:
		*((uint16_t *) dest) = ntohs(*((const uint16_t *) data));
		break;
	case 3:
		tmp = 0;
		memcpy(((char *) &tmp) - size, data, size);
		*((uint32_t *) dest) = ntohl(tmp);
		break;
	case 4:
		*((uint32_t *) dest) = ntohl(*((const uint32_t *) data));
		break;
	case 5:
	case 6:
	case 7:
		tmp_long = 0;
		memcpy(((char *) &tmp_long) - size, data, size);
		*((uint64_t *) dest) = ntohl(tmp_long);
		break;
	case 8:
		*((uint64_t *) dest) = ntohll(*((const uint64_t *) data));
		break;
	default:
		MSG_ERROR(MSG_MODULE, "numeric element too big");
		break;
	}
	column->row++;
}

void store_blob(struct fb_column *column, const void *data, size_t size)
{
	uint64_t pos;

	pos = column->length_prev + column->data.get_size();
	column->data.append(size, data);
	column->spfile.append(sizeof(pos), &pos);
	column->row++;
}

void fb_table::store(uint16_t field, const void *data, size_t length)
{
	if (elements[field].column->row > row) {
		/* this row already written */
		MSG_WARNING(MSG_MODULE, "not writing element #%d of template %d, element already written", field, template_id);
		return;
	}

	switch (elements[field].type) {
		/* Numeric types */
	case IPFIX_TYPE_unsigned8:
	case IPFIX_TYPE_signed8:
	case IPFIX_TYPE_unsigned16:
	case IPFIX_TYPE_signed16:
	case IPFIX_TYPE_unsigned32:
	case IPFIX_TYPE_dateTimeSeconds:
	case IPFIX_TYPE_ipv4Address:
	case IPFIX_TYPE_signed32:
	case IPFIX_TYPE_float32:
	case IPFIX_TYPE_unsigned64:
	case IPFIX_TYPE_dateTimeMilliseconds:
	case IPFIX_TYPE_dateTimeMicroseconds:
	case IPFIX_TYPE_dateTimeNanoseconds:
	case IPFIX_TYPE_signed64:
	case IPFIX_TYPE_float64:
	case IPFIX_TYPE_boolean:
	case IPFIX_TYPE_macAddress:
		store_numeric(elements[field].column, data, length);
		break;

	case IPFIX_TYPE_ipv6Address:
		store_numeric(elements[field].column, &((char *) data)[0], 8);
		store_numeric(elements[field].column+1, &((char *) data)[8], 8);
		break;
	case IPFIX_TYPE_octetArray:
		store_blob(elements[field].column, data, length);
		break;
	case IPFIX_TYPE_string:
		elements[field].column->data.append(length, data);
		elements[field].column->data.append(1, "\0");
		elements[field].column->row++;
		break;
	
	case IPFIX_TYPE_basicList:
	case IPFIX_TYPE_subTemplateList:
	case IPFIX_TYPE_subTemplateMultiList:
		store_blob(elements[field].column, data, length);
		break;

	default:
		break;
	}

}

void fb_table::flush()
{
	char *part_file_path;
	FILE *part_file;
	const char *description = "Generated by ipfixcol fasbit plugin";
	column_writer *writer;
	plain_writer default_writer;

	struct fb_table_header header;
	std::vector<struct fb_column> columns_orig;

	if (!dir) {
		return;
	}

	part_file_path = get_file_path(PART_FILE_NAME);

	MSG_DEBUG(MSG_MODULE, "creating directory '%s'", dir);
	if (!mkdir_parents(dir, 0775)) {
		MSG_ERROR(MSG_MODULE, "failed creating directory %s: %s", dir, strerror(errno));
		delete[] part_file_path;
		return;
	}

	/* first read existing part file */
	part_file = fopen(part_file_path, "r");

	header.name = NULL;
	header.description = NULL;
	header.nrows = 0;
	header.ncolumns = 0;
	if (part_file) {
		parse_part_file(part_file, &header, columns_orig);
		fclose(part_file);
		if (header.description) {
			description = header.description;
		}
	} else {
		MSG_DEBUG(MSG_MODULE, "couldn't open file '%s': %s", part_file_path, strerror(errno));
	}

	/* TODO: check if the original rows match the current ones, otherwise delete the old table */
	/* TODO: proper format of part file */

	part_file = fopen(part_file_path, "w");
	if (part_file == NULL) {
		MSG_WARNING(MSG_MODULE, "couldn't open file '%s': %s", part_file_path, strerror(errno));
		delete[] part_file_path;
		return;
	}
	fprintf(part_file, "# meta data for data partition %u written by ipfixcol fastbit plugin on %s\n\n", template_id, "date");
	fprintf(part_file, "BEGIN HEADER\nName = %u\nDescription = %s\nNumber_of_rows = %lu\nNumber_of_columns = %lu\nTimestamp = %u\nEND HEADER\n", template_id, description, header.nrows + row, ncolumns, 0);

	for (size_t i = 0; i < ncolumns; i++) {
		if (columns[i].type == ibis::UNKNOWN_TYPE) {
			continue;
		}
		char *column_file = this->get_file_path(columns[i].name);

		writer = columns[i].writer;
		if (writer == NULL) {
			writer = &default_writer;
		}

		if (!writer->write(column_file, columns[i].data.get_size(), columns[i].data.access(0))) {
			MSG_ERROR(MSG_MODULE, "failed to write column %s in partition %d", columns[i].name, template_id);
		}
		delete[] column_file;

		// write .sp file for blob columns
		if (columns[i].type == ibis::BLOB) {
			char *sp_filename = this->get_file_path(columns[i].name, ".sp");
			MSG_DEBUG(MSG_MODULE, "wirting .sp file '%d'", sp_filename);
			default_writer.write(sp_filename, columns[i].spfile.get_size(), columns[i].spfile.access(0));
			delete[] sp_filename;
		}

		fprintf(part_file, "\nBegin Column\nname = %s\ndescription = compression: %s\ndata_type = %s\nEnd Column\n", columns[i].name, writer->name, fastbit_type_str(columns[i].type));
		
		
		columns[i].length_prev += columns[i].data.get_size();
		columns[i].row = 0;
		columns[i].data.empty();
		columns[i].spfile.empty();
	}
	fclose(part_file);
	delete[] part_file_path;

	if (header.name) {
		free(header.name);
	}
	if (header.description) {
		free(header.description);
	}

	row = 0;
}

void fb_table::build_indexes()
{
	for (size_t i = 0; i < ncolumns; i++) {
		if (columns[i].build_index) {
			fastbit_build_index(dir, columns[i].name, NULL);
		}
	}
}

char *fb_table::get_file_path(const char *name)
{
	return this->get_file_path(name, "");
}

char *fb_table::get_file_path(const char *name, const char *suffix)
{
	char *result;

	if (dir == NULL) {
		return NULL;
	}

	result = new char[strlen(dir) + 1 + strlen(name) + strlen(suffix) + 1];
	strcpy(result, dir);
	strcat(result, "/");
	strcat(result, name);
	strcat(result, suffix);

	return result;
}

const char *read_value(const char *line, const char *key)
{
	size_t i;

	/* skip whitespace */
	for (i = 0; isspace(line[0]); i++);

	if (strncmp(&line[i], key, strlen(key))) {
		MSG_DEBUG(MSG_MODULE, "reading value of '%s' failed: key differ", key);
		return NULL;
	}

	for (i += strlen(key); isspace(line[i]); i++);

	if (line[i] != '=') {
		MSG_DEBUG(MSG_MODULE, "reading value of '%s' failed: invalid format", key);
		return NULL;
	}

	for (i++; isspace(line[i]); i++);

	MSG_DEBUG(MSG_MODULE, "reading value of '%s': %s", key, &line[i]);
	return &line[i];
}

#define PART_LINE_LEN 16

bool parse_part_file(FILE *file, struct fb_table_header *header, std::vector<struct fb_column> &columns)
{
	char *line;
	size_t buf_size = PART_LINE_LEN;
	ssize_t line_len;
	const char *value;
	bool in_header = false;
	bool in_column = false;
	struct fb_column column;

	line = (char *) malloc(buf_size);
	if (!line) {
		return false;
	}

	while (1) {
		line_len = getline(&line, &buf_size, file);
		if (line_len  == -1) {
			break;
		}
		line[line_len-1] = 0;
		if (!strcmp(line, "BEGIN HEADER")) {
			in_header = true;
			continue;
		}
		if (!strcmp(line, "END HEADER")) {
			in_header = false;
			continue;
		}
		if (!strcmp(line, "Begin Column")) {
			in_column = true;
			column.type = ibis::UNKNOWN_TYPE;
			column.writer = NULL;
			column.name[0] = 0;
			continue;
		}
		if (!strcmp(line, "End Column")) {
			in_column = false;
			columns.push_back(column);
			continue;
		}

		if (in_header) {
			value = read_value(line, "Name");
			if (value) {
				header->name = strdup(value);
				continue;
			}
			value = read_value(line, "Description");
			if (value) {
				header->description = strdup(value);
				continue;
			}
			value = read_value(line, "Number_of_rows");
			if (value) {
				sscanf(value, "%lu", &header->nrows);
				continue;
			}
			value = read_value(line, "Number_of_columns");
			if (value) {
				sscanf(value, "%lu", &header->ncolumns);
				continue;
			}
			value = read_value(line, "Timestamp");
			if (value) {
				sscanf(value, "%ld", &header->timestamp);
				continue;
			}
		}

		if (in_column) {
			value = read_value(line, "name");
			if (value) {
				strncpy(column.name, value, sizeof(column.name));
				continue;
			}
			value = read_value(line, "data_type");
			if (value) {
				column.type = fastbit_type_from_str(value);
				continue;
			}
		}
	}

	free(line);
	return true;
}

