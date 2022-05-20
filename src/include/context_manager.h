#ifndef __CONTEXT_MANAGER_H__
#define __CONTEXT_MANAGER_H__
#include <stdint.h>
#include <stdlib.h>

#define CONTEXT_FILE "lwan_context.bin"

#define PARAM_UNINIT UINT8_MAX

/* Mac params, that gets reset after join. Check ResetMacParameters() in firmware. */
enum mac_pram_type_e {
	MAC_PARAM_DATA_RATE = 0,
	MAC_PARAM_TRANSMIT_POWER,
	MAC_PARAM_RX1_DELAY,
	MAC_PARAM_RX2_DELAY,
	MAC_PARAM_RX2_DATA_RATE,
	MAC_PARAM_MAX,
};

#define MAC_PARAM_BIT(mac_param_type)  (0x1 << mac_param_type)

struct mac_params {
	uint16_t dirty; /* use this to update */
	uint8_t network_join_mode;
	uint8_t confirmation_mode;
	uint32_t params[MAC_PARAM_MAX];
};

struct lwan_context {
	struct mac_params mac_params;
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
