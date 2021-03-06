/**
 * \file ipfixsend/reader.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Functions for reading IPFIX file
 *
 * Copyright (C) 2016 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ipfixcol.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "reader.h"
#include "ipfixsend.h"

/** Maximum IPFIX packet size (2^16) */
#define MAX_PACKET_SIZE 65536

/** Internal representation of the packet reader                             */
struct reader_internal {
	FILE *file;          /**< Input file                                     */
	size_t next_id;      /**< Index of next packet                           */
	bool is_preloaded;   /**< Is the whole file preloaded                    */

	struct ipfix_header **packets_preload;   /**< Preloaded packets          */
	uint8_t packet_single[MAX_PACKET_SIZE];  /**< Internal buffer            */

	struct {
		bool   valid;      /**< Position validity flag                       */
		fpos_t pos_offset; /**< Position (only for non-preloaded)            */
		size_t pos_idx;    /**< Position (only for preloaded)                */
	} pos;  /**< Pushed position in the file */
};

// Function prototypes
static struct ipfix_header **
reader_preload_packets(reader_t *reader);
static void
reader_free_preloaded_packets(struct ipfix_header **packets);


// Create a new packet reader
reader_t *
reader_create(const char *file, bool preload)
{
	reader_t *new_reader = calloc(1, sizeof(*new_reader));
	if (!new_reader) {
		ERR_MEM;
		return NULL;
	}

	new_reader->file = fopen(file, "rb");
	if (!new_reader->file) {
		fprintf(stderr, "Unable to open input file '%s': %s\n", file,
			strerror(errno));
		free(new_reader);
		return NULL;
	}

	new_reader->is_preloaded = preload;
	if (preload) {
		new_reader->packets_preload = reader_preload_packets(new_reader);
		if (new_reader->packets_preload == NULL) {
			fclose(new_reader->file);
			free(new_reader);
			return NULL;
		}

		// We don't need file anymore...
		fclose(new_reader->file);
		new_reader->file = NULL;
	}

	return new_reader;
}


// Destroy a packet reader
void
reader_destroy(reader_t *reader)
{
	if (!reader) {
		return;
	}

	if (reader->is_preloaded) {
		reader_free_preloaded_packets(reader->packets_preload);
	}

	if (reader->file) {
		fclose(reader->file);
	}

	free(reader);
}

/**
 * \brief Read the the IPFIX header from a file
 * \param[in]  reader Pointer to the reader
 * \param[out] header IPFIX header
 * \return On success returns #READER_OK and fills the \p header. When
 *   end-of-file occurs returns #READER_EOF. Otherwise (failed to read
 *   the header, malformed header) returns #READER_ERROR and the content of
 *   the \p header is undefined.
 */
static enum READER_STATUS
reader_load_packet_header(reader_t *reader, struct ipfix_header *header)
{
	size_t read_size = fread(header, 1, IPFIX_HEADER_LENGTH, reader->file);
	if (read_size != IPFIX_HEADER_LENGTH) {
		if (read_size == 0 && feof(reader->file)) {
			return READER_EOF;
		} else {
			fprintf(stderr, "Unable to read a packet header (probably "
				"malformed packet).\n");
			return READER_ERROR;
		}
	}

	if (ntohs(header->version) != IPFIX_VERSION) {
		fprintf(stderr, "Invalid version of a packet header.\n");
		return READER_ERROR;
	}

	uint16_t new_size = ntohs(header->length);
	if (new_size < IPFIX_HEADER_LENGTH) {
		fprintf(stderr, "Invalid size a packet in the packet header.\n");
		return READER_ERROR;
	}

	return READER_OK;
}

/**
 * \brief Load the next packet from a file to a user defined buffer
 *
 * Load message from the the file and store it into the \p out_buffer.
 * If the size of the buffer (\p out_size) is too small, the fuction will fail
 * the position in the file is unchanged and a content of the buffer is
 * undefined.
 * \param[in]     reader      Pointer to the packet reader
 * \param[in,out] out_buffer  Pointer to the output buffer
 * \param[in,out] out_size    Size of the output buffer
 * \return On success returns #READER_OK, fills the \p output_buffer and
 *   \p out_size. When the size of ther buffer is too small, returns
 *   #READER_SIZE and the buffer is unchanged. When end-of-file occurs, returns
 *   #READER_EOF. Otherwise (i.e. malformed packet) returns #READER_ERROR.
 */
static enum READER_STATUS
reader_load_packet_buffer(reader_t *reader, uint8_t *out_buffer,
	size_t *out_size)
{
	// Load the packet header
	struct ipfix_header header;
	enum READER_STATUS status = reader_load_packet_header(reader, &header);
	if (status != READER_OK) {
		return status;
	}

	uint16_t new_size = ntohs(header.length);
	if (*out_size < new_size) {
		if (fseek(reader->file, -IPFIX_HEADER_LENGTH, SEEK_CUR)) {
			fprintf(stderr, "fseek error: %s\n", strerror(errno));
			return READER_ERROR;
		}
		return READER_SIZE;
	}

	// Load the rest of the packet
	memcpy(out_buffer, (const uint8_t *) &header, IPFIX_HEADER_LENGTH);
	uint8_t *body_ptr = out_buffer + IPFIX_HEADER_LENGTH;
	uint16_t body_size = new_size - IPFIX_HEADER_LENGTH;

	if (body_size > 0 && fread(body_ptr, body_size, 1, reader->file) != 1) {
		fprintf(stderr, "Unable to read a packet!\n");
		return READER_ERROR;
	}

	*out_size = new_size;
	return READER_OK;
}

/**
 * \brief Allocate a memory large enought and load the next packet from a file
 *   into it.
 *
 * User MUST manualy free the packet later.
 * \note You should check EOF before calling this function.
 * \param[in] reader        Pointer to the packet reader
 * \param[out] packet_data  Pointer to newly allocated packet
 * \param[out] packet_size  Size of the new packet (can be NULL)
 * \return On success returns #READER_OK and fills the \p packet_data and
 *   \p packet_size. When end-of-file occurs, returns #READER_EOF. Otherwise
 *   (i.e. malformed packet, memory allocation error) returns #READER_ERROR.
 */
static enum READER_STATUS
reader_load_packet_alloc(reader_t *reader, struct ipfix_header **packet_data,
	uint16_t *packet_size)
{
	// Load the packet header
	struct ipfix_header header;
	uint8_t *result;

	enum READER_STATUS status = reader_load_packet_header(reader, &header);
	if (status != READER_OK) {
		return status;
	}

	uint16_t new_size = ntohs(header.length);
	result = (uint8_t *) malloc(new_size);
	if (!result) {
		ERR_MEM;
		return READER_ERROR;
	}

	memcpy(result, (const uint8_t *) &header, IPFIX_HEADER_LENGTH);
	uint8_t *body_ptr = result + IPFIX_HEADER_LENGTH;
	uint16_t body_size = new_size - IPFIX_HEADER_LENGTH;

	if (body_size > 0 && fread(body_ptr, body_size, 1, reader->file) != 1) {
		fprintf(stderr, "Unable to read a packet!\n");
		free(result);
		return READER_ERROR;
	}

	if (packet_size) {
		*packet_size = new_size;
	}

	*packet_data = (struct ipfix_header *) result;
	return READER_OK;
}

/**
 * \brief Read packets from IPFIX file and store them into memory
 *
 * Result represents a NULL-terminated array of pointers to the packets.
 * \param[in] reader  Pointer to the packet reader
 * \return On success returns a pointer to the array. Otherwise returns NULL.
 */
static struct ipfix_header **
reader_preload_packets(reader_t *reader)
{
	enum READER_STATUS status;
	size_t pkt_cnt = 0;
	size_t pkt_max = 2048;

	struct ipfix_header **packets = calloc(pkt_max, sizeof(*packets));
	if (!packets) {
		ERR_MEM;
		return NULL;
	}

	while (1) {
		// Read packet
		status = reader_load_packet_alloc(reader, &packets[pkt_cnt], NULL);
		if (status == READER_EOF) {
			break;
		}

		if (status != READER_OK) {
			// Failed -> Delete all loaded packets
			for (size_t i = 0; i < pkt_cnt; ++i) {
				free(packets[i]);
			}

			free(packets);
			return NULL;
		}

		// Move array index to next packet - resize array if needed
		pkt_cnt++;
		if (pkt_cnt < pkt_max) {
			continue;
		}

		size_t new_max = 2 * pkt_max;
		struct ipfix_header** new_packets;

		new_packets = realloc(packets, new_max * sizeof(*packets));
		if (!new_packets) {
			ERR_MEM;
			// Failed -> Delete all loaded packets
			for (size_t i = 0; i < pkt_cnt; ++i) {
				free(packets[i]);
			}

			free(packets);
			return NULL;
		}

		packets = new_packets;
		pkt_max = new_max;
	}

	packets[pkt_cnt] = NULL;
	return packets;
}

/**
 * \brief Free allocated memory for packets
 * \param[in] packets Array of packets
 */
static void
reader_free_preloaded_packets(struct ipfix_header **packets)
{

	for (size_t i = 0; packets[i] != NULL; ++i) {
		free(packets[i]);
	}

	free(packets);
}


// Rewind file (go to the beginning of a file)
void
reader_rewind(reader_t *reader)
{
	if (reader->is_preloaded) {
		reader->next_id = 0;
	} else {
		rewind(reader->file);
	}
}

// Push the current position in a file
enum READER_STATUS
reader_position_push(reader_t *reader)
{
	reader->pos.valid = false;
	if (reader->is_preloaded) {
		reader->pos.pos_idx = reader->next_id;
	} else {
		if (fgetpos(reader->file, &reader->pos.pos_offset)) {
			fprintf(stderr, "fgetpos() error: %s\n", strerror(errno));
			return READER_ERROR;
		}
	}

	reader->pos.valid = true;
	return READER_OK;
}

// Pop the previously pushed position in the file
enum READER_STATUS
reader_position_pop(reader_t *reader)
{
	if (reader->pos.valid == false) {
		fprintf(stderr, "Internal Error: reader_position_pop()\n");
		return READER_ERROR;
	}

	reader->pos.valid = false;
	if (reader->is_preloaded) {
		reader->next_id = reader->pos.pos_idx;
	} else {
		if (fsetpos(reader->file, &reader->pos.pos_offset)) {
			fprintf(stderr, "fsetpos() error: %s\n", strerror(errno));
			return READER_ERROR;
		}
	}

	return READER_OK;
}

// Get the pointer to a next packet
enum READER_STATUS
reader_get_next_packet(reader_t *reader, struct ipfix_header **output,
	uint16_t *size)
{
	struct ipfix_header *packet = NULL;

	if (reader->is_preloaded) {
		// Read from memory
		packet = reader->packets_preload[reader->next_id];
		if (packet == NULL) {
			return READER_EOF;
		}

		++reader->next_id;
	} else {
		// Read from the file
		size_t b_size = MAX_PACKET_SIZE;
		enum READER_STATUS ret;

		ret = reader_load_packet_buffer(reader, reader->packet_single, &b_size);
		if (ret == READER_EOF) {
			return READER_EOF;
		}

		if (ret != READER_OK) {
			// Buffer should be big enought, so only an error can occur
			return READER_ERROR;
		}

		packet = (struct ipfix_header *) reader->packet_single;
	}

	*output = packet;
	if (size) {
		*size = ntohs(packet->length);
	}

	return READER_OK;
}

// Get the pointer to header of a next packet
enum READER_STATUS
reader_get_next_header(reader_t *reader, struct ipfix_header **header)
{
	if (reader->is_preloaded) {
		// Read from memory
		struct ipfix_header *packet = reader->packets_preload[reader->next_id];
		if (packet == NULL) {
			return READER_EOF;
		}

		++reader->next_id;
		*header = packet;
	} else {
		// Read from the file
		enum READER_STATUS status;
		struct ipfix_header *header_buffer;

		header_buffer = (struct ipfix_header *) reader->packet_single;
		status = reader_load_packet_header(reader, header_buffer);
		if (status != READER_OK) {
			return status;
		}

		// Seek to the next header
		uint16_t packet_size = ntohs(header_buffer->length);
		uint16_t body_size = packet_size - IPFIX_HEADER_LENGTH;

		if (fseek(reader->file, (long) body_size, SEEK_CUR)) {
			fprintf(stderr, "fseek error: %s\n", strerror(errno));
			return READER_ERROR;
		}

		*header = header_buffer;
	}

	return READER_OK;
}
