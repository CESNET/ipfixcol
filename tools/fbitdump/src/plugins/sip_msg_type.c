#define _GNU_SOURCE
#include <stdio.h>
#include <strings.h>
#include "plugin_header.h"

typedef struct msg_type_s {
	int code;
	char *name;
} msg_type_t;

static const msg_type_t msg_types[] = {
	{ 0, "Invalid" },
    { 1, "Invite" },
    { 2, "Ack" },
    { 3, "Cancel" },
    { 4, "Bye" },
    { 5, "Register" },
    { 6, "Options" },
    { 7, "Publish" },
    { 8, "Notify" },
    { 9, "Info" },
    { 10, "Subscribe" },
    { 99, "Status" },
    { 100, "Trying" },
    { 101, "Dial Established" },
    { 180, "Ringing" },
    { 183, "Session Progress" },
    { 200, "OK" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 407, "Proxy Auth Required" },
    { 486, "Busy Here" },
    { 487, "Request Canceled" },
    { 500, "Internal Error" },
    { 603, "Decline" },
    { 999, "Undefined" }
};

#define MSG_CNT (sizeof(msg_types) / sizeof(msg_types[0]))

char *info()
{
	return \
"Converts SIP message type description to code and vice versa.\n \
e.g. \"Ringing\" -> 180";
}

void format(const plugin_arg_t * arg, int plain_numbers, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	char *str = NULL;
	char num[15];
	int i, size = MSG_CNT;
	
	for (i = 0; i < size; ++i) {
		if (msg_types[i].code == arg->val[0].uint32) {
			str = msg_types[i].name;
			break;
		}
	}
	
	if (str == NULL) {
		snprintf(num, sizeof(num), "%u", arg->val[0].uint32);
		str = num;
	}

	snprintf(out, PLUGIN_BUFFER_SIZE, "%s", str);
}

void parse(char *input, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	int code, i, size = MSG_CNT;
	
	for (i = 0; i < size; ++i) {
		if (!strcasecmp(input, msg_types[i].name)) {
			code = msg_types[i].code;
			break;
		}
	}
	
	// Return empty string if SIP message type description was not found
	if (i == size) {
		snprintf(out, PLUGIN_BUFFER_SIZE, "%s", "");
		return;
	}
	
	snprintf(out, PLUGIN_BUFFER_SIZE, "%d", code);
}
