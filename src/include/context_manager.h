#ifndef __CONTEXT_MANAGER_H__
#define __CONTEXT_MANAGER_H__
#include <stdint.h>
#include <stdlib.h>

struct lwan_context {
    uint8_t network_join_mode;
    uint8_t confirmation_mode;
    /* lora firmware data */
    size_t ctx_len[6];
    char ctx[6][1024];
};

struct context_manager {
    struct http_client * client;
    char filename[255];
    int fd;
    struct lwan_context *lwan_ctx;
};

void context_manager_init(struct context_manager *ctx_mngr);
struct lwan_context * get_lwan_ctx();

#endif /* __CONTEXT_MANAGER_H__ */
