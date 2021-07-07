/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#ifndef __LORAWANATD_H__
#define __LORAWANATD_H__
#include <sys/types.h>
#include <sys/queue.h>
#include <event2/event.h>
#include <sys/epoll.h>
#include <stdbool.h>
#include <regex.h>

/* Function return status */
enum {
	RETURN_ERROR = -1,
	RETURN_OK = 0,
	RETURN_TRUE = 6,
	RETURN_FALSE = 7,
};

struct event_def {
	struct event_base *base;
	struct event *uart_read;
	struct event *uart_write;
	struct event *timer_processor;
	struct event *http_listen;
	struct event *push_listen;
};

STAILQ_HEAD(uart_tx_queue_head, uart_tx);

struct uart_tx {
	STAILQ_ENTRY(uart_tx) entries;
	char buf[1024];
	size_t buf_len;
};

struct uart_def {
	int fd;
	char file[255];
	unsigned int baudrate;
	char buf[8149];
	size_t buf_len;
	struct uart_tx_queue_head tx_q;
#if 0
	int epfd_in; /* epoll fd */
	int epfd_out; /* epoll fd */
	struct epoll_event ev_in;
	struct epoll_event ev_out;
#endif
};

struct http_def {
	int fd;
	struct http_client_queue_head *http_clientq_head;
};

struct push_def {
	int fd;
	struct push_client_queue_head *push_clientq_head;
	struct push_callbacks *cb;
};

/* All compiled regex goes here */
struct regex_def {
	regex_t recv;
	uint8_t n_recv_grps;
};

/* Persistant storage */
struct params_storage {
	uint8_t network_join_mode;
	uint8_t confirmation_mode;
	/* lora firmware data */
	size_t ctx_len[6];
	char ctx[6][1024];
};

struct lrwanatd {
	pid_t pid;
	pid_t sid;
	bool remote_mode;
	struct event_def event;
	struct uart_def uart;
	struct http_def http;
	struct push_def push;
	struct regex_def regex;
	struct params_storage params;
};

extern struct lrwanatd *global_lw;

#endif
