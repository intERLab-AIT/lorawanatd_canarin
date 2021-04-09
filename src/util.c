/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "util.h"
#include "lorawanatd.h"

char wspace_chars[] =  "\n\r\t ";
#define WSPACE_CHARS_LEN sizeof(wspace_chars)/sizeof(wspace_chars[0])

int init_tcp_listen_sock(int port, bool remote_mode)
{
	struct sockaddr_in listen_addr;
	int reuseaddr_on = 1;
	struct event *ev_accept;

	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0)
		return listen_fd;

	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
				&reuseaddr_on, sizeof(reuseaddr_on)) == -1)
		return RETURN_ERROR;

	memset(&listen_addr, 0, sizeof(listen_addr));

	listen_addr.sin_family = AF_INET;

	if (remote_mode)
		listen_addr.sin_addr.s_addr = INADDR_ANY;
	else	
		listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	listen_addr.sin_port = htons(port);

	if (bind(listen_fd, (struct sockaddr *)&listen_addr,
				sizeof(listen_addr)) < 0)
		return RETURN_ERROR;

	if (listen(listen_fd, 5) < 0)
		return RETURN_ERROR;

	if (set_nonblock_sock(listen_fd) < 0)
		return RETURN_ERROR;
	return listen_fd;
}

int set_nonblock_sock(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0)
		return flags;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return RETURN_ERROR;

	return RETURN_OK;
}

void str_to_hex(char *str, size_t len)
{
	int i;
	char* cp;

	cp = str;

	for (i = 0; i < len; ++cp, i++ )
	{
		printf("%02x", *cp);
		if (i < len -1)
			printf(":");
	}

	printf("\n");
}

size_t ltrim(char *buf, size_t len)
{
	int i,j;
	char c;
	bool is_wspace;
	for(i = 0; i < len; i++) {
		c = buf[i];
		is_wspace = false;
		for(j = 0; j < WSPACE_CHARS_LEN; j++) {
			if (wspace_chars[j] == c) {
				is_wspace = true;
				break;
			}
		}
		if (!is_wspace)
			break;
	}
	return i;
}

size_t rtrim(char *buf, size_t len)
{
	int i,j;
	char c;
	bool is_wspace;
	for(i = len - 1; i >= 0; i--) {
		c = buf[i];
		is_wspace = false;
		for(j = 0; j < WSPACE_CHARS_LEN; j++) {
			if (wspace_chars[j] == c) {
				is_wspace = true;
				break;
			}
		}
		if (!is_wspace)
			break;
	}
	return i;
}

char *trim(char *buf, size_t *len)
{
	size_t ltpos, rtpos;
	int i;

	if (!buf)
		return NULL;

	ltpos = ltrim(buf, *len);
	rtpos = rtrim(buf, *len);

	*len = rtpos - ltpos + 1;

	if (*len <= 0)
		return NULL;
	
	for(i = 0; i < *len; i++)
		buf[i] = buf[ltpos + i];

	buf[i] = '\0';

	return buf;
}
