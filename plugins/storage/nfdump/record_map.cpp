/*
 * \file record_map.cpp
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

#include <stdlib.h>
#include <iostream>

extern "C" {
#include <ipfixcol/verbose.h>
}

#include <string.h>
#include <stdio.h>
#include <cstdio>
#include "config_struct.h"
#include "extensions.h"
#include "record_map.h"
#include "nfstore.h"
#include "nffile.h"

extern "C" {
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>
}

static lzo_align_t __LZO_MMODEL wrkmem[LZO1X_1_MEM_COMPRESS];

void FileHeader::newHeader(FILE *f, struct nfdumpConfig* conf){
	header_.magic = MAGIC;
	header_.version = LAYOUT_VERSION_1;
	header_.flags = 0;
	header_.NumBlocks = 0;
	if(conf->compression){
		header_.flags = header_.flags | FLAG_COMPRESSED;
	}
	memset(header_.ident,0,IDENTLEN);
	strcpy(header_.ident,conf->ident.c_str());
	position_ = ftell(f);
	updateHeader(f);
}

void FileHeader::updateHeader(FILE *f){

	//seek header location
	if((fseek(f, position_, SEEK_SET)) != 0){
		MSG_ERROR(MSG_MODULE,"Can't update file");
	}

	//write header
	if((fwrite(&header_,1,sizeof(struct file_header_s),f))
			!= sizeof(struct file_header_s)){
		MSG_ERROR(MSG_MODULE,"Can't update file");
	}
}

void Stats::newStats(FILE *f){
	memset(&stats_,0,sizeof(struct stat_record_s));
	position_ = ftell(f);
	updateStats(f);
}

void Stats::addStats(FlowStats *fstats){
	stats_.numflows++;
	stats_.numbytes+=fstats->bytes;
	stats_.numpackets+=fstats->packets;

	//update first seen flow timestamp if needed
	if(stats_.first_seen > fstats->first_ts){
		stats_.first_seen = fstats->first_ts;
		stats_.msec_first = fstats->first_msec_ts;
	}else if(stats_.first_seen == fstats->first_ts){
		if(stats_.msec_first > fstats->first_msec_ts){
			stats_.msec_first = fstats->first_msec_ts;
		}
	}

	stats_.last_seen = fstats->last_ts;
	stats_.msec_last = fstats->last_msec_ts;

	if(fstats->protocol == TCP){
		stats_.numflows_tcp++;
		stats_.numbytes_tcp+=fstats->bytes;
		stats_.numpackets_tcp+=fstats->packets;
	}else if(fstats->protocol == UDP){
		stats_.numflows_udp++;
		stats_.numbytes_udp+=fstats->bytes;
		stats_.numpackets_udp+=fstats->packets;
	}else if(fstats->protocol == ICMP){
		stats_.numflows_icmp++;
		stats_.numbytes_icmp+=fstats->bytes;
		stats_.numpackets_icmp+=fstats->packets;
	}else{
		stats_.numflows_other++;
		stats_.numbytes_other+=fstats->bytes;
		stats_.numpackets_other+=fstats->packets;
	}
}

void Stats::updateStats(FILE *f){
	//seek stats location
	if((fseek(f, position_, SEEK_SET)) != 0){
		MSG_ERROR(MSG_MODULE,"Can't update file stats");
	}

	//write stats
	if((fwrite(&stats_,1,sizeof(struct stat_record_s),f))
			!= sizeof(struct stat_record_s)){
		MSG_ERROR(MSG_MODULE,"Can't update file stats");
	}
}

void Stats::increaseSQFail(){
	stats_.sequence_failure++;
}


void BlockHeader::newBlock(FILE *f){
	block_.NumRecords = 0;
	block_.size = 0;
	block_.id = 2;
	position_ = ftell(f);
	updateBlock(f);
}

void BlockHeader::updateBlock(FILE *f){
	//seek block header
	if((fseek(f, position_, SEEK_SET)) != 0){
		MSG_ERROR(MSG_MODULE,"Can't update block header");
	}
	//write block header
	if((fwrite(&block_,1,sizeof(struct data_block_header_s),f))
			!= sizeof(struct data_block_header_s)){
		MSG_ERROR(MSG_MODULE,"Can't update block header");
	}
}



void BlockHeader::compress(char *buffer, uint *bufferUsed){ //TODO
	lzo_uint iSize,oSize;
	unsigned char __LZO_MMODEL *input;
	unsigned char __LZO_MMODEL *output;

	char *ibuff;

	ibuff = new char[block_.size];
	memcpy(ibuff,buffer,block_.size);

	input = (unsigned char __LZO_MMODEL *) ibuff;
	iSize = block_.size;
	output = (unsigned char __LZO_MMODEL *) buffer;
	oSize = 0;
	if(lzo1x_1_compress(input,iSize,output,&oSize,wrkmem) != LZO_E_OK){
		MSG_ERROR(MSG_MODULE,"Compression failed");
	}
	*bufferUsed = oSize;
	block_.size = oSize;
	delete[] ibuff;
	//TODO ret val
}


int NfdumpFile::newFile(std::string name, struct nfdumpConfig* conf){
	MSG_DEBUG(MSG_MODULE,"Creating new file: \"%s\"",name.c_str());
	f_ = fopen(name.c_str(),"w+");
	if(f_ == NULL){
		MSG_ERROR(MSG_MODULE,"Can't create file: \"%s\"",name.c_str());
		return -1;
	}
	//create header
	fileHeader_.newHeader(f_,conf);
	//create stats
	stats_.newStats(f_);
	fileHeader_.increaseBlockCnt();
	currentBlock_.newBlock(f_);

	extMaps_ = new std::map<uint16_t,RecordMap*>;
	if(extMaps_ == NULL){
		MSG_ERROR(MSG_MODULE, "Can't allocate memory for extensions");
		return -1;
	}

	bufferSize_ = conf->bufferSize;
	bufferUsed_ = 0;
	buffer_ = new char[BUFFER_SIZE];

	if(buffer_ == NULL){
		MSG_ERROR(MSG_MODULE,"Can't allocate memory");
		return -1;
	}
	return 0;
}

void NfdumpFile::updateFile(bool compression){
	uint check;

	if(f_ == NULL){
		MSG_ERROR(MSG_MODULE,"Can't update file");
		bufferUsed_ = 0;
		return;
	}
	//update file header
	fileHeader_.updateHeader(f_);
	//update stats
	stats_.updateStats(f_);
	//update last block header


	if(compression){
		currentBlock_.compress(buffer_, &bufferUsed_);
		currentBlock_.updateBlock(f_);
	}else{
		currentBlock_.updateBlock(f_);
	}

	//flush data from buffer
	check = fseek(f_, 0, SEEK_END);
	if(check != 0){
		MSG_ERROR(MSG_MODULE,"Can't update file");
	}
	check = fwrite(buffer_,1,bufferUsed_,f_);
	if(check != bufferUsed_){
		MSG_ERROR(MSG_MODULE,"Can't update file");
	}
	bufferUsed_ = 0;
}

unsigned int
NfdumpFile::bufferPtk(const data_template_couple *dtcouple){
	uint16_t template_id;
	std::map<uint16_t,RecordMap*>::iterator ext_map_it;
	RecordMap *map_tmp;
	static uint16_t map_id_cnt = 1;
	char *buffer;
	unsigned int flowCount = 0;

	if(f_ == NULL) return 0;

	/* message from ipfixcol have maximum of MSG_MAX_DATA_COUPLES data records */
	for(int i = 0 ; i < MSG_MAX_DATA_COUPLES; i++){
		if(dtcouple[i].data_set == NULL){
			//there are no more filled data_sets
			break;
		}

		if(dtcouple[i].data_template == NULL){
			//skip data without template!
			continue;
		}

		template_id =dtcouple[i].data_template->template_id;

		/* if there is unknown template parse it and add it to template map */
		if((ext_map_it = extMaps_->find(template_id)) == extMaps_->end()){
			MSG_DEBUG(MSG_MODULE,"Received new template: %hu", template_id);
			map_tmp = new RecordMap();
			map_tmp->init(dtcouple[i].data_template,map_id_cnt);
			map_id_cnt++;
			(*extMaps_)[template_id] = map_tmp;
			ext_map_it = extMaps_->find(template_id);
		}

		if(!ext_map_it->second->valid()){
			continue;
		}

		/* flush data if there is no space in buffers */
		if(bufferSize_ <= bufferUsed_ + ext_map_it->second->maxSize()){
			std::map<uint16_t,RecordMap*>::iterator maps_it;
			updateFile(fileHeader_.compressed());
			fileHeader_.increaseBlockCnt();
			currentBlock_.newBlock(f_);
			//for(maps_it = _ext_maps->begin(); maps_it!=_ext_maps->end();maps_it++){
			//	maps_it->second->clean_metadata();
			//}
		}

		/* store this data record */
		buffer = buffer_ + bufferUsed_;

		if(!ext_map_it->second->stored()){
			ext_map_it->second->stored(true);
			ext_map_it->second->genereate_map(buffer);
			bufferUsed_+= ext_map_it->second->size();
			currentBlock_.addRecordSize(ext_map_it->second->size());
			currentBlock_.increaseRecordsCnt();
			buffer = buffer_ + bufferUsed_;
		}
		//store extension data
		flowCount+= ext_map_it->second->bufferData(dtcouple[i].data_set, buffer,
						&bufferUsed_, &currentBlock_, &stats_);
	}
	return flowCount;
}

void NfdumpFile::checkSQNumber(unsigned int SQ, unsigned int recFlows){
	if(SQ != nextSQ_ ){
		stats_.increaseSQFail();
		MSG_DEBUG(MSG_MODULE,"SQ: %u expectedSQ: %u recFlows: %u nextSQ: %u",SQ,nextSQ_,recFlows,(SQ + recFlows) % 0xffffffff);
		nextSQ_ = SQ;

	}
	nextSQ_ = (nextSQ_ + recFlows) % 0xffffffff;
}

void NfdumpFile::closeFile(){
	std::map<uint16_t,RecordMap*>::iterator maps_it;

	if(f_ == NULL){
		return;
	}

	updateFile(fileHeader_.compressed());
	fclose(f_);


	for(maps_it = extMaps_->begin(); maps_it!=extMaps_->end();maps_it++){
		delete maps_it->second;
	}
	delete extMaps_;
	delete[] buffer_;
}

RecordMap::RecordMap() {
	memset(extensions_,0,sizeof(int)*MAX_EXT);
	minRecordSize_ = 0;
	recordSize_ = 0;
	mapSize_ = 0;
	mapId_ = 0;

	ids_ = NULL;
	idsSize_ = NULL;
	idsCnt_ = 0;
	idsAlloc_ = 20;
	idsExt_ = NULL;
	valid_ = true;
	mapStored_ = false;
	mapAlign_ = 0;

	extensions_[0] = new CommonBlock();
	//Needed extensions:
	extensions_[1] = new Extension1();
	extensions_[2] = new Extension2();
	extensions_[3] = new Extension3();
	//Optional extensions:
	extensions_[4] = new Extension5();
	extensions_[5] = new Extension7();
	extensions_[6] = new Extension8();
	extensions_[7] = new Extension9();
	extensions_[8] = new Extension10();
	extensions_[9] = new Extension11();
	extensions_[10] = new Extension12();
	extensions_[11] = new Extension13();
	extensions_[12] = new Extension15();
	extensions_[13] = new Extension17();
	extensions_[14] = new Extension19();
	extensions_[15] = new Extension20();
	extensions_[16] = new Extension21();
	extensions_[17] = new Extension22();
}

int RecordMap::init(struct ipfix_template *data_template,uint16_t map_id){
	int en_offset = 0;
	uint32_t ext_offset = 0;
	template_ie *field;

	mapId_ = map_id;
	mapSize_ = EXT_HEADER_SIZE + PAD_SIZE;

	ids_ = (uint16_t *) malloc(sizeof(uint16_t)*idsAlloc_);
	if(ids_ == NULL){
		return -1;
	}
	idsSize_ = (uint16_t *) malloc(sizeof(uint16_t)*idsAlloc_);
	if(idsSize_ == NULL){
		return -1;
	}

	//Is there anything to parse?
	if(data_template == NULL){
		return 1;
	}

	if(data_template->template_type != TM_TEMPLATE){
		return -1;
	}
	minRecordSize_ = 0;
	//Find elements
	for(int i=0;i<data_template->field_count + en_offset;i++){
		field = &(data_template->fields[i]);
		if(field->ie.length == VAR_IE_LENGTH){
			minRecordSize_ += 1;
		} else {
			minRecordSize_ += field->ie.length;
		}

		//reallocate buffer if needed
		if(idsCnt_ == idsAlloc_){
			idsAlloc_*=2;
			ids_ = (uint16_t *)realloc(ids_, sizeof(uint16_t)*idsAlloc_);
			if(ids_ == NULL){
					return -1;
			}
			idsSize_ = (uint16_t *)realloc(idsSize_, sizeof(uint16_t)*idsAlloc_);
			if(idsSize_ == NULL){
				return -1;
			}
		}

		//Is this an enterprise element?
		if(field->ie.id & 0x8000){
			i++;
			en_offset++;
			continue; //skip enterprise elements (no need for nfdump files)
		}

		//add id to buffer
		ids_[idsCnt_] = field->ie.id & 0x7FFF;
		idsSize_[idsCnt_] = field->ie.length;
		idsCnt_++;
	}

	idsExt_ = (class Extension **)malloc(sizeof(class Extension*) * idsAlloc_);
	//_ids_ext = new extension *[_ids_alloc];
	if(idsExt_ == NULL){
		return -1;
	}
	memset(idsExt_, 0, sizeof(class Extension*) * idsAlloc_);

	//check available elements for extensions and common record
	for(int i=0; i < MAX_EXT; i++){
		if(extensions_[i] == NULL) continue;

		if(!extensions_[i]->checkElements(idsCnt_, ids_, idsExt_)){
			extensions_[i]->used(false);
			if( i < 4){ //one of needed elements is missing
				valid_ = false;
				MSG_WARNING(MSG_MODULE,"Records with template %hu are ignored (wrong elements) ",data_template->template_id);
			}
		} else {
			extensions_[i]->used(true);
			extensions_[i]->offset(ext_offset);
			recordSize_+= extensions_[i]->size();

			if( i > 3){ //FIRST 3 extension are not included in map
				mapSize_ += EXTENSION_ID_SIZE;
			}

			MSG_DEBUG(MSG_MODULE,"Added extension: %hu (tmp: %hu ex_size: %hu offset: %u)",extensions_[i]->extId()
					,data_template->template_id, recordSize_, ext_offset);
			ext_offset+= extensions_[i]->size();
		}
	}
	//elements with variable size!
	for(int i=0;i<idsCnt_;i++){
		if(idsSize_[i] == 0xffff){
			idsExt_[i] = extensions_[0];
		}
	}

	mapAlign_ = mapSize_%4; // 32bit alignment
	mapSize_+= mapAlign_;
	return 0;
}

void RecordMap::genereate_map(char * buffer){
	struct extension_map_s *map;
	int offset = 0;

	map = (extension_map_s *) buffer;
	map->type = ExtensionMapType;
	map->size = mapSize_;
	map->map_id = mapId_;
	map->extension_size = 0;


	for(int i=4; i < MAX_EXT; i++){ // 0-3 extensions are MUST so they are not in ext map
			if(extensions_[i] == NULL) continue;
			if(!extensions_[i]->used()) continue;
			MSG_DEBUG(MSG_MODULE,"EXT in map: %u (off: %u addr: %p)",
					extensions_[i]->extId(), offset, buffer);

			map->ex_id[offset] = extensions_[i]->extId();
			offset++;
			map->extension_size+= extensions_[i]->size();
	}
	map->ex_id[offset] = 0; //PAD
	for(int i=0;i<mapAlign_/2;i++){ //32bit alignment
		map->ex_id[offset+1] = 0;
		MSG_DEBUG(MSG_MODULE,"ALIGMENT2 %u",mapId_);
	}
}

uint16_t RecordMap::bufferData(ipfix_data_set *data_set,char *buffer, uint *buffer_used,
		class BlockHeader * block, class Stats *stats){
	unsigned int data_size,read_data,cur_size;
	unsigned int flowCount = 0;
	struct FlowStats fstats;
	fstats.flags=0;
	int filled =0; //TODO

	uint8_t *data = data_set->records;
	if (data == NULL){
		return 0;
	}

	data_size = (ntohs(data_set->header.length)-(sizeof(struct ipfix_set_header)));
	read_data = 0;
	while(read_data < data_size){

		if((data_size - read_data) < minRecordSize_){
			//padding
			break;
		}

		memset(buffer+ filled,0,recordSize());
		//check buffer size!
		for(int i=0;i<idsCnt_;i++){
			cur_size = idsSize_[i];
			if(idsExt_[i] != NULL){
				cur_size = idsExt_[i]->fill(ids_[i],idsSize_[i],data,buffer+ filled, &fstats);
			}
			data += cur_size;
			read_data += cur_size;
		}

		extensions_[0]->fillHeader(buffer+filled,fstats.flags,0,mapId_,recordSize());
		filled += recordSize();
		stats->addStats(&fstats);
		block->increaseRecordsCnt();
		flowCount++;
	}
	block->addRecordSize(filled);
	(*buffer_used)+= filled;
	return flowCount;
}

void RecordMap::cleanMetadata(){
	mapStored_ = false;
}


RecordMap::~RecordMap() {
	for(int i=0; i < MAX_EXT; i++){
		if(extensions_[i] == NULL) continue;
		delete extensions_[i];
	}
	free (ids_);
	free (idsSize_);
	free (idsExt_);
}

