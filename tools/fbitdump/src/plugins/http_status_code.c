#define _GNU_SOURCE
#include <stdio.h>
#include "plugin_header.h"

void format( const union plugin_arg * arg, int plain_numbers, char * out ) {
	char *str = NULL;
	char num[15];

	switch (arg[0].uint32) {
		case 100:
			str = "100 Continue";
			break;
		case 101:
			str = "101 Switching Protocols";
			break;
		case 200:
			str = "200 OK";
			break;
		case 201:
			str = "201 Created";
			break;
		case 202:
			str = "202 Accepted";
			break;
		case 203:
			str = "203 Non-Authoritative Information";
			break;
		case 204:
			str = "204 No Content";
			break;
		case 205:
			str = "205 Reset Content";
			break;
		case 206:
			str = "206 Partial Content";
			break;
		case 300:
			str = "300 Multiple Choices";
			break;
		case 301:
			str = "301 Moved Permanently";
			break;
		case 302:
			str = "302 Found";
			break;
		case 303:
			str = "303 See Other";
			break;
		case 304:
			str = "304 Not Modified";
			break;
		case 305:
			str = "305 Use Proxy";
			break;
		case 306:
			str = "306 (Unused)";
			break;
		case 307:
			str = "307 Temporary Redirect";
			break;
		case 400:
			str = "400 Bad Request";
			break;
		case 401:
			str = "401 Unauthorized";
			break;
		case 402:
			str = "402 Payment Required";
			break;
		case 403:
			str = "403 Forbidden";
			break;
		case 404:
			str = "404 Not Found";
			break;
		case 405:
			str = "405 Method Not Allowed";
			break;
		case 406:
			str = "406 Not Acceptable";
			break;
		case 407:
			str = "407 Proxy Authentication Required";
			break;
		case 408:
			str = "408 Request Timeout";
			break;
		case 409:
			str = "409 Conflict";
			break;
		case 410:
			str = "410 Gone";
			break;
		case 411:
			str = "411 Length Required";
			break;
		case 412:
			str = "412 Precondition Failed";
			break;
		case 413:
			str = "413 Request Entity Too Large";
			break;
		case 414:
			str = "414 Request-URI Too Long";
			break;
		case 415:
			str = "415 Unsupported Media Type";
			break;
		case 416:
			str = "416 Requested Range Not Satisfiable";
			break;
		case 417:
			str = "417 Expectation Failed";
			break;
		case 500:
			str = "500 Internal Server Error";
			break;
		case 501:
			str = "501 Not Implemented";
			break;
		case 502:
			str = "502 Bad Gateway";
			break;
		case 503:
			str = "503 Service Unavailable";
			break;
		case 504:
			str = "504 Gateway Timeout";
			break;
		case 505:
			str = "505 HTTP Version Not Supported";
			break;
		default:
			snprintf(num, sizeof(num), "%u", arg[0].uint32);
			str = num;
			break;
	}

	snprintf(out, PLUGIN_BUFFER_SIZE, "%s", str);
}
