/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#ifndef __SOCK_H__
#define __SOCK_H__
#include <sys/queue.h>
#include <event2/event.h>
#include <stdbool.h>
#include "lorawanatd.h"

STAILQ_HEAD(http_client_queue_head, http_client);

enum http_action {
	HTTP_UNDEFINED,
	HTTP_RESET,
	HTTP_STATUS,
	HTTP_JOIN,
	HTTP_GET_CONFIG,
	HTTP_SET_CONFIG,
	HTTP_SEND_DATA,
	HTTP_SENDB_DATA,
};

enum http_client_state {
	HTTP_CLIENT_ERROR,
	HTTP_CLIENT_DISCONNECTED,
	HTTP_CLIENT_ACTIVE,
	HTTP_CLIENT_REQUEST_COMPLETE,
};

struct http_request_def {
	char *path;
	size_t path_len;
	char *method;
	size_t method_len;
	char *content; // the body of the request
	size_t content_len;
	size_t header_len;
};

struct http_client {
	STAILQ_ENTRY(http_client) entries;
	int fd; // file descriptor
	struct event *read_event;
	struct cmd_queue_head *cmdq_head; // commands for this client
	unsigned char buf[8196];
	size_t buf_len;
	enum http_action action;
	struct http_request_def request;
	enum http_client_state state;
	bool is_json;
	bool timed_out;
	char error_resp[255];
	bool local; /* True if client in an internal client */
};

struct http_client_queue_head *init_http_client_queue();

void setup_http_events(struct lrwanatd * lw);

int init_http_listen_sock(int port);

int http_client_write(struct http_client *client, char* buf, size_t len);

void set_http_client_uart_buf(struct lrwanatd *lw, char *buf, size_t len);

void process_http_clients(struct lrwanatd *lw);

void remove_disconnected_http_clients(struct lrwanatd *lw);

struct http_client * create_http_client(struct lrwanatd *lw, int fd);

void free_http_client(struct lrwanatd *lw, struct http_client *client);
#endif
