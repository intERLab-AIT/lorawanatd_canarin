/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#ifndef __UTIL_H__
#define __UTIL_H__

int init_tcp_listen_sock(int port, bool remote_mode);
int set_nonblock_sock(int fd);
void str_to_hex(char *str, size_t len);
char *trim(char *buf, size_t *len);

#endif
