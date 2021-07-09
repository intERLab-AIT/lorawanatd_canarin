/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include "http.h"
#include "command.h"
#include "uart.h"
#include "util.h"
#include "logger.h"
#include "picohttpparser.h"
#include "jsmn.h"

#define HTTP_ERROR_500 "HTTP/1.1 500 Internal Server Error\nContent-Type: application/json\n\n{\"status\":\"ERROR\"}"
#define HTTP_ERROR_401 "HTTP/1.1 401 Not Found\nContent-Type: application/json\n\n{\"status\":\"ERROR\"}"


struct http_client_queue_head *init_http_client_queue()
{
	struct http_client_queue_head *head = malloc(sizeof(struct http_client_queue_head));
	STAILQ_INIT(head);
	return head;
}


char * get_http_action_string(enum http_action action)
{
	switch(action) {
		case HTTP_UNDEFINED:
			return "Undefined";
		case HTTP_RESET:
			return "Reset";
		case HTTP_STATUS:
			return "Status";
		case HTTP_JOIN:
			return "Join";
		case HTTP_SET_CONFIG:
			return "Set Config";
		case HTTP_GET_CONFIG:
			return "Get Config";
		case HTTP_SEND_DATA:
			return "Send Data";
		case HTTP_SENDB_DATA:
			return "Send Binary Data";
		default:
			return "Unknown Action";
	}
}


enum http_action get_action_from_http_request(struct http_client *client)
{
	if (strncmp("GET", client->request.method , client->request.method_len) == 0
			&& strncmp("/reset", client->request.path, client->request.path_len) == 0)
		return HTTP_RESET;

	if (strncmp("GET", client->request.method , client->request.method_len) == 0
			&& strncmp("/status", client->request.path, client->request.path_len) == 0)
		return HTTP_STATUS;

	if (strncmp("GET", client->request.method , client->request.method_len) == 0
			&& strncmp("/join", client->request.path, client->request.path_len) == 0)
		return HTTP_JOIN;

	if (strncmp("POST", client->request.method , client->request.method_len) == 0
			&& strncmp("/config/get", client->request.path, client->request.path_len) == 0)
		return HTTP_GET_CONFIG;

	if (strncmp("POST", client->request.method , client->request.method_len) == 0
			&& strncmp("/config/set", client->request.path, client->request.path_len) == 0)
		return HTTP_SET_CONFIG;

	if (strncmp("POST", client->request.method , client->request.method_len) == 0
			&& strncmp("/send", client->request.path, client->request.path_len) == 0)
		return HTTP_SEND_DATA;

	if (strncmp("POST", client->request.method , client->request.method_len) == 0
			&& strncmp("/sendb", client->request.path, client->request.path_len) == 0)
		return HTTP_SENDB_DATA;

	return HTTP_UNDEFINED;
}


int parse_http_buf(struct http_client *client, size_t len)
{
	int pret, minor_version;
	struct phr_header headers[48];
	size_t num_headers, prevbuflen;
	int i;

	prevbuflen = client->buf_len;

	num_headers = sizeof(headers) / sizeof(headers[0]);

	pret = phr_parse_request(client->buf, client->buf_len + len ,
			(const char **)&(client->request.method), &(client->request.method_len),
			(const char **)&(client->request.path), &(client->request.path_len),
			&minor_version, headers, &num_headers, prevbuflen);

	for (i = 0; i != num_headers; ++i) {
		if (strncmp("Content-Length", headers[i].name, headers[i].name_len) == 0) {
			char tmp[16];
			strncpy(tmp, headers[i].value, headers[i].value_len);
			tmp[headers[i].value_len] = '\0';
			client->request.content_len = atoi(tmp);
		}

		if (strncmp("Content-Type", headers[i].name, headers[i].name_len) == 0 &&
				strncmp("application/json", headers[i].value, headers[i].value_len) == 0) {
			client->is_json = true;
		}
	}

	if (pret > 0) { /* request complete */
		client->request.header_len = pret;
		client->action = get_action_from_http_request(client);
		log(LOG_INFO, "%.*s %.*s Json?%s Content-Length: %d Action:%s",
				client->request.method_len, client->request.method,
				client->request.path_len, client->request.path,
				client->is_json? "yes": "no",
				client->request.content_len,
				get_http_action_string(client->action));
		return RETURN_OK;
	}
	else if (pret == -1) /* error */
		return RETURN_ERROR;

	return 1; /* more to go */
}

int add_cmd(struct http_client *client)
{
	struct command *cmd = NULL;

	switch(client->action) {
		case HTTP_RESET:
			cmd = make_cmd(TOKEN_AT_RESET, sizeof(TOKEN_AT_RESET) - 1,
					NULL, 5, CMD_ACTION);
			break;
		case HTTP_STATUS:
			cmd = make_cmd(TOKEN_AT, sizeof(TOKEN_AT) - 1,
					NULL, 0, CMD_ACTION);
			break;
		case HTTP_JOIN:
	/* Timeout of 1 minute */
			cmd = make_cmd(TOKEN_AT_JOIN , sizeof(TOKEN_AT_JOIN) - 1,
					NULL, 60,CMD_ACTION);
			break;
	}
	if (cmd)
		STAILQ_INSERT_TAIL(client->cmdq_head, cmd, entries);
	else
		return RETURN_ERROR;

	return RETURN_OK;
}

int parse_json_content_add_cmd(struct http_client *client)
{
	struct command *cmd = NULL;
	char *data, *port;
	size_t data_len, port_len;
	jsmn_parser p;
	jsmntok_t t[128];
	jsmntok_t *tok, *tok1, *tok2;
	jsmn_init(&p);
	memset(t, 0, sizeof(t));
	int i, ret = jsmn_parse(&p, client->request.content, client->request.content_len, t, 128);
	if (ret < 0)
		return ret;

	switch(client->action) {
		case HTTP_GET_CONFIG:
			/* 	The request should be of the type
			*	[
			*	"command1",
			*	"command2",
			*	....
			*	]
			*/
			if (t[0].type != JSMN_ARRAY)
				return RETURN_ERROR;

			// First pass check if all tokens are string
			for (i =1; i < t[0].size + 1; i++) {
				tok = &t[i];
				if (tok->type != JSMN_STRING)
					return RETURN_ERROR;
			}


			for (i =1; i < t[0].size + 1; i++) {
				tok = &t[i];
				char *tkstr = client->request.content + tok->start;
				size_t tklen = tok->end - tok->start;
				/* 1 minute timeout */
				cmd = make_cmd(tkstr, tklen, NULL, 60, CMD_GET);
				if (cmd)
					STAILQ_INSERT_TAIL(client->cmdq_head, cmd, entries);
				else
					return RETURN_ERROR;
			}


			break;
		case HTTP_SET_CONFIG:
			/* The request should be of the type
			*	{
			*		"command1": "param1",
			*		"command2": "param2",
			*		....
			*	}
			*/
			if (t[0].type != JSMN_OBJECT)
				return RETURN_ERROR;

			for (i =1; i < t[0].size * 2 + 1; i += 2) {
				union command_param cmd_param;
				tok1 = &t[i];
				tok2 = &t[i + 1];

				char *tkstr = client->request.content + tok1->start;
				size_t tklen = tok1->end - tok1->start;

				char *param = client->request.content + tok2->start;
				size_t paramlen = tok2->end - tok2->start;
				cmd_param.set.param = param;
				cmd_param.set.param_len = paramlen;
				/* 1 minute timeout */
				cmd = make_cmd(tkstr, tklen, &cmd_param, 60, CMD_SET);
				if (cmd)
					STAILQ_INSERT_TAIL(client->cmdq_head, cmd, entries);
				else
					return RETURN_ERROR;
			}

			break;
		case HTTP_SEND_DATA:
		case HTTP_SENDB_DATA:
			/*	The request should be of the type
			*	{
			*		"data": "thisisdata",
			*		"port": 21,
			*	}
			*/
			data_len = port_len = 0;
			data = port = NULL;

			if (t[0].type != JSMN_OBJECT)
				return RETURN_ERROR;

			for (i =1; i < t[0].size * 2 + 1; i += 2) {
				tok1 = &t[i];
				tok2 = &t[i + 1];

				char *tkstr = client->request.content + tok1->start;
				size_t tklen = tok1->end - tok1->start;

				char *param = client->request.content + tok2->start;
				size_t paramlen = tok2->end - tok2->start;

				if (!strncmp(tkstr, "data", tklen)) {
					data = param;
                    data_len = paramlen;
				}
				else if (!strncmp(tkstr, "port", tklen)) {
					port = param;
                    port_len = paramlen;
				}
			}

			if (data && port) {
				union command_param cmd_param;
				cmd_param.send.param = data;
				cmd_param.send.param_len = data_len;
				cmd_param.send.port = port;
				cmd_param.send.port_len = port_len;

				if (client->action == HTTP_SEND_DATA)
					cmd = make_cmd(TOKEN_AT_SEND, sizeof(TOKEN_AT_SEND) - 1,
                                   &cmd_param, 0, CMD_SEND);
				else
					cmd = make_cmd(TOKEN_AT_SENDB, sizeof(TOKEN_AT_SENDB) - 1,
                                   &cmd_param, 0, CMD_SEND);

				if (cmd)
					STAILQ_INSERT_TAIL(client->cmdq_head, cmd, entries);
				else
					return RETURN_ERROR;
			} else {
				return RETURN_ERROR;
			}
	}
	return RETURN_OK;
}


void on_read_http(evutil_socket_t fd, short what, void *arg)
{
	struct http_client *client = (struct http_client *)arg;
	ssize_t len;

	if (client->state == HTTP_CLIENT_DISCONNECTED)
		return;

	len = read(fd, (void *)&client->buf[client->buf_len],
			sizeof(client->buf) - client->buf_len);

	if (len == 0) {
		log(LOG_INFO, "http client disconnected.\n");
		client->state = HTTP_CLIENT_DISCONNECTED;
		return;
	}
	else if (len < 0) {
		log(LOG_INFO, "socket failure, disconnecting http client: %s",
				strerror(errno));
		client->state = HTTP_CLIENT_DISCONNECTED;
		return;
	}

	int ret = parse_http_buf(client, len);
	client->buf_len += len;

	log(LOG_INFO, "%.*s", client->buf_len, client->buf);

	if (ret == 0) {
		if (client->action == HTTP_UNDEFINED) { /* Nothing to do here */
			strcpy(client->error_resp, HTTP_ERROR_401);
			client->state = HTTP_CLIENT_ERROR;
			return;
		}
		if (client->request.content_len &&
				client->buf_len >= (client->request.header_len + client->request.content_len)) {
			client->state = HTTP_CLIENT_REQUEST_COMPLETE;
			client->request.content = client->buf + client->request.header_len;

			if (client->is_json && parse_json_content_add_cmd(client) < 0) {
				log(LOG_INFO, "JSON parse error.");
				strcpy(client->error_resp, HTTP_ERROR_500);
				client->state = HTTP_CLIENT_ERROR;
			}
		}
		if (!client->request.content_len && !client->is_json) {
			client->state = HTTP_CLIENT_REQUEST_COMPLETE;

			if(add_cmd(client) < 0) {
				log(LOG_INFO, "add command error");
				strcpy(client->error_resp, HTTP_ERROR_500);
				client->state = HTTP_CLIENT_ERROR;
			}
		}

	}
	else if (ret == -1) {
		client->state = HTTP_CLIENT_ERROR;
	}
}

struct http_client * create_http_client(struct lrwanatd *lw, int fd)
{
    struct http_client *client;
    client = malloc(sizeof(struct http_client));
    client->fd = fd;
    client->cmdq_head = init_cmd_queue();
    client->is_json = client->timed_out =  false;
    client->buf_len = client->request.path_len =
    client->request.header_len = client->request.method_len =
    client->request.content_len = 0;
    client->state = HTTP_CLIENT_ACTIVE;
    strcpy(client->error_resp, HTTP_ERROR_500);
    memset(client->buf, 0, sizeof(client->buf));
    return client;
}

void on_accept_http(evutil_socket_t fd, short what, void *arg)
{
	struct lrwanatd *lw = (struct lrwanatd *)arg;
	struct http_client *client;
	int client_fd;
	struct sockaddr_in client_addr;

	socklen_t client_len = sizeof(client_addr);

	client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
	if (client_fd == -1) {
		log(LOG_INFO, "http sock accept failed.");
		return;
	}

	if (set_nonblock_sock(client_fd) < 0)
		log(LOG_INFO, "http sock non blocking not set.");

	client = create_http_client(lw, client_fd);

	/* This is not local client */
	client->local = false;

	client->read_event = event_new(lw->event.base, client_fd, EV_READ|EV_PERSIST,
								   on_read_http, (void *)client);
	event_priority_set(client->read_event, 1);


	STAILQ_INSERT_TAIL(lw->http.http_clientq_head, client, entries);

	event_add(client->read_event, NULL);

	log(LOG_INFO, "accepted http connection from %s with fd %d\n",
			inet_ntoa(client_addr.sin_addr), client->fd);
}

char *reply_get_cmds(struct http_client *client)
{
	char *buf = calloc(1, 8196);;
	char *sptr = buf;
	strcpy(sptr, "[\n");
	sptr+= 2;
	struct command *cmd;
	STAILQ_FOREACH(cmd, client->cmdq_head, entries) {
		strcpy(sptr++, "\"");
		trim(cmd->buf, &cmd->buf_len);
		strncpy(sptr, cmd->buf, cmd->buf_len);
		sptr += cmd->buf_len;
		strcpy(sptr++, "\"");
		if (STAILQ_NEXT(cmd, entries))
			strcpy(sptr++, ",");
		strcpy(sptr++, "\n");
	}
	strcpy(sptr, "]\n");
	return buf;
}

int http_client_write(struct http_client *client, char *buf, size_t len)
{
	size_t wlen;
	wlen = write(client->fd, buf, len);
	if (wlen < len) {
		log(LOG_INFO, "short write, not all data echoed back to http client.\n");
		return RETURN_ERROR;
	}
	return wlen;
}

void set_http_client_uart_buf(struct lrwanatd *lw, char *buf, size_t len)
{
	struct http_client *client;
	struct command *cmd;

	client = STAILQ_FIRST(lw->http.http_clientq_head);

	/* If a client does not exist, then ignore the buf */
	if (client && client->state != HTTP_CLIENT_ERROR)
		set_active_cmd_uart_buf(client->cmdq_head, buf, len);
}

void free_http_client(struct lrwanatd *lw, struct http_client *client)
{
	if (!client->local) {
		event_del(client->read_event);
		event_free(client->read_event);
		close(client->fd);
	}
	free_cmd_queue(client->cmdq_head);
	STAILQ_REMOVE(lw->http.http_clientq_head, client, http_client, entries);
	free(client);
}

void remove_disconnected_http_clients(struct lrwanatd *lw)
{
	struct http_client *client, *client_next;
	client = STAILQ_FIRST(lw->http.http_clientq_head);

	while(client != NULL) {
		client_next = STAILQ_NEXT(client, entries);
		if (client->state == HTTP_CLIENT_DISCONNECTED) {
			log(LOG_INFO, "removing http client.");
			free_http_client(lw, client);
		}
		client = client_next;
	}
}


void process_http_clients(struct lrwanatd *lw)
{
	struct http_client *client;
	struct command *cmd;
	enum cmd_res_code cmdres;
	char *httpres, *jsondata;

	client = STAILQ_FIRST(lw->http.http_clientq_head);

	if (client) {
		/*	Two cases
		*	1. Command has been successfully executed and all the incoming data has been parsed
		*	2. TCP has error and connection closes midway
		*/

		if (client->state == HTTP_CLIENT_REQUEST_COMPLETE) {
			cmd = STAILQ_FIRST(client->cmdq_head);
			while(cmd && cmd->state != CMD_NEW) {
				if (cmd->state == CMD_EXECUTING) {
					cmdres = cmd->def.process_cmd(cmd);
					switch (cmdres) {
						case CMD_RES_TIMEOUT:
							client->timed_out = true;
							log(LOG_INFO, "command timed out.");
						case CMD_RES_OK:
							cmd->state = CMD_EXECUTED;
							log(LOG_INFO, "rx[len:%d]: %.*s", cmd->buf_len, cmd->buf_len, cmd->buf);
							/* Clear the global buffer */
							clear_uart_buf(&(lw->uart.buf_len));
							/* Signal for store */
							context_manager_event(cmd->def.type, cmd);
							break;
						default:
							break;
					}
					return;
				} else if (cmd->state == CMD_ERROR) {
					/* Send a bunch of new line to try recover from error. */
					/* Rude-mentry */
					log(LOG_INFO, "Command error!!!!!");
					uart_write(lw, "\r\n\r\n\r\n\r\n", 8);
					cmd->state = CMD_EXECUTED;
					return;
				}
				cmd = STAILQ_NEXT(cmd, entries);
			}

			if(!cmd) {
				if (client->local) {
					/* If the client is local, there is no fd to write data to */
					free_http_client(lw, client);
					return;
				}

				if (client->timed_out)
					httpres = "HTTP/1.1 504 Gateway Timeout\nContent-Type: application/json\n\n";
				else
					httpres = "HTTP/1.1 200 OK\nContent-Type: application/json\n\n";

				jsondata = reply_get_cmds(client);

				http_client_write(client, httpres, strlen(httpres));
				http_client_write(client, jsondata, strlen(jsondata));

				free(jsondata);

                free_http_client(lw, client);

			}
		}
		else if (client->state < HTTP_CLIENT_ACTIVE) {
			assert(!client->local);
			/* Disconnect, error, anything which is not active or request complete */
			http_client_write(client, client->error_resp, strlen(client->error_resp));
			/* delete all commands */
			free_http_client(lw, client);
		}
	}
}

void setup_http_events(struct lrwanatd *lw)
{
	lw->event.http_listen = event_new(lw->event.base,
			lw->http.fd, EV_READ|EV_PERSIST, on_accept_http, (void *)lw);
	event_priority_set(lw->event.http_listen, 1);
	event_add(lw->event.http_listen, NULL);
}
