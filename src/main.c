/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <event2/event.h>
#include <signal.h>
#include "lorawanatd.h"
#include "logger.h"
#include "uart.h"
#include "http.h"
#include "push.h"
#include "util.h"

struct lrwanatd *global_lw;

static void log_cb(int priority, const char *msg)
{
	log(priority, msg);
}

int parse_opts(struct lrwanatd *lw, int argc, char **argv)
{
	int opt;
	while((opt = getopt(argc, argv, ":f:b:")) != -1) {
		switch(opt) {
			case 'f':
				strcpy(lw->uart.file, optarg);
				log(LOG_INFO, "uart device: %s", lw->uart.file);
				break;
			case ':':
				log(LOG_INFO, "option needs a value");
				break;
			case '?':
				log(LOG_INFO, "unknown option: %c", optopt);
				break;
		}
	}

	if (!strlen(lw->uart.file)) {
		log(LOG_INFO, "Invalid uart device parameters.");
		return RETURN_ERROR;
	}

	return RETURN_OK;
}

int init(struct lrwanatd *lw, int argc, char **argv)
{

	lw->pid = getpid();
	lw->sid = getsid(lw->pid);
	log(LOG_INFO, "LoRaWANATd started. Pid:%d, Sid:%d.", lw->pid, lw->sid);

	lw->http.http_clientq_head = init_http_client_queue();

	lw->push.push_clientq_head = init_push_client_queue();

	lw->http.fd = init_tcp_listen_sock(5555);

	if(lw->http.fd == RETURN_ERROR) {
		log(LOG_INFO, "error in opening http socket.");
		return RETURN_ERROR;
	} else
		log(LOG_INFO, "http socket opened successfully port 5555.");

	lw->push.fd = init_tcp_listen_sock(6666);

	if(lw->push.fd == RETURN_ERROR) {
		log(LOG_INFO, "error in opening push socket.");
		return RETURN_ERROR;
	} else
		log(LOG_INFO, "push socket opened successfully port 6666.");

	/* libevent */
#ifdef EVENT_LOG
	event_enable_debug_logging(EVENT_DBG_ALL);
#endif
	event_set_log_callback(log_cb);
	return RETURN_OK;
}

void clean(struct lrwanatd *lw)
{
	event_del(lw->event.http_listen);
	event_free(lw->event.http_listen);

	event_del(lw->event.push_listen);
	event_free(lw->event.push_listen);

	event_del(lw->event.uart_read);
	event_free(lw->event.uart_read);

	event_del(lw->event.timer_processor);
	event_free(lw->event.timer_processor);

	event_base_free(lw->event.base);

	close(lw->uart.fd);
	close(lw->http.fd);
	close(lw->push.fd);

	free(lw->http.http_clientq_head);
	free(lw->push.push_clientq_head);
	free(lw);
}

void sigint_handler(int signum)
{
	clean(global_lw);
	exit(signum);
}

int main(int argc, char **argv)
{
	global_lw = calloc(1, sizeof(struct lrwanatd));

	if (parse_opts(global_lw, argc, argv))
		return EXIT_FAILURE;

#ifndef NO_DEAMON
	if (daemon(0, 0) == -1) {
		log(LOG_INFO, strerror(errno));
		return EXIT_FAILURE;
	}
#endif

	signal(SIGINT, sigint_handler);

	if (init(global_lw, argc, argv))
		return EXIT_FAILURE;

	register_push_callbacks(global_lw);
	global_lw->event.base = event_base_new();
	event_base_priority_init(global_lw->event.base, 2);
	setup_uart_events(global_lw);
	setup_http_events(global_lw);
	setup_push_events(global_lw);

	event_base_dispatch(global_lw->event.base);

	clean(global_lw);

	return EXIT_SUCCESS;
}
