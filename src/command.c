/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#include <malloc.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <regex.h>
#include <stdlib.h>
#include "command.h"
#include "util.h"
#include "logger.h"
#include "push.h"

#define DEFAULT_TIMEOUT 15
/* AT_SLAVE has \r\n and \n\r used interchangebly.
*/
#define RX_NEWLINE 		"\r\n"

#define JOINED_RESPONSE "+EVT:JOINED\r\n"
#define JOIN_FAILED_RESPONSE "+EVT:JOIN FAILED\r\n"

/* Construct the cmds function defs */
char * construct_raw_cmd(struct command *cmd);
char * construct_get_cmd(struct command *cmd);
char * construct_set_cmd(struct command *cmd);
char * construct_send_cmd(struct command *cmd);
char * construct_join_cmd(struct command *cmd);
char * construct_context_restore_cmd(struct command *cmd);
char * construct_mac_param_cmd(struct command *cmd);
char * construct_delay_cmd(struct command *cmd);
char * construct_force_update_cmd(struct command *cmd);

/* Process the rx function defs */
enum cmd_res_code wait_for_ok(struct command *cmd);
enum cmd_res_code wait_for_timeout(struct command *cmd);
enum cmd_res_code wait_for_good_timeout(struct command *cmd);
enum cmd_res_code wait_for_joined_or_timeout(struct command *cmd);
enum cmd_res_code wait_for_ok_or_timeout(struct command *cmd);

/* Async response processors */
void async_recv(struct lrwanatd *lw, char *buf, size_t buflen);
void async_has_more_tx(struct lrwanatd *lw, char *buf, size_t buflen);

/* Local commands */
char * get_njm_cmd(struct command *cmd);
char * get_cfm_cmd(struct command *cmd);

char * set_njm_cmd(struct command *cmd);
char * set_cfm_cmd(struct command *cmd);


char *response[] = {
	"\r\nOK\r\n",
	"\r\nAT_PARAM_ERROR\r\n",
	"\r\nAT_ERROR\r\n",
	"\r\nAT_BUSY_ERROR\r\n",
	"\r\nAT_NO_NETWORK_JOINED\r\n"
};

char *delay_msg = "\r\n\r\n";

struct command_def cmd_def_list[] = {
	{
		.type = CMD_RESET,
		.group = CMD_ACTION,
		.token = TOKEN_AT_RESET,
		.token_len = sizeof(TOKEN_AT_RESET) - 1,
		.cmd = AT_CMD_RESET,
		.cmd_len = sizeof(AT_CMD_RESET) - 1,
		.construct_cmd = construct_raw_cmd,
		.process_cmd = wait_for_good_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_HARD_RESET,
		.group = CMD_ACTION,
		.token = TOKEN_AT_HARD_RESET,
		.token_len = sizeof(TOKEN_AT_HARD_RESET) - 1,
		.cmd = AT_CMD_RESET,
		.cmd_len = sizeof(AT_CMD_RESET) - 1,
		.construct_cmd = construct_raw_cmd,
		.process_cmd = wait_for_good_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_STATUS,
		.group = CMD_ACTION,
		.token = TOKEN_AT,
		.token_len = sizeof(TOKEN_AT) - 1,
		.cmd = AT_CMD,
		.cmd_len = sizeof(AT_CMD) - 1,
		.construct_cmd = construct_raw_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_JOIN,
		.group = CMD_ACTION,
		.token = TOKEN_AT_JOIN,
		.token_len = sizeof(TOKEN_AT_JOIN) - 1,
		.cmd = AT_CMD_JOIN,
		.cmd_len = sizeof(AT_CMD_JOIN) - 1,
		.construct_cmd = construct_join_cmd,
		.process_cmd = wait_for_joined_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_DEUI,
		.group = CMD_GET,
		.token = TOKEN_AT_DEUI,
		.token_len = sizeof(TOKEN_AT_DEUI) - 1,
		.cmd = AT_CMD_DEUI,
		.cmd_len = sizeof(AT_CMD_DEUI) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_DADDR,
		.group = CMD_GET,
		.token = TOKEN_AT_DADDR,
		.token_len = sizeof(TOKEN_AT_DADDR) - 1,
		.cmd = AT_CMD_DADDR,
		.cmd_len = sizeof(AT_CMD_DADDR) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_APPKEY,
		.group = CMD_GET,
		.token = TOKEN_AT_APPKEY,
		.token_len = sizeof(TOKEN_AT_APPKEY) - 1,
		.cmd = AT_CMD_APPKEY,
		.cmd_len = sizeof(AT_CMD_APPKEY) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_APPEUI,
		.group = CMD_GET,
		.token = TOKEN_AT_APPEUI,
		.token_len = sizeof(TOKEN_AT_APPEUI) - 1,
		.cmd = AT_CMD_APPEUI,
		.cmd_len = sizeof(AT_CMD_APPEUI) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_ADR,
		.group = CMD_GET,
		.token = TOKEN_AT_ADR,
		.token_len = sizeof(TOKEN_AT_ADR) - 1,
		.cmd = AT_CMD_ADR,
		.cmd_len = sizeof(AT_CMD_ADR) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_TXP,
		.group = CMD_GET,
		.token = TOKEN_AT_TXP,
		.token_len = sizeof(TOKEN_AT_TXP) - 1,
		.cmd = AT_CMD_TXP,
		.cmd_len = sizeof(AT_CMD_TXP) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_DR,
		.group = CMD_GET,
		.token = TOKEN_AT_DR,
		.token_len = sizeof(TOKEN_AT_DR) - 1,
		.cmd = AT_CMD_DR,
		.cmd_len = sizeof(AT_CMD_DR) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_RX2FQ,
		.group = CMD_GET,
		.token = TOKEN_AT_RX2FQ,
		.token_len = sizeof(TOKEN_AT_RX2FQ) - 1,
		.cmd = AT_CMD_RX2FQ,
		.cmd_len = sizeof(AT_CMD_RX2FQ) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_RX2DR,
		.group = CMD_GET,
		.token = TOKEN_AT_RX2DR,
		.token_len = sizeof(TOKEN_AT_RX2DR) - 1,
		.cmd = AT_CMD_RX2DR,
		.cmd_len = sizeof(AT_CMD_RX2DR) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_RX1DL,
		.group = CMD_GET,
		.token = TOKEN_AT_RX1DL,
		.token_len = sizeof(TOKEN_AT_RX1DL) - 1,
		.cmd = AT_CMD_RX1DL,
		.cmd_len = sizeof(AT_CMD_RX1DL) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_RX2DL,
		.group = CMD_GET,
		.token = TOKEN_AT_RX2DL,
		.token_len = sizeof(TOKEN_AT_RX2DL) - 1,
		.cmd = AT_CMD_RX2DL,
		.cmd_len = sizeof(AT_CMD_RX2DL) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_JN1DL,
		.group = CMD_GET,
		.token = TOKEN_AT_JN1DL,
		.token_len = sizeof(TOKEN_AT_JN1DL) - 1,
		.cmd = AT_CMD_JN1DL,
		.cmd_len = sizeof(AT_CMD_JN1DL) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_JN2DL,
		.group = CMD_GET,
		.token = TOKEN_AT_JN2DL,
		.token_len = sizeof(TOKEN_AT_JN2DL) - 1,
		.cmd = AT_CMD_JN2DL,
		.cmd_len = sizeof(AT_CMD_JN2DL) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_NJM,
		.group = CMD_GET,
		.token = TOKEN_AT_NJM,
		.token_len = sizeof(TOKEN_AT_NJM) - 1,
		.cmd = AT_CMD_NJM,
		.cmd_len = sizeof(AT_CMD_NJM) - 1,
		.construct_cmd = get_njm_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
	{
		.type = CMD_GET_NWKID,
		.group = CMD_GET,
		.token = TOKEN_AT_NWKID,
		.token_len = sizeof(TOKEN_AT_NWKID) - 1,
		.cmd = AT_CMD_NWKID,
		.cmd_len = sizeof(AT_CMD_NWKID) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_CLASS,
		.group = CMD_GET,
		.token = TOKEN_AT_CLASS,
		.token_len = sizeof(TOKEN_AT_CLASS) - 1,
		.cmd = AT_CMD_CLASS,
		.cmd_len = sizeof(AT_CMD_CLASS) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_NJS,
		.group = CMD_GET,
		.token = TOKEN_AT_NJS,
		.token_len = sizeof(TOKEN_AT_NJS) - 1,
		.cmd = AT_CMD_NJS,
		.cmd_len = sizeof(AT_CMD_NJS) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_CFM,
		.group = CMD_GET,
		.token = TOKEN_AT_CFM,
		.token_len = sizeof(TOKEN_AT_CFM) - 1,
		.cmd = AT_CMD_CFM,
		.cmd_len = sizeof(AT_CMD_CFM) - 1,
		.construct_cmd = get_cfm_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
	{
		.type = CMD_GET_CFS,
		.group = CMD_GET,
		.token = TOKEN_AT_CFS,
		.token_len = sizeof(TOKEN_AT_CFS) - 1,
		.cmd = AT_CMD_CFS,
		.cmd_len = sizeof(AT_CMD_CFS) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_SNR,
		.group = CMD_GET,
		.token = TOKEN_AT_SNR,
		.token_len = sizeof(TOKEN_AT_SNR) - 1,
		.cmd = AT_CMD_SNR,
		.cmd_len = sizeof(AT_CMD_SNR) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_RSSI,
		.group = CMD_GET,
		.token = TOKEN_AT_RSSI,
		.token_len = sizeof(TOKEN_AT_RSSI) - 1,
		.cmd = AT_CMD_RSSI,
		.cmd_len = sizeof(AT_CMD_RSSI) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_DADDR,
		.group = CMD_SET,
		.token = TOKEN_AT_DADDR,
		.token_len = sizeof(TOKEN_AT_DADDR) - 1,
		.cmd = AT_CMD_DADDR,
		.cmd_len = sizeof(AT_CMD_DADDR) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_APPKEY,
		.group = CMD_SET,
		.token = TOKEN_AT_APPKEY,
		.token_len = sizeof(TOKEN_AT_APPKEY) - 1,
		.cmd = AT_CMD_APPKEY,
		.cmd_len = sizeof(AT_CMD_APPKEY) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_NWKSKEY,
		.group = CMD_SET,
		.token = TOKEN_AT_NWKSKEY,
		.token_len = sizeof(TOKEN_AT_NWKSKEY) - 1,
		.cmd = AT_CMD_NWKSKEY,
		.cmd_len = sizeof(AT_CMD_NWKSKEY) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_APPSKEY,
		.group = CMD_SET,
		.token = TOKEN_AT_APPSKEY,
		.token_len = sizeof(TOKEN_AT_APPSKEY) - 1,
		.cmd = AT_CMD_APPSKEY,
		.cmd_len = sizeof(AT_CMD_APPSKEY) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_APPEUI,
		.group = CMD_SET,
		.token = TOKEN_AT_APPEUI,
		.token_len = sizeof(TOKEN_AT_APPEUI) - 1,
		.cmd = AT_CMD_APPEUI,
		.cmd_len = sizeof(AT_CMD_APPEUI) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_ADR,
		.group = CMD_SET,
		.token = TOKEN_AT_ADR,
		.token_len = sizeof(TOKEN_AT_ADR) - 1,
		.cmd = AT_CMD_ADR,
		.cmd_len = sizeof(AT_CMD_ADR) - 1,
		.construct_cmd = construct_mac_param_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
        .local_state = true,
	},
	{
		.type = CMD_SET_TXP,
		.group = CMD_SET,
		.token = TOKEN_AT_TXP,
		.token_len = sizeof(TOKEN_AT_TXP) - 1,
		.cmd = AT_CMD_TXP,
		.cmd_len = sizeof(AT_CMD_TXP) - 1,
		.construct_cmd = construct_mac_param_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
	{
		.type = CMD_SET_DR,
		.group = CMD_SET,
		.token = TOKEN_AT_DR,
		.token_len = sizeof(TOKEN_AT_DR) - 1,
		.cmd = AT_CMD_DR,
		.cmd_len = sizeof(AT_CMD_DR) - 1,
		.construct_cmd = construct_mac_param_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
	{
		.type = CMD_SET_RX2FQ,
		.group = CMD_SET,
		.token = TOKEN_AT_RX2FQ,
		.token_len = sizeof(TOKEN_AT_RX2FQ) - 1,
		.cmd = AT_CMD_RX2FQ,
		.cmd_len = sizeof(AT_CMD_RX2FQ) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_RX2DR,
		.group = CMD_SET,
		.token = TOKEN_AT_RX2DR,
		.token_len = sizeof(TOKEN_AT_RX2DR) - 1,
		.cmd = AT_CMD_RX2DR,
		.cmd_len = sizeof(AT_CMD_RX2DR) - 1,
		.construct_cmd = construct_mac_param_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
	{
		.type = CMD_SET_RX1DL,
		.group = CMD_SET,
		.token = TOKEN_AT_RX1DL,
		.token_len = sizeof(TOKEN_AT_RX1DL) - 1,
		.cmd = AT_CMD_RX1DL,
		.cmd_len = sizeof(AT_CMD_RX1DL) - 1,
		.construct_cmd = construct_mac_param_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
	{
		.type = CMD_SET_RX2DL,
		.group = CMD_SET,
		.token = TOKEN_AT_RX2DL,
		.token_len = sizeof(TOKEN_AT_RX2DL) - 1,
		.cmd = AT_CMD_RX2DL,
		.cmd_len = sizeof(AT_CMD_RX2DL) - 1,
		.construct_cmd = construct_mac_param_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
	{
		.type = CMD_SET_JN1DL,
		.group = CMD_SET,
		.token = TOKEN_AT_JN1DL,
		.token_len = sizeof(TOKEN_AT_JN1DL) - 1,
		.cmd = AT_CMD_JN1DL,
		.cmd_len = sizeof(AT_CMD_JN1DL) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_JN2DL,
		.group = CMD_SET,
		.token = TOKEN_AT_JN2DL,
		.token_len = sizeof(TOKEN_AT_JN2DL) - 1,
		.cmd = AT_CMD_JN2DL,
		.cmd_len = sizeof(AT_CMD_JN2DL) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_NJM,
		.group = CMD_SET,
		.token = TOKEN_AT_NJM,
		.token_len = sizeof(TOKEN_AT_NJM) - 1,
		.cmd = AT_CMD_NJM,
		.cmd_len = sizeof(AT_CMD_NJM) - 1,
		.construct_cmd = set_njm_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
	{
		.type = CMD_SET_NWKID,
		.group = CMD_SET,
		.token = TOKEN_AT_NWKID,
		.token_len = sizeof(TOKEN_AT_NWKID) - 1,
		.cmd = AT_CMD_NWKID,
		.cmd_len = sizeof(AT_CMD_NWKID) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_CLASS,
		.group = CMD_SET,
		.token = TOKEN_AT_CLASS,
		.token_len = sizeof(TOKEN_AT_CLASS) - 1,
		.cmd = AT_CMD_CLASS,
		.cmd_len = sizeof(AT_CMD_CLASS) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_CFM,
		.group = CMD_SET,
		.token = TOKEN_AT_CFM,
		.token_len = sizeof(TOKEN_AT_CFM) - 1,
		.cmd = AT_CMD_CFM,
		.cmd_len = sizeof(AT_CMD_CFM) - 1,
		.construct_cmd = set_cfm_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
	{
		.type = CMD_SET_FCNT,
		.group = CMD_SET,
		.token = TOKEN_AT_FCNT,
		.token_len = sizeof(TOKEN_AT_FCNT) - 1,
		.cmd = AT_CMD_FCNT,
		.cmd_len = sizeof(AT_CMD_FCNT) - 1,
		.construct_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SEND_TEXT,
		.group = CMD_SEND,
		.token = TOKEN_AT_SEND,
		.token_len = sizeof(TOKEN_AT_SEND) - 1,
		.cmd = AT_CMD_SEND,
		.cmd_len = sizeof(AT_CMD_SEND) - 1,
		.construct_cmd = construct_send_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SEND_BINARY,
		.group = CMD_SEND,
		.token = TOKEN_AT_SENDB,
		.token_len = sizeof(TOKEN_AT_SENDB) - 1,
		.cmd = AT_CMD_SENDB,
		.cmd_len = sizeof(AT_CMD_SENDB) - 1,
		.construct_cmd = construct_send_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_ASYNC_RECV,
		.group = CMD_ASYNC,
		.token = NULL,
		.token_len = 0,
		.cmd = NULL,
		.cmd_len = 0,
		.construct_cmd = NULL,
		.process_cmd = NULL,
		.async_cmd = async_recv,
	},
	{
		.type = CMD_ASYNC_MORE_TX,
		.group = CMD_ASYNC,
		.token = NULL,
		.token_len = 0,
		.cmd = NULL,
		.cmd_len = 0,
		.construct_cmd = NULL,
		.process_cmd = NULL,
		.async_cmd = async_has_more_tx,
	},
	{
		.type = CMD_ACQUIRE_CONTEXT,
		.group = CMD_INTERNAL,
		.token = TOKEN_AT_CTX_ACQ,
		.token_len = sizeof(TOKEN_AT_CTX_ACQ) - 1,
		.cmd = AT_CMD_CTX,
		.cmd_len = sizeof(AT_CMD_CTX) - 1,
		.construct_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_RESTORE_CONTEXT,
		.group = CMD_INTERNAL,
		.token = TOKEN_AT_CTX_RES,
		.token_len = sizeof(TOKEN_AT_CTX_RES) - 1,
		.cmd = AT_CMD,
		.cmd_len = sizeof(AT_CMD) - 1,
		.construct_cmd = construct_context_restore_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_DELAY,
		.group = CMD_INTERNAL,
		.token = TOKEN_AT_DELAY,
		.token_len = sizeof(TOKEN_AT_DELAY) - 1,
		.cmd = AT_CMD_DELAY,
		.cmd_len = sizeof(AT_CMD_DELAY) - 1,
		.construct_cmd = construct_delay_cmd,
		.process_cmd = wait_for_good_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
	{
		.type = CMD_FORCE_UPDATE,
		.group = CMD_INTERNAL,
		.token = TOKEN_AT_FORCE_UPDATE,
		.token_len = sizeof(TOKEN_AT_FORCE_UPDATE) - 1,
		.cmd = AT_CMD_FORCE_UPDATE,
		.cmd_len = sizeof(AT_CMD_FORCE_UPDATE) - 1,
		.construct_cmd = construct_force_update_cmd,
		.process_cmd = wait_for_good_timeout,
		.async_cmd = NULL,
		.local_state = true,
	},
};


time_t get_epoc_timeout(struct command *cmd)
{
	return cmd->timeout + time(NULL);
}

char *construct_raw_cmd(struct command *cmd)
{
	char *buf, *strptr;
	size_t buflen;

	buflen = cmd->def.cmd_len + 1;

	buf = malloc(buflen);

	strptr = buf;

	strncpy(strptr, cmd->def.cmd, cmd->def.cmd_len);
	strptr += cmd->def.cmd_len;

	*strptr = '\0';

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return buf;
}

char *construct_join_cmd(struct command *cmd)
{
	char *buf, *strptr;
	size_t buflen;

	buflen = cmd->def.cmd_len /* AT+JOIN */ + 2 /* =[0/1] */;

	buf = malloc(buflen + 1);

	sprintf(buf, "%.*s=%u", (int)cmd->def.cmd_len, cmd->def.cmd,
            global_lw->ctx_mngr.lwan_ctx->mac_params.network_join_mode);
	buf[buflen] = '\0';

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return buf;
}

char * construct_get_cmd(struct command *cmd)
{
	const char *postfix;
	char *buf, *strptr;
	size_t buflen, postfixlen;

	postfix = "=?";
	postfixlen = strlen(postfix);

	buflen = cmd->def.cmd_len + postfixlen + 1;

	buf = malloc(buflen);

	strptr = buf;

	strncpy(strptr, cmd->def.cmd, cmd->def.cmd_len);
	strptr += cmd->def.cmd_len;

	strncpy(strptr, postfix, postfixlen);
	strptr += postfixlen;

	*strptr = '\0';

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return buf;
}

char * construct_set_cmd(struct command *cmd)
{
	const char *eqstr;
	char *buf, *strptr;
	size_t buflen, eqstrlen;

	eqstr = "=";
	eqstrlen = strlen(eqstr);

	buflen = cmd->def.cmd_len + eqstrlen +
		cmd->param.set.param_len + 1;

	buf = malloc(buflen);

	strptr = buf;

	strncpy(strptr, cmd->def.cmd, cmd->def.cmd_len);
	strptr += cmd->def.cmd_len;

	strncpy(strptr, eqstr, eqstrlen);
	strptr += eqstrlen;

	strncpy(strptr, cmd->param.set.param, cmd->param.set.param_len);
	strptr += cmd->param.set.param_len;

	*strptr = '\0';

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return buf;
}

char * construct_send_cmd(struct command *cmd)
{
	char *buf;
	size_t buflen;
	/* AT+SEND=[port]:[confirmation_mode]:[data] */

	buflen = cmd->def.cmd_len /* AT+SEND/B */ +
		1 /* = */ +
		cmd->param.send.port_len /* [port] */+
		3 /* :[cfm]: */ +
		cmd->param.send.param_len /* [data] */;

	buf = malloc(buflen + 4);

	sprintf(buf, "%.*s=%.*s:%u:%.*s",
		(int)cmd->def.cmd_len, cmd->def.cmd,
		(int)cmd->param.send.port_len, cmd->param.send.port,
		global_lw->ctx_mngr.lwan_ctx->mac_params.confirmation_mode,
		(int)cmd->param.send.param_len + 1, cmd->param.send.param);
			   
	buf[buflen] = '\0';

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return buf;
}

char * construct_context_restore_cmd(struct command *cmd)
{
	char *buf;
	size_t buflen;
	int type;
	/* AT[+CTX=0:aabbcc] */

	type = cmd->param.internal.context_type;

	buflen = cmd->def.cmd_len /* AT */ +
			global_lw->ctx_mngr.lwan_ctx->ctx_len[type];

	buf = malloc(buflen + 1);
	sprintf(buf, "%.*s%.*s",
			(int)cmd->def.cmd_len, cmd->def.cmd,
			(int)global_lw->ctx_mngr.lwan_ctx->ctx_len[type],
			global_lw->ctx_mngr.lwan_ctx->ctx[type]);
	buf[buflen] = '\0';

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return buf;
}


/* These commands are not executed in the uart hardware, check local member of command */

char * get_njm_cmd(struct command *cmd)
{
	char *result;
	result = malloc(16);
	sprintf(result, "%u\r\nOK\r\n",
            global_lw->ctx_mngr.lwan_ctx->mac_params.network_join_mode);

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return result;
}

char * get_cfm_cmd(struct command *cmd)
{
	char *result;
	result = malloc(16);
	sprintf(result, "%u\r\nOK\r\n",
            global_lw->ctx_mngr.lwan_ctx->mac_params.confirmation_mode);

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return result;
}

char * get_mac_param_cmd(struct command *cmd)
{
	char *result;
	result = malloc(16);
	sprintf(result, "%u\r\nOK\r\n",
			global_lw->ctx_mngr.lwan_ctx->mac_params.network_join_mode);

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return result;
}

char * set_njm_cmd(struct command *cmd)
{
	char *result;
	char buf[16];
	uint8_t code = 0;

	strncpy(buf, cmd->param.set.param, cmd->param.set.param_len);
	buf[cmd->param.set.param_len] = '\0';

	code = strtol(cmd->param.set.param, NULL, 10);
	if (code < 0 || code > 1) {
		result = response[1];
	}
	else {
		global_lw->ctx_mngr.lwan_ctx->mac_params.network_join_mode = code;
		result = response[0];
	}

	log(LOG_INFO, "network join mode = %u",
        global_lw->ctx_mngr.lwan_ctx->mac_params.network_join_mode);

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return result;
}

char * set_cfm_cmd(struct command *cmd)
{
	char *result;
	char buf[16];
	uint8_t code = 0;

	strncpy(buf, cmd->param.set.param, cmd->param.set.param_len);
	buf[cmd->param.set.param_len] = '\0';


	code = strtol(cmd->param.set.param, NULL, 10);
	if (code < 0 || code > 1) {
        result = response[1];
	}
	else {
		global_lw->ctx_mngr.lwan_ctx->mac_params.confirmation_mode = code;
        result = response[0];
	}

	log(LOG_INFO, "confirmation mode = %u",
        global_lw->ctx_mngr.lwan_ctx->mac_params.confirmation_mode);

	cmd->epoc_timeout = get_epoc_timeout(cmd);

    return result;
}


char * construct_mac_param_cmd(struct command *cmd)
{

	char *result;
	struct lrwanatd *lw = global_lw;
	uint32_t code;

	/* If we have forced this to write to hardware uart, then call the uart code */
	if (!cmd->def.local_state) {
		return construct_set_cmd(cmd);
	}

	errno = 0;
	code = strtol(cmd->param.set.param, NULL, 10);

	if (errno != 0 ) {
		log(LOG_ERR, "%s", strerror(errno));
		result = response[1];
		return result;
	}

	switch (cmd->def.type) {
		case CMD_SET_DR:
			if (code < 0 || code > 7) {
				result = response[1];
			}
			else {
				lw->ctx_mngr.lwan_ctx->mac_params.dirty |= MAC_PARAM_BIT(MAC_PARAM_DATA_RATE);
				lw->ctx_mngr.lwan_ctx->mac_params.params[MAC_PARAM_DATA_RATE] = code;
				result = response[0];
				log(LOG_INFO, "Data Rate Set: %u", code);
			}
			break;
		case CMD_SET_TXP:
			if (code < 0 || code > 5) {
				result = response[1];
			}
			else {
				lw->ctx_mngr.lwan_ctx->mac_params.dirty |= MAC_PARAM_BIT(MAC_PARAM_TRANSMIT_POWER);
				lw->ctx_mngr.lwan_ctx->mac_params.params[MAC_PARAM_TRANSMIT_POWER] = code;
				result = response[0];
				log(LOG_INFO, "Transmit Power Set: %u", code);
			}
			break;
		case CMD_SET_RX1DL:
			lw->ctx_mngr.lwan_ctx->mac_params.dirty |= MAC_PARAM_BIT(MAC_PARAM_RX1_DELAY);
			lw->ctx_mngr.lwan_ctx->mac_params.params[MAC_PARAM_RX1_DELAY] = code;
			result = response[0];
			log(LOG_INFO, "Rx1 Delay Set: %u", code);
			break;
		case CMD_SET_RX2DL:
			lw->ctx_mngr.lwan_ctx->mac_params.dirty |= MAC_PARAM_BIT(MAC_PARAM_RX2_DELAY);
			lw->ctx_mngr.lwan_ctx->mac_params.params[MAC_PARAM_RX2_DELAY] = code;
			result = response[0];
			log(LOG_INFO, "Rx2 Delay Set: %u", code);
			break;
		case CMD_SET_RX2DR:
			if (code < 0 || code > 7) {
				result = response[1];
			}
			else {
				lw->ctx_mngr.lwan_ctx->mac_params.dirty |= MAC_PARAM_BIT(MAC_PARAM_RX2_DATA_RATE);
				lw->ctx_mngr.lwan_ctx->mac_params.params[MAC_PARAM_RX2_DATA_RATE] = code;
				result = response[0];
				log(LOG_INFO, "Rx2 Data Rate Set: %u", code);
			}
			break;
        case CMD_SET_ADR:
            if (code < 0 || code > 1) {
                result = response[1];
            }
            else {
				lw->ctx_mngr.lwan_ctx->mac_params.dirty |= MAC_PARAM_BIT(MAC_PARAM_ADR);
				lw->ctx_mngr.lwan_ctx->mac_params.params[MAC_PARAM_ADR] = code;
				result = response[0];
				log(LOG_INFO, "ADR Set: %u", code);
            }
            break;
	}

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return result;
}

char * construct_delay_cmd(struct command *cmd)
{
	/* wait_for_good_timeout */
	log(LOG_INFO, "Internal Delay  %u", cmd->timeout);

	cmd->epoc_timeout = get_epoc_timeout(cmd);

	return delay_msg;
}


char * construct_force_update_cmd(struct command *cmd)
{
    log(LOG_INFO, "Force Update");
    return response[0];
}

enum cmd_res_code wait_for_ok(struct command *cmd)
{
	size_t resplen;
	int i;

	resplen = sizeof(response)/sizeof(response[0]);

	for (i = 0; i < resplen; i++) {
		if (is_buffer_contains(cmd->buf, cmd->buf_len, response[i]))
			return CMD_RES_OK;
	}
	return CMD_RES_WAITING;
}

enum cmd_res_code wait_for_timeout(struct command *cmd)
{
	time_t now = time(NULL);
	if (cmd->epoc_timeout <= now) {
		log(LOG_INFO, "command %p timed out.", cmd);
		return CMD_RES_TIMEOUT;
	}
	return CMD_RES_WAITING;
}

enum cmd_res_code wait_for_good_timeout(struct command *cmd)
{
	/* sometimes a epoc_timeout is okay :D */
	time_t now = time(NULL);
	if (cmd->epoc_timeout <= now) {
		log(LOG_INFO, "Delay over.", cmd);
		return CMD_RES_OK;
	}
	return CMD_RES_WAITING;
}

enum cmd_res_code wait_for_ok_or_timeout(struct command *cmd)
{
	if (wait_for_ok(cmd) == CMD_RES_OK)
		return CMD_RES_OK;

	return wait_for_timeout(cmd);
}

enum cmd_res_code wait_for_joined_or_timeout(struct command *cmd)
{
	if (is_buffer_contains(cmd->buf, cmd->buf_len, JOINED_RESPONSE))
		return CMD_RES_OK;
	else if (is_buffer_contains(cmd->buf, cmd->buf_len, JOIN_FAILED_RESPONSE))
		return CMD_RES_TIMEOUT; // notify failure as epoc_timeout.

	return wait_for_timeout(cmd);
}

/* This structure will help in parsing, +EVT:[port]:[data]\r\n */
struct {
	size_t start;
	size_t end;
	bool found;
} async_rx_context;


void remove_buf_substr(struct lrwanatd *lw, size_t so, size_t eo)
{
	char _buf[8192];
	char *buf = lw->uart.buf;
	size_t buf_len = lw->uart.buf_len;
	lw->uart.buf[lw->uart.buf_len] = '\0';

	strncpy(_buf, buf, so);
	_buf[so] = '\0';
	strcat(_buf, buf + eo);

	strcpy(lw->uart.buf, _buf);
	lw->uart.buf_len = buf_len - (eo - so);
}


void async_recv(struct lrwanatd *lw, char *buf, size_t buflen)
{
	int ret;
	regmatch_t match[lw->regex.n_recv_grps];
	// use this buff as msg or error buff depenending on situtation.
	char msgbuf[255] = { '\0' };
    // log(LOG_INFO, "%.*s", buflen, buf);
	ret = regexec(&lw->regex.recv, buf, lw->regex.n_recv_grps, match, 0);

	if (!ret) {
		// got match
		// message format is port,payload,fcntdown,event,rssi,snr

		for (int i=1; i < lw->regex.n_recv_grps; i++) {
			int len = match[i].rm_eo - match[i].rm_so;
			strncat(msgbuf, buf + match[i].rm_so, len);
			if (i != lw->regex.n_recv_grps - 1)
				strcat(msgbuf, ",");
		}
		lw->push.cb->recv(lw, msgbuf, strlen(msgbuf));
		remove_buf_substr(lw, match[0].rm_so, match[0].rm_eo);

		context_manager_event(CMD_ASYNC_RECV, NULL);
	}
	else if (ret != REG_NOMATCH) {
		// error, because its neither 0 nor REG_NOMATCH
		regerror(ret, &lw->regex.recv, msgbuf, sizeof(msgbuf));
		log(LOG_ERR, "Recv regex match failed: %s\n", msgbuf);
	}
}

#if 0 // old receive
void async_recv(struct lrwanatd *lw, char *buf, size_t buflen)
{
	const char *start_with, *should_not_follow_with, *end_with;
	size_t len;
	start_with = "+EVT:";
	should_not_follow_with = "RX";
	end_with = "\r\n";

	/*	If not found, check for +EVT:
	*	If found, check for RX right after +EVT:. If RX present, set to not found
	*	If found, check for "\r\n" to register a positive hit.
	*/

	if (!_async_rx_context.found) {
		if(!cmp_last_few_chars(buf, buflen, start_with)) {
			_async_rx_context.found = true;
			_async_rx_context.start = buflen;
		}
	}
	else {
		len = _async_rx_context.start + strlen(should_not_follow_with);
		if(buflen == len &&
				!cmp_last_few_chars(buf, buflen, should_not_follow_with)){
			_async_rx_context.found = false;
		}
		else if (!cmp_last_few_chars(buf, buflen, end_with)) {
			_async_rx_context.end = buflen - strlen(end_with);

			/* Yay, a successful match, push it out */
			lw->push.cb->recv(lw, buf + _async_rx_context.start,
					_async_rx_context.end - _async_rx_context.start);
			_async_rx_context.found = false;
		}
	}
}
#endif

void async_has_more_tx(struct lrwanatd *lw, char *buf, size_t buflen)
{
	char  *str = "Network Server is asking for an uplink transmission\n\r";
	if (is_buffer_contains(buf, buflen, str)) {
		lw->push.cb->more_tx(lw, NULL, 0); /* Yay, a successful match, push it out */
		// clean it?
	}
}

struct cmd_queue_head *init_cmd_queue()
{
	struct cmd_queue_head *head = malloc(sizeof(struct cmd_queue_head));
	STAILQ_INIT(head);
	return head;
}

struct command *make_cmd(char *token, size_t token_len,
		union command_param *param,
		time_t timeout_in_sec,
		enum cmd_group group)
{
	struct command *cmd = NULL;
	struct command_def *def;
	size_t cmd_def_list_size;
	int i;

	cmd_def_list_size = sizeof(cmd_def_list)/sizeof(cmd_def_list[0]);

	for (i = 0; i < cmd_def_list_size; i++) {
		def = &cmd_def_list[i];
		if (group == def->group &&
				token_len == def->token_len &&
				!strncmp(token, def->token, token_len)) {
			cmd = calloc(1, sizeof(struct command));
			cmd->def = *def;
			cmd->buf_len = 0;
			cmd->state = CMD_NEW;
			if (param)
				cmd->param = *param;
#if 0
			if (timeout_in_sec)
				cmd->epoc_timeout = time(NULL) + timeout_in_sec;
			else /* use default epoc_timeout */
				cmd->epoc_timeout = time(NULL) + DEFAULT_TIMEOUT;
#endif
			if (timeout_in_sec)
				cmd->timeout = timeout_in_sec;
			else
				cmd->timeout = DEFAULT_TIMEOUT;

			break;
		}
	}
	return cmd;
}

void set_active_cmd_uart_buf(struct cmd_queue_head *cmdq_head, char *buf, size_t len)
{
	struct command *cmd;

	cmd = STAILQ_FIRST(cmdq_head);

	while(cmd && cmd->state != CMD_EXECUTING)
		cmd = STAILQ_NEXT(cmd, entries);

	if (cmd) {
		assert(cmd->state == CMD_EXECUTING);
		memcpy(cmd->buf + cmd->buf_len, buf, len);
		cmd->buf_len += len;
	}
}

void run_async_cmd(struct lrwanatd *lw, char *buf, size_t buflen)
{
	struct command *cmd = NULL;
	struct command_def *def;
	size_t cmd_def_list_size;
	int i;

	cmd_def_list_size = sizeof(cmd_def_list)/sizeof(cmd_def_list[0]);

	for (i = 0; i < cmd_def_list_size; i++) {
		def = &cmd_def_list[i];
		if (def->group != CMD_ASYNC)
			continue;

		if (def->async_cmd)
			def->async_cmd(lw, buf, buflen);
	}

}

void free_cmd_queue(struct cmd_queue_head *cmdq_head)
{
	struct command *cmd_next,
		*cmd = STAILQ_FIRST(cmdq_head);

	while (cmd != NULL) {
		cmd_next = STAILQ_NEXT(cmd, entries);
		free(cmd);
		cmd = cmd_next;
	}

	free(cmdq_head);
}

void clear_uart_buf(size_t *buflen)
{
	*buflen = 0;
	async_rx_context.found = false;
	async_rx_context.start = async_rx_context.end = 0;
}
