/*
 * \file record_map.h
 * \author Petr Kramolis <kramolis@cesnet.cz>
 * \brief nfdump storage plugin
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
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

#ifndef RECORDMAP_H_
#define RECORDMAP_H_

#include <stdint.h>
#include "nffile.h"
#include <ipfixcol/storage.h>
#include <stdio.h>
#include <string>
#include <map>




struct FlowStats{

	FlowStats(): bytes(0), packets(0), protocol(0), first_ts(0),
		first_msec_ts(0), last_ts(0), last_msec_ts(0), flags(0) {}

	uint64_t bytes;
	uint64_t packets;
	uint8_t protocol;
	uint32_t first_ts;
	uint16_t first_msec_ts;
	uint32_t last_ts;
	uint16_t last_msec_ts;
	uint32_t flags;
};

class Stats{
	enum{ TCP = 6, UDP = 17, ICMP = 1};
	struct stat_record_s stats_;
	long position_;
public:
	uint size(){return sizeof(struct stat_record_s);}
	void newStats(FILE *f);
	void addStats(struct FlowStats *fstats);
	void updateStats(FILE *f);
	void increaseSQFail();
};

class BlockHeader {
	enum{HEADER_SIZE=12,MAX_SIZE=500/*MAX_SIZE=4294967295*/};
	struct data_block_header_s block_;
	long position_;
public:
	uint size(){return HEADER_SIZE;}
	void increaseRecordsCnt(){block_.NumRecords++;}
	void addRecordSize(uint32_t size){block_.size+=size;}
	void newBlock(FILE *f);
	void compress(char *buffer, uint *bufferUsed);
	void updateBlock(FILE *f);
};

class FileHeader{
	struct file_header_s header_;
	long position_;
public:
	uint size(){return sizeof(struct file_header_s);}
	bool compressed(){return header_.flags & FLAG_COMPRESSED;}
	void increaseBlockCnt(){header_.NumBlocks++;};
	void newHeader(FILE *f, struct nfdumpConfig* conf);
	void updateHeader(FILE *f);
};


class RecordMap {
	enum{MAX_EXT=18, EXT_HEADER_SIZE=8, EXTENSION_ID_SIZE=2, PAD_SIZE=2};
	uint32_t minRecordSize_;
	uint16_t recordSize_;
	uint16_t mapSize_;
	bool valid_;
	bool mapStored_;
	uint16_t mapAlign_;
	uint16_t *ids_;			//IDs of IPFIX elements
	uint16_t *idsSize_;	//size of IPFIX elements
	class Extension **idsExt_; //nfdump extension for IPFIX elements
	uint16_t idsCnt_;		//count of IDs
	uint16_t idsAlloc_; 	//allocated size of IDs and IDs size arrays
	class Extension *extensions_[MAX_EXT];
	uint16_t mapId_;

public:
	RecordMap();
	int init(struct ipfix_template *dataTemplate,uint16_t mapId);
	void genereate_map(char *buffer);
	bool stored(){return mapStored_;}
	void stored(bool stored){mapStored_ = stored;}
	uint16_t bufferData(ipfix_data_set *dataSet,char *buffer,uint *bufferUsed,
			class BlockHeader *curBlock,class Stats *stats);
	void cleanMetadata();// clean metadata for block
	virtual ~RecordMap();
	uint16_t recordSize(){return recordSize_;}
	bool valid(){return valid_;}
	uint16_t size(){return mapSize_;}
	uint maxSize(){return recordSize_ + mapSize_;}
};

class NfdumpFile{
	enum {BUFFER_SIZE_ = 512000};
	FILE * f_;
	//HEADER
	class FileHeader fileHeader_;
	class Stats stats_;
	class BlockHeader currentBlock_;
	std::map<uint16_t,RecordMap*> *extMaps_;
	unsigned int nextSQ_;

	char *buffer_;
	/* buffer allocated size */
	unsigned int bufferSize_;
	/* buffer number of bytes used in buffer */
	unsigned int bufferUsed_;
public:
	int newFile(std::string name, struct nfdumpConfig* conf);
	void updateFile(bool compression = false);
	unsigned int bufferPtk(const struct data_template_couple dtcouple[]);
	void checkSQNumber(unsigned int SQ, unsigned int recFlows);
	void closeFile();
};


#endif /* RECORDMAP_H_ */
