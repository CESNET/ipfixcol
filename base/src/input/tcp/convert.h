#ifndef __CONVERT_H
#define __CONVERT_H

enum {
	UDP_PLUGIN,
	TCP_PLUGIN,
	SCTP_PLUGIN
};

inline void modify();

int convert_init(int in_plugin, int len);

int templates_realloc();

void convert_close();

void convert_packet(char **packet, ssize_t *len, char *info_list);


#endif
