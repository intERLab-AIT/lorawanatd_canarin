/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#ifndef __UART_H__
#define __UART_H__

#include <fcntl.h>
#include <termios.h>
#include "lorawanatd.h"
#define BAUDRATE B19200

int set_interface_attribs(int fd, speed_t speed);

void set_mincount(int fd, int mcount);

int uart_write(struct lrwanatd *lw, char *buf, size_t len);

void setup_uart_events(struct lrwanatd * lw);

#endif
