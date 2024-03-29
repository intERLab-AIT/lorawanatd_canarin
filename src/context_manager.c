#include <stdio.h>
#include <string.h>
#include "context_manager.h"
#include "util.h"
#include "http.h"
#include "command.h"
#include "logger.h"

#define CTX_ACQUIRE_COUNT	10

/*
* Enumeration of modules which have a context
*/
typedef enum LoRaMacNvmCtxModule_e
{
	/*!
	 * Context for the MAC
	 */
	LORAMAC_NVMCTXMODULE_MAC,
	/*!
	 * Context for the regions
	 */
	LORAMAC_NVMCTXMODULE_REGION,
	/*!
	 * Context for the crypto module
	 */
	LORAMAC_NVMCTXMODULE_CRYPTO,
	/*!
	 * Context for the secure element
	 */
	LORAMAC_NVMCTXMODULE_SECURE_ELEMENT,
	/*!
	 * Context for the command queue
	 */
	LORAMAC_NVMCTXMODULE_COMMANDS,
	/*!
	 * Context for class b
	 */
	LORAMAC_NVMCTXMODULE_CLASS_B,
	/*!
	 * Context for the confirm queue
	 */
	LORAMAC_NVMCTXMODULE_CONFIRM_QUEUE,
}LoRaMacNvmCtxModule_t;


static struct lwan_context lwan_ctx;
static struct context_manager *this;

static char params_str[MAC_PARAM_MAX][255];


void read_context()
{
	log(LOG_INFO, "Read context file");
	FILE *f = fopen(this->filename, "rb");
	if (!f) {
		log(LOG_ERR, "Cannot open context file. %s", strerror(errno));
		return;
	}
	struct lwan_context lctx;
	if (fread(&lctx, 1, sizeof(struct lwan_context), f) != sizeof(struct lwan_context)) {
		log(LOG_ERR, "Error reading context file. %s", strerror(errno));
		fclose(f);
		return;
	}
	lwan_ctx = lctx;
	fclose(f);
}


void write_context()
{
	log(LOG_INFO, "Write context file");
	FILE *f = fopen(this->filename, "wb");
	if (!f) {
		log(LOG_ERR, "Cannot open context file. %s", strerror(errno));
		return;
	}
	if (fwrite(&lwan_ctx, 1, sizeof(struct  lwan_context), f) != sizeof(struct lwan_context)) {
		log(LOG_ERR, "error writing context file. %s", strerror(errno));
		fclose(f);
		return;
	}
	fclose(f);
}


bool update_cntr()
{
	if (++this->send_recv_cntr >= CTX_ACQUIRE_COUNT) {
		this->send_recv_cntr = 0;
		return true;
	}
	return false;
}

void set_mac_params() {
	struct http_client *client;
	struct lrwanatd *lw;
	struct command *cmd;
	char *token;
	union command_param param;
	log(LOG_INFO, "Setting mac params on firmware!\n");

	lw = global_lw;
	client = create_http_client(lw, 0);
	client->local = true;
	this->client = client;

	for (enum mac_pram_type_e param_type = 0; param_type < MAC_PARAM_MAX; param_type++) {
		uint16_t bit_check = MAC_PARAM_BIT(param_type);
		bool check = bit_check & lwan_ctx.mac_params.dirty;
		if (check) {
			switch (param_type) {
                case MAC_PARAM_ADR:
                    token = TOKEN_AT_ADR;
					memset(params_str[param_type], 0, 255);
					sprintf(params_str[param_type], "%u", lwan_ctx.mac_params.params[param_type]);
					break;
				case MAC_PARAM_DATA_RATE:
					token = TOKEN_AT_DR;
					memset(params_str[param_type], 0, 255);
					sprintf(params_str[param_type], "%u", lwan_ctx.mac_params.params[param_type]);
					break;
				case MAC_PARAM_TRANSMIT_POWER:
					token = TOKEN_AT_TXP;
					memset(params_str[param_type], 0, 255);
					sprintf(params_str[param_type], "%u", lwan_ctx.mac_params.params[param_type]);
					break;
				case MAC_PARAM_RX1_DELAY:
					token = TOKEN_AT_RX1DL;
					memset(params_str[param_type], 0, 255);
					sprintf(params_str[param_type], "%u", lwan_ctx.mac_params.params[param_type]);
					break;
				case MAC_PARAM_RX2_DELAY:
					token = TOKEN_AT_RX2DL;
					memset(params_str[param_type], 0, 255);
					sprintf(params_str[param_type], "%u", lwan_ctx.mac_params.params[param_type]);
					break;
				case MAC_PARAM_RX2_DATA_RATE:
					token = TOKEN_AT_RX2DR;
					memset(params_str[param_type], 0, 255);
					sprintf(params_str[param_type], "%u", lwan_ctx.mac_params.params[param_type]);
					break;
				default:
					continue; /* skip unknown bits */
			}
			param.set.param = params_str[param_type];
			param.set.param_len = strlen(params_str[param_type]);

			/* Force the command to execute in hardware */
			cmd = make_cmd(token, strlen(token),
						   &param, 0, CMD_SET);
			cmd->def.local_state = false;
			STAILQ_INSERT_TAIL(client->cmdq_head, cmd, entries);
			lwan_ctx.mac_params.dirty &= ~(MAC_PARAM_BIT(param_type));
		}
	}

	if  (!STAILQ_EMPTY(client->cmdq_head)) {
		log(LOG_INFO, "accepted local client");
		client->state = HTTP_CLIENT_REQUEST_COMPLETE;
		STAILQ_INSERT_TAIL(lw->http.http_clientq_head, client, entries);
	}
	else {
		free_http_client(lw, client);
		this->client = NULL;
		log(LOG_ERR, "cannot accept local client");
	}
}


void generate_ctx()
{
	struct http_client *client;
	struct lrwanatd *lw;
	struct command *cmd;
	log(LOG_INFO, "Initiating context save!\n");

	lw = global_lw;
	client = create_http_client(lw, 0);
	client->local = true;
	this->client = client;

	cmd = make_cmd(TOKEN_AT_DELAY, sizeof(TOKEN_AT_DELAY) - 1,
				   NULL, 5, CMD_INTERNAL);

	STAILQ_INSERT_TAIL(client->cmdq_head, cmd, entries);

	/* Initiate a acquire context command */
	cmd = make_cmd(TOKEN_AT_CTX_ACQ, sizeof(TOKEN_AT_CTX_ACQ) - 1,
				   NULL, 0, CMD_INTERNAL);

	if (!cmd) {
		free_http_client(lw, client);
		this->client = NULL;
		log(LOG_ERR, "cannot accept local client");
	}

	STAILQ_INSERT_TAIL(client->cmdq_head, cmd, entries);

	if (cmd) {
		log(LOG_INFO, "accepted local client");
		client->state = HTTP_CLIENT_REQUEST_COMPLETE;
		STAILQ_INSERT_TAIL(lw->http.http_clientq_head, client, entries);
	}
	else {
		free_http_client(lw, client);
		this->client = NULL;
		log(LOG_ERR, "cannot accept local client");
	}
}


void restore_firmware_context()
{
	struct http_client *client;
	struct lrwanatd *lw;
	struct command *cmd;

	log(LOG_INFO, "Initiating context restore!\n");

	lw = global_lw;
	client = create_http_client(lw, 0);
	client->local = true;
	client->restore_context = true;	/* trying to restore context */
	this->client = client;

	/* Initiate a acquire context command */
	for (LoRaMacNvmCtxModule_t type = 0; type <= LORAMAC_NVMCTXMODULE_CONFIRM_QUEUE; type++) {
		union command_param cmd_param;
		cmd_param.internal.context_type = type;
		if (lwan_ctx.ctx_len[type] == 0) {
			continue;
		}
		cmd = make_cmd(TOKEN_AT_CTX_RES, sizeof(TOKEN_AT_CTX_RES) - 1,
					   &cmd_param, 0, CMD_INTERNAL);
		if (cmd) {
			STAILQ_INSERT_TAIL(client->cmdq_head, cmd, entries);
			log(LOG_INFO, "added restore command for context type %d", type);
		}

		cmd = make_cmd(TOKEN_AT_DELAY, sizeof(TOKEN_AT_DELAY) - 1,
					   NULL, 2, CMD_INTERNAL);

		STAILQ_INSERT_TAIL(client->cmdq_head, cmd, entries);

	}

	/* Always trigger restore first */
	STAILQ_INSERT_HEAD(lw->http.http_clientq_head, client, entries);

	if (STAILQ_EMPTY(client->cmdq_head)) {
		free_http_client(lw, client);
		this->client = NULL;
		log(LOG_ERR, "cannot accept local client");
	}
	else {
		log(LOG_INFO, "accepted local client");
		client->state = HTTP_CLIENT_REQUEST_COMPLETE;
	}

}


void context_acquired(struct command *cmd)
{
	/* Split using delimiter '\r\n' */
	char ctx[1024];
	size_t ctx_len;

	char *start;
	char *end;

	start = cmd->buf;

	do {
		end = strstr(start, "\r\n");
		if (!end) {
			break;
		}
		ctx_len = end - start;
		strncpy(ctx, start, ctx_len);
		ctx[ctx_len] = '\0';
		if (strncmp(ctx, "+CTX=", 5) == 0) {
			printf("HAT: %s\n", ctx);
			LoRaMacNvmCtxModule_t type = (LoRaMacNvmCtxModule_t)(ctx[5] - '0');
			if (type > LORAMAC_NVMCTXMODULE_CONFIRM_QUEUE || type < LORAMAC_NVMCTXMODULE_MAC) {
				log(LOG_ERR, "Unknown context type %u", type);
				continue;
			}
			strcpy(lwan_ctx.ctx[type], ctx);
			lwan_ctx.ctx_len[type] = ctx_len;
		}
		start = end + 2;
	} while ((start - cmd->buf) <= cmd->buf_len);

	write_context();
}


void reset_lwan_ctx(bool delete)
{
	struct mac_params mac_params = {
		.dirty = 0x0000,
		.params = { PARAM_UNINIT },
	};

	memset(&lwan_ctx, 0 , sizeof(struct lwan_context));
	lwan_ctx.mac_params.network_join_mode = 1;
	lwan_ctx.mac_params.confirmation_mode = 0;
	lwan_ctx.mac_params = mac_params;
	if (delete)
		unlink(this->filename);
}


void context_manager_init(struct context_manager *ctx_mngr)
{
	reset_lwan_ctx(false);
	this = ctx_mngr;
	this->lwan_ctx = &lwan_ctx;
	/* Uninitialised all mac params */
	read_context();
	restore_firmware_context();
}

bool check_if_client_error()
{
	struct http_client *client = this->client;
	struct command *cmd;

	if (!client) {
		return false;
	}

	if (client->cmdq_head == NULL) {
		/* The client is not running anymore */
		return false;
	}
	STAILQ_FOREACH(cmd, client->cmdq_head, entries) {
		if (cmd == NULL) break;
		if (cmd->state == CMD_NEW || cmd->def.type == CMD_DELAY)
			continue;
		if (!is_buffer_contains(cmd->buf, cmd->buf_len, "OK")) {
			return true;
		}
	}

	return false;
}

void context_manager_event(enum cmd_type cmd_type, struct command *cmd)
{
	log(LOG_INFO, "Context manager event %u", cmd_type);
	switch (cmd_type) {
		case CMD_ASYNC_RECV:
		case CMD_SEND_BINARY:
		case CMD_SEND_TEXT:
			if (update_cntr()) {
				generate_ctx();
			}
			break;
		case CMD_JOIN:
			set_mac_params();
			generate_ctx();
			break;
		case CMD_ACQUIRE_CONTEXT:
			context_acquired(cmd);
			break;
		case CMD_RESTORE_CONTEXT:
			/* if we encounter error, restore again */
			if (check_if_client_error()) {
				restore_firmware_context();
			}
			break;
		case CMD_RESET:
			restore_firmware_context();
			break;
		case CMD_HARD_RESET:
			reset_lwan_ctx(true);
			// write_context();
			break;
        case CMD_FORCE_UPDATE:
            set_mac_params();
		default:
			break;
	}
}



