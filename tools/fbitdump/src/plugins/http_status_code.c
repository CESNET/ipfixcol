#define _GNU_SOURCE
#include <stdio.h>
#include <strings.h>
#include "plugin_header.h"

typedef struct status_s {
	int code;
	char *name;
} status_t;

static const status_t status[] = {
	{ 100, "100 Continue" },
	{ 101, "101 Switching Protocols" },
	{ 102, "102 Processing" },
	{ 200, "200 OK" },
	{ 201, "201 Created" },
	{ 202, "202 Accepted" },
	{ 203, "203 Non-Authoritative Information" },
	{ 204, "204 No Content" },
	{ 205, "205 Reset Content" },
	{ 206, "206 Partial Content" },
	{ 207, "207 Multi-Status" },
	{ 208, "208 Already Reported" },
	{ 226, "226 IM Used" },
	{ 300, "300 Multiple Choices" },
	{ 301, "301 Moved Permanently" },
	{ 302, "302 Found" },
	{ 303, "303 See Other" },
	{ 304, "304 Not Modified" },
	{ 305, "305 Use Proxy" },
	{ 306, "306 (Unused)" },
	{ 307, "307 Temporary Redirect" },
	{ 308, "308 Permanent Redirect" },
	{ 400, "400 Bad Request" },
	{ 401, "401 Unauthorized" },
	{ 402, "402 Payment Required" },
	{ 403, "403 Forbidden" },
	{ 404, "404 Not Found" },
	{ 405, "405 Method Not Allowed" },
	{ 406, "406 Not Acceptable" },
	{ 407, "407 Proxy Authentication Required" },
	{ 408, "408 Request Timeout" },
	{ 409, "409 Conflict" },
	{ 410, "410 Gone" },
	{ 411, "411 Length Required" },
	{ 412, "412 Precondition Failed" },
	{ 413, "413 Request Entity Too Large" },
	{ 414, "414 Request-URI Too Long" },
	{ 415, "415 Unsupported Media Type" },
	{ 416, "416 Requested Range Not Satisfiable" },
	{ 417, "417 Expectation Failed" },
	{ 422, "422 Unprocessable Entity" },
	{ 423, "423 Locked" },
	{ 424, "424 Failed Dependency" },
	{ 426, "426 Upgrade Required" },
	{ 428, "428 Precondition Required" },
	{ 429, "429 Too Many Requests" },
	{ 431, "431 Request Header Fields Too Large" },
	{ 500, "500 Internal Server Error" },
	{ 501, "501 Not Implemented" },
	{ 502, "502 Bad Gateway" },
	{ 503, "503 Service Unavailable" },
	{ 504, "504 Gateway Timeout" },
	{ 505, "505 HTTP Version Not Supported" },
	{ 506, "506 Variant Also Negotiates" },
	{ 507, "507 Insufficient Storage" },
	{ 508, "508 Loop Detected" },
	{ 510, "510 Not Extended" },
	{ 511, "511 Network Authentication Required" }
};

#define MSG_CNT (sizeof(status) / sizeof(status[0]))

char *info()
{
	return \
"Converts HTTP status description to code and vice versa.\n \
e.g. \"Gateway Timeout\" -> 504";
}

void format(const plugin_arg_t * arg, int plain_numbers, char out[PLUGIN_BUFFER_SIZE], void *conf)
{
	char *str = NULL;
	char num[15];
	int i, size = MSG_CNT;
	
	for (i = 0; i < size; ++i) {
		if (status[i].code == arg->val[0].uint32) {
			str = status[i].name;
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
		if (!strcasecmp(input, status[i].name + 4)) {
			code = status[i].code;
			break;
		}
	}
	
	// Return empty string if HTTP status description was not found
	if (code == size) {
		snprintf(out, PLUGIN_BUFFER_SIZE, "%s", "");
		return;
	}
	
	snprintf(out, PLUGIN_BUFFER_SIZE, "%d", code);
}
