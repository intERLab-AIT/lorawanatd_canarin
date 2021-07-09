#ifndef __CONTEXT_MANAGER_H__
#define __CONTEXT_MANAGER_H__
#include <stdint.h>
#include <stdlib.h>

#define CONTEXT_FILE "lwan_context.bin"

struct lwan_context {
	uint8_t network_join_mode;
	uint8_t confirmation_mode;
	/* lora firmware data */
	size_t ctx_len[7];
	char ctx[7][1024];
};

struct context_manager {
	struct http_client * client;
	int send_recv_cntr;
	char filename[255];
	int fd;
	struct lwan_context *lwan_ctx;
};

enum cmd_type;
struct command;

void context_manager_init(struct context_manager *ctx_mngr);
void context_manager_event(enum cmd_type cmd_type, struct command *cmd);

#endif /* __CONTEXT_MANAGER_H__ */
