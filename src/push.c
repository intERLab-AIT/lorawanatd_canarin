/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <malloc.h>
#include <sys/queue.h>
#include <arpa/inet.h>
#include <string.h>
#include <err.h>
#include "push.h"
#include "util.h"
#include "logger.h"


/* Push callbacks */
void push_recv(struct lrwanatd *lw, char *buf, size_t buflen);
void push_more_tx(struct lrwanatd *lw, char *buf, size_t buflen);

/* Initialized struct */
struct push_callbacks push_cb = {
	.recv = push_recv,
	.more_tx = push_more_tx,
};


int push_write(struct push_client *client, char *buf, size_t len)
{
	int wlen;
	wlen = write(client->fd, buf, len);
	if (wlen < len) {
		log(LOG_INFO, "short write, not all data echoed back to push client.\n");
		return RETURN_ERROR;
	}
	return wlen;
}

void push_recv(struct lrwanatd *lw, char *buf, size_t buflen)
{
	struct push_client *client;
	int wlen;

	char *pre, *post;

	log(LOG_INFO, "pushing rx: %.*s", buflen, buf);

	pre = "<rx=";
	post = ">";

	STAILQ_FOREACH(client, lw->push.push_clientq_head, entries) {
		if (client->state == PUSH_CLIENT_ACTIVE) {
			push_write(client, pre, strlen(pre));
			push_write(client, buf, buflen);
			push_write(client, post, strlen(post));
		}
	}
}

void push_more_tx(struct lrwanatd *lw, char *buf, size_t buflen)
{
	struct push_client *client;
	int wlen;
	char *res;

	log(LOG_INFO, "more tx available.");

	res = "<moretx>";

	STAILQ_FOREACH(client, lw->push.push_clientq_head, entries) {
		if (client->state == PUSH_CLIENT_ACTIVE) {
			push_write(client, res, strlen(res));
		}
	}
}

void on_read_push(evutil_socket_t fd, short what, void *arg)
{
	struct push_client *client = (struct push_client *)arg;
	ssize_t len;

	if (client->state == PUSH_CLIENT_DISCONNECTED)
		return;

	len = read(fd, (void *)&client->buf[client->buf_len],
			sizeof(client->buf) - client->buf_len);

	if (len == 0) {
		log(LOG_INFO, "push client disconnected.\n");
		client->state = PUSH_CLIENT_DISCONNECTED;
	}
	else if (len < 0) {
		log(LOG_INFO, "socket failure, disconnecting push client: %s",
				strerror(errno));
		client->state = PUSH_CLIENT_DISCONNECTED;
	}

}

void on_accept_push(evutil_socket_t fd, short what, void *arg)
{
	struct lrwanatd *lw = (struct lrwanatd *)arg;
	struct push_client *client;
	int client_fd;
	struct sockaddr_in client_addr;

	socklen_t client_len = sizeof(client_addr);

	client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
	if (client_fd == -1) {
		log(LOG_INFO, "push sock accept failed.");
		return;
	}

	if (set_nonblock_sock(client_fd) < 0)
		log(LOG_INFO, "push sock non blocking not set.");

	client = malloc(sizeof(struct push_client));
	client->fd = client_fd;
	client->state = PUSH_CLIENT_ACTIVE;

	STAILQ_INSERT_TAIL(lw->push.push_clientq_head, client, entries);

	client->read_event = event_new(lw->event.base, client_fd, EV_READ|EV_PERSIST,
			on_read_push, (void *)client);
	event_add(client->read_event, NULL);

	log(LOG_INFO, "accepted push connection from %s with fd %d\n",
			inet_ntoa(client_addr.sin_addr), client->fd);
}

void register_push_callbacks(struct lrwanatd *lw)
{
	lw->push.cb = &push_cb;
}

struct push_client_queue_head *init_push_client_queue()
{
	struct push_client_queue_head *head = malloc(sizeof(struct push_client_queue_head));
	STAILQ_INIT(head);
	return head;
}

void free_push_client(struct lrwanatd *lw, struct push_client *client)
{
	event_del(client->read_event);
	event_free(client->read_event);
	close(client->fd);
	STAILQ_REMOVE(lw->push.push_clientq_head, client, push_client, entries);
	free(client);
}

void remove_disconnected_push_clients(struct lrwanatd *lw)
{
	struct push_client *client, *client_next;

	client = STAILQ_FIRST(lw->push.push_clientq_head);

	while(client != NULL) {
		client_next = STAILQ_NEXT(client, entries);
		if (client->state == PUSH_CLIENT_DISCONNECTED) {
			log(LOG_INFO, "removing push client.");
			free_push_client(lw, client);
		}
		client = client_next;
	}
}

void setup_push_events(struct lrwanatd *lw)
{
	lw->event.push_listen = event_new(lw->event.base,
			lw->push.fd, EV_READ|EV_PERSIST, on_accept_push, (void *)lw);
	event_priority_set(lw->event.push_listen, 1);
	event_add(lw->event.push_listen, NULL);
}
