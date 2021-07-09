#include <stdio.h>
#include <string.h>
#include "context_manager.h"
#include "http.h"
#include "command.h"
#include "logger.h"

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

void read_context()
{
	log(LOG_INFO, "Read context file");
	FILE *f = fopen(CONTEXT_FILE, "rb");
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
	FILE *f = fopen(CONTEXT_FILE, "wb");
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
	if (++this->send_recv_cntr >= 10) {
		this->send_recv_cntr = 0;
		return true;
	}
	return false;
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

	/* Initiate a acquire context command */
	cmd = make_cmd(TOKEN_AT_CTX_ACQ, sizeof(TOKEN_AT_CTX_ACQ) - 1,
				   NULL, 0, CMD_INTERNAL);

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

	log(LOG_INFO, "Initiating context save!\n");

	lw = global_lw;
	client = create_http_client(lw, 0);
	client->local = true;
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

	}

	STAILQ_INSERT_TAIL(lw->http.http_clientq_head, client, entries);

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

void context_manager_init(struct context_manager *ctx_mngr)
{
	this = ctx_mngr;
	this->lwan_ctx = &lwan_ctx;
	memset(this->lwan_ctx, 0 , sizeof(struct lwan_context));
	this->lwan_ctx->network_join_mode = 1;
	this->lwan_ctx->confirmation_mode = 0;
	read_context();
	restore_firmware_context();
}


void context_manager_event(enum cmd_type cmd_type, struct command *cmd)
{
	switch (cmd_type) {
		case CMD_ASYNC_RECV:
		case CMD_SEND_BINARY:
		case CMD_SEND_TEXT:
			if (update_cntr()) {
				generate_ctx();
			}
			break;
		case CMD_JOIN:
			generate_ctx();
			break;
		case CMD_ACQUIRE_CONTEXT:
			context_acquired(cmd);
			break;
		default:
			break;
	}
}



