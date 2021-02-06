/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <event2/event.h>
#include <malloc.h>
#include <assert.h>
#include "lorawanatd.h"
#include "logger.h"
#include "uart.h"
#include "command.h"
#include "http.h"
#include "push.h"
#include "util.h"

// 0.5 sec
#define TIMER_USEC_INTERVAL 500000
// 0.5 sec
#define READ_DELAY_USEC 500000

int set_interface_attribs(int fd, speed_t speed)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0) {
		log(LOG_INFO, "error from tcgetattr: %s\n", strerror(errno));
		return RETURN_ERROR;
	}

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;         /* 8-bit characters */
	tty.c_cflag &= ~PARENB;     /* no parity bit */
	tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
	tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

	/* setup for non-canonical mode */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tty.c_oflag &= ~OPOST;

	/* fetch bytes as they become available */
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 2; // 0.5 seconds

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		log(LOG_INFO, "error from tcsetattr: %s\n", strerror(errno));
		return RETURN_ERROR;
	}

	return RETURN_OK;
}

#if 0
void set_mincount(int fd, int mcount)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0) {
		printf("Error tcgetattr: %s\n", strerror(errno));
		return;
	}

	tty.c_cc[VMIN] = mcount ? 1 : 0;
	tty.c_cc[VTIME] = 5;        /* half second timer */

	if (tcsetattr(fd, TCSANOW, &tty) < 0)
		printf("Error tcsetattr: %s\n", strerror(errno));
}

#endif

int uart_write(struct lrwanatd *lw, char *buf, size_t len)
{
  struct uart_tx *tx = malloc(sizeof(struct uart_tx));
  strncpy(tx->buf, buf, len);
  tx->buf_len = len;
  STAILQ_INSERT_TAIL(&lw->uart.tx_q, tx, entries);
  return len;
}

void remove_disconnected_clients(struct lrwanatd *lw)
{
	remove_disconnected_push_clients(lw);
	remove_disconnected_http_clients(lw);
}

#define READ_SIZE 1
#if 0
void cb_read(evutil_socket_t fd, short what, void *arg)
{
	struct lrwanatd *lw;
	char buf[READ_SIZE];
	int ret;

	lw = (struct lrwanatd *)arg;

	ret = read(fd, buf, READ_SIZE);
	if (ret <= 0)
		return;
	else if(ret != 1)
		log(LOG_INFO, "read size = %d and not 1.", ret);

	/*	While reading, check for asynchronous events.
	*	Then clean the buffer. Then pass it to client if possible.
	*/

	memcpy(lw->uart.buf + lw->uart.buf_len, buf, ret);
	lw->uart.buf_len += ret;

	run_async_cmd(lw, lw->uart.buf, lw->uart.buf_len);

	/* TODO: the cleaning part */

	set_http_client_uart_buf(lw, buf, ret);
}
#endif

int uart_dev_read(evutil_socket_t fd, short what, void *arg)
{
	struct lrwanatd *lw;
	char buf[READ_SIZE];
	int ret;

	lw = (struct lrwanatd *)arg;

	ret = read(fd, buf, READ_SIZE);
	if (ret <= 0)
		return 0;
	else if(ret != 1) {
		log(LOG_INFO, "read size = %d and not 1.", ret);
    return 1;
  }

	/*	While reading, check for asynchronous events.
	*	Then clean the buffer. Then pass it to client if possible.
	*/

	memcpy(lw->uart.buf + lw->uart.buf_len, buf, ret);
	lw->uart.buf_len += ret;

	run_async_cmd(lw, lw->uart.buf, lw->uart.buf_len);

	/* TODO: the cleaning part */

	set_http_client_uart_buf(lw, buf, ret);
  return ret;
}

void uart_dev_write(evutil_socket_t fd, short what, void *arg)
{
  struct lrwanatd *lw;
  struct uart_tx *tx;
	int wlen;

  lw = (struct lrwanatd *)arg;

  tx = STAILQ_FIRST(&lw->uart.tx_q);

  if (!tx)
    return;

	wlen = write(lw->uart.fd, tx->buf, tx->buf_len);
  tcdrain(lw->uart.fd);

	if (wlen != tx->buf_len)
		log(LOG_INFO, "write to uart failed.");
  else {
    STAILQ_REMOVE_HEAD(&lw->uart.tx_q, entries);
    free(tx);
  }
}

#if 0
void uart_epoll_setup(struct lrwanatd *lw)
{
  lw->uart.epfd_in = epoll_create(1);
  lw->uart.epfd_out = epoll_create(1);

  lw->uart.ev_in.events = EPOLLIN;
  lw->uart.ev_out.events = EPOLLOUT;

  lw->uart.ev_in.data.fd = lw->uart.ev_out.data.fd = lw->uart.fd;
  epoll_ctl(lw->uart.epfd_in, EPOLL_CTL_ADD, lw->uart.fd, &lw->uart.ev_in);
  epoll_ctl(lw->uart.epfd_out, EPOLL_CTL_ADD, lw->uart.fd, &lw->uart.ev_out);
}

void uart_epoll_teardown(struct lrwanatd *lw)
{
  close(lw->uart.epfd_in);
  close(lw->uart.epfd_out);
}

bool uart_ready_read(struct lrwanatd *lw)
{
  int nfds;
  nfds = epoll_wait(lw->uart.epfd_in, &lw->uart.ev_in, 1, 0);

  if (nfds < 0) {
    log(LOG_INFO, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (nfds > 0)
    return true;
  return false;
}

bool uart_ready_write(struct lrwanatd *lw)
{
  int nfds;
  nfds = epoll_wait(lw->uart.epfd_out, &lw->uart.ev_out, 1, 0);

  if (nfds < 0) {
    log(LOG_INFO, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (nfds > 0)
    return true;
  return false;
}


#endif

void uart_reset(struct lrwanatd *lw, bool teardown)
{
  if (teardown) {
    // uart_epoll_teardown(lw);
    close(lw->uart.fd);
		log(LOG_INFO, "closing %s.", lw->uart.file);
  }
	// open uart device
	lw->uart.fd = open(lw->uart.file, O_RDWR, O_NOCTTY);
	if(lw->uart.fd == -1) {
		log(LOG_INFO, "error in opening %s.", lw->uart.file);
    exit(EXIT_FAILURE);
	} else
		log(LOG_INFO, "%s opened successfully.", lw->uart.file);

	set_interface_attribs(lw->uart.fd, BAUDRATE);
  // uart_epoll_setup(lw);
}


void process_write(evutil_socket_t fd, short what, void *arg)
{
	struct lrwanatd *lw;
	struct http_client *client;
	struct command *cmd;
	char *buf;
	size_t buflen;
	int wlen, ret;

	lw = (struct lrwanatd *)arg;

	client = STAILQ_FIRST(lw->http.http_clientq_head);

	if (client) {
		if (client->state == HTTP_CLIENT_REQUEST_COMPLETE) {
			cmd = STAILQ_FIRST(client->cmdq_head);

			while(cmd && cmd->state != CMD_NEW) {
				if (cmd->state == CMD_EXECUTING) {
					return;
				}
				cmd = STAILQ_NEXT(cmd, entries);
			}

			if (cmd) {
				assert(cmd->state == CMD_NEW);

				buf = cmd->def.get_cmd(cmd);
				buflen = strlen(buf);

				log(LOG_INFO, "tx[len:%d]: %s", buflen, buf);
				// str_to_hex(buf, buflen);
        
        if (cmd->def.type == CMD_RESET)
          uart_reset(lw, true);

				if (uart_write(lw, buf, buflen) == RETURN_ERROR) {
					cmd->state = CMD_ERROR;
					free(buf);
					return;
				}

				free(buf);

				// Write enter key
				if (uart_write(lw, "\r\n", 2) == RETURN_ERROR) {
					cmd->state = CMD_ERROR;
					return;
				}
				cmd->state = CMD_EXECUTING;
			}
		}
	}
}

void uart_io(struct lrwanatd *lw)
{
  bool read = false;
  /*
  while(uart_ready_read(lw)) {
    cb_read(lw->uart.fd, 0, lw);
    read = true;
  }

  if (!read && uart_ready_write(lw)) {
    uart_dev_write(lw->uart.fd, 0, lw);
  }
  */
  while(uart_dev_read(lw->uart.fd, 0, lw) > 0) {
    read = true;
  }

  if (!read) {
    uart_dev_write(lw->uart.fd, 0, lw);
  }
}

void cb_timer(evutil_socket_t fd, short what, void *arg);

void setup_uart_loop_timer(struct lrwanatd *lw, bool isInit)
{
	struct timeval timer = { 0, TIMER_USEC_INTERVAL };

  if (!isInit)
	  evtimer_del(lw->event.timer_processor);
  lw->event.timer_processor = evtimer_new(lw->event.base, cb_timer, (void *)lw);
  evtimer_add(lw->event.timer_processor, &timer);
}

void cb_timer(evutil_socket_t fd, short what, void *arg)
{
	struct lrwanatd *lw = (struct lrwanatd *)arg;

	remove_disconnected_clients(lw);
	process_http_clients(lw);
  process_write(fd, what, arg);
  //cb_write(fd, what, arg);
  uart_io(lw);
  setup_uart_loop_timer(lw, false);
}

void setup_uart_events(struct lrwanatd *lw)
{
  STAILQ_INIT(&lw->uart.tx_q);

  /*
	lw->event.uart_read = event_new(lw->event.base, lw->uart.fd,
			EV_TIMEOUT|EV_READ|EV_PERSIST,
			cb_read, (void *)lw);
	event_priority_set(lw->event.uart_read, 0);

	lw->event.uart_write = event_new(lw->event.base, lw->uart.fd,
			EV_WRITE|EV_PERSIST,
			cb_write, (void *)lw);
	event_priority_set(lw->event.uart_write, 1);
  */
  uart_reset(lw, false);

  setup_uart_loop_timer(lw, true);

	//event_add(lw->event.uart_read, NULL);
	// event_add(lw->event.uart_write, NULL);
}
