/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#ifndef __PUSHRX_H__
#define __PUSHRX_H__
#include <sys/queue.h>
#include <event2/event.h>
#include "lorawanatd.h"

typedef void (*push_async_cb)(struct lrwanatd *lw, char *buf, size_t buflen);

struct push_callbacks {
	push_async_cb recv;
	push_async_cb more_tx;
};

STAILQ_HEAD(push_client_queue_head, push_client);

enum push_client_state {
	PUSH_CLIENT_ACTIVE,
	PUSH_CLIENT_DISCONNECTED,
};

struct push_client {
	STAILQ_ENTRY(push_client) entries;
	enum push_client_state state;
	int fd;
	struct event *read_event;
	unsigned char buf[8196];
	size_t buf_len;
};

struct push_client_queue_head *init_push_client_queue();

void register_push_callbacks(struct lrwanatd *lw);

void free_push_client(struct lrwanatd *lw, struct push_client *client);

void remove_disconnected_push_clients(struct lrwanatd *lw);

int push_write(struct push_client *client, char *buf, size_t len);

void setup_push_events(struct lrwanatd *lw);

#endif
