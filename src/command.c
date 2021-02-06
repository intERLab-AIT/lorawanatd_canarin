/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#include <malloc.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "command.h"
#include "logger.h"
#include "push.h"

#define DEFAULT_TIMEOUT 15
/* AT_SLAVE has \r\n and \n\r used interchangebly.
*/
#define RX_NEWLINE 		"\r\n"

#define JOINED_RESPONSE "JOINED\n\r"

/* Construct the cmds function defs */
char *construct_raw_cmd(struct command *cmd);
char * construct_get_cmd(struct command *cmd);
char * construct_set_cmd(struct command *cmd);
char * construct_send_cmd(struct command *cmd);

/* Process the rx function defs */
enum cmd_res_code wait_for_ok(struct command *cmd);
enum cmd_res_code wait_for_timeout(struct command *cmd);
enum cmd_res_code wait_for_joined_or_timeout(struct command *cmd);
enum cmd_res_code wait_for_ok_or_timeout(struct command *cmd);

/* Async response processors */
void async_recv(struct lrwanatd *lw, char *buf, size_t buflen);
void async_has_more_tx(struct lrwanatd *lw, char *buf, size_t buflen);


char *response[] = {
	"\r\nOK\r\n",
	"\r\nAT_PARAM_ERROR\r\n",
	"\r\nAT_ERROR\r\n"
};

struct command_def cmd_def_list[] = {
	{
		.type = CMD_RESET,
		.group = CMD_ACTION,
		.token = TOKEN_AT_RESET,
		.token_len = sizeof(TOKEN_AT_RESET) - 1,
		.cmd = AT_CMD_RESET,
		.cmd_len = sizeof(AT_CMD_RESET) - 1,
		.get_cmd = construct_raw_cmd,
		.process_cmd = wait_for_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_STATUS,
		.group = CMD_ACTION,
		.token = TOKEN_AT,
		.token_len = sizeof(TOKEN_AT) - 1,
		.cmd = AT_CMD,
		.cmd_len = sizeof(AT_CMD) - 1,
		.get_cmd = construct_raw_cmd,
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
		.get_cmd = construct_raw_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_NWKID,
		.group = CMD_GET,
		.token = TOKEN_AT_NWKID,
		.token_len = sizeof(TOKEN_AT_NWKID) - 1,
		.cmd = AT_CMD_NWKID,
		.cmd_len = sizeof(AT_CMD_NWKID) - 1,
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_GET_CFS,
		.group = CMD_GET,
		.token = TOKEN_AT_CFS,
		.token_len = sizeof(TOKEN_AT_CFS) - 1,
		.cmd = AT_CMD_CFS,
		.cmd_len = sizeof(AT_CMD_CFS) - 1,
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_get_cmd,
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
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_TXP,
		.group = CMD_SET,
		.token = TOKEN_AT_TXP,
		.token_len = sizeof(TOKEN_AT_TXP) - 1,
		.cmd = AT_CMD_TXP,
		.cmd_len = sizeof(AT_CMD_TXP) - 1,
		.get_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_DR,
		.group = CMD_SET,
		.token = TOKEN_AT_DR,
		.token_len = sizeof(TOKEN_AT_DR) - 1,
		.cmd = AT_CMD_DR,
		.cmd_len = sizeof(AT_CMD_DR) - 1,
		.get_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_RX2FQ,
		.group = CMD_SET,
		.token = TOKEN_AT_RX2FQ,
		.token_len = sizeof(TOKEN_AT_RX2FQ) - 1,
		.cmd = AT_CMD_RX2FQ,
		.cmd_len = sizeof(AT_CMD_RX2FQ) - 1,
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_RX1DL,
		.group = CMD_SET,
		.token = TOKEN_AT_RX1DL,
		.token_len = sizeof(TOKEN_AT_RX1DL) - 1,
		.cmd = AT_CMD_RX1DL,
		.cmd_len = sizeof(AT_CMD_RX1DL) - 1,
		.get_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_RX2DL,
		.group = CMD_SET,
		.token = TOKEN_AT_RX2DL,
		.token_len = sizeof(TOKEN_AT_RX2DL) - 1,
		.cmd = AT_CMD_RX2DL,
		.cmd_len = sizeof(AT_CMD_RX2DL) - 1,
		.get_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_JN1DL,
		.group = CMD_SET,
		.token = TOKEN_AT_JN1DL,
		.token_len = sizeof(TOKEN_AT_JN1DL) - 1,
		.cmd = AT_CMD_JN1DL,
		.cmd_len = sizeof(AT_CMD_JN1DL) - 1,
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_set_cmd,
		.process_cmd = wait_for_ok_or_timeout,
		.async_cmd = NULL,
	},
	{
		.type = CMD_SET_NWKID,
		.group = CMD_SET,
		.token = TOKEN_AT_NWKID,
		.token_len = sizeof(TOKEN_AT_NWKID) - 1,
		.cmd = AT_CMD_NWKID,
		.cmd_len = sizeof(AT_CMD_NWKID) - 1,
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_set_cmd,
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
		.get_cmd = construct_send_cmd,
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
		.get_cmd = construct_send_cmd,
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
		.get_cmd = NULL,
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
		.get_cmd = NULL,
		.process_cmd = NULL,
		.async_cmd = async_has_more_tx,
	}
};


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
	return buf;
}

char * construct_send_cmd(struct command *cmd)
{
	const char *eqstr, *sepstr;
	char *buf, *strptr;
	size_t buflen, eqstrlen, sepstrlen;

	eqstr = "=";
	eqstrlen = strlen(eqstr);
	sepstr = ":";
	sepstrlen = strlen(sepstr);

	buflen = cmd->def.cmd_len + eqstrlen +
		cmd->param.send.port_len + sepstrlen +
		cmd->param.send.param_len + 1;

	buf = malloc(buflen);

	strptr = buf;

	strncpy(strptr, cmd->def.cmd, cmd->def.cmd_len);
	strptr += cmd->def.cmd_len;

	strncpy(strptr, eqstr, eqstrlen);
	strptr += eqstrlen;

	strncpy(strptr, cmd->param.send.port, cmd->param.send.port_len);
	strptr += cmd->param.send.port_len;

	strncpy(strptr, sepstr, sepstrlen);
	strptr += sepstrlen;

	strncpy(strptr, cmd->param.send.param, cmd->param.send.param_len);
	strptr += cmd->param.send.param_len;

	*strptr = '\0';
	return buf;
}

int cmp_last_few_chars(char *buf, size_t buflen, const char *str)
{
	char *spos;
	size_t str_len;

	str_len = strlen(str);
	spos = buf + (buflen - str_len);
	return strncmp(spos, str, str_len);
}

enum cmd_res_code wait_for_ok(struct command *cmd)
{
	size_t resplen;
	int i;

	resplen = sizeof(response)/sizeof(response[0]);

	for (i = 0; i < resplen; i++) {
		if (!cmp_last_few_chars(cmd->buf, cmd->buf_len, response[i]))
			return CMD_RES_OK;
	}
	return CMD_RES_WAITING;
}

enum cmd_res_code wait_for_timeout(struct command *cmd)
{
	time_t now = time(NULL);
	if (cmd->param.timeout.timeout <= now) {
		log(LOG_INFO, "command %p timed out.", cmd);
		return CMD_RES_TIMEOUT;
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
	if (!cmp_last_few_chars(cmd->buf, cmd->buf_len, JOINED_RESPONSE))
		return CMD_RES_OK;

	return wait_for_timeout(cmd);
}

/* This structure will help in parsing, +EVT:[port]:[data]\r\n */
struct {
	size_t start;
	size_t end;
	bool found;
} _async_rx_context;

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

void async_has_more_tx(struct lrwanatd *lw, char *buf, size_t buflen)
{
	char  *str = "Network Server is asking for an uplink transmission\n\r";
	if (!cmp_last_few_chars(buf, buflen, str))
		lw->push.cb->more_tx(lw, NULL, 0); /* Yay, a successful match, push it out */
}

struct cmd_queue_head *init_cmd_queue()
{
	struct cmd_queue_head *head = malloc(sizeof(struct cmd_queue_head));
	STAILQ_INIT(head);
	return head;
}

struct command *make_cmd(char *token, size_t token_len,
		char *param, size_t param_len,
		char *port, size_t port_len,
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

			switch (group) {
				case CMD_SET:
					cmd->param.set.param = param;
					cmd->param.set.param_len = param_len;
					break;
				case CMD_SEND:
					cmd->param.send.param = param;
					cmd->param.send.param_len = param_len;
					cmd->param.send.port = port;
					cmd->param.send.port_len = port_len;
					break;
			};

			if (timeout_in_sec)
				cmd->param.timeout.timeout = time(NULL) + timeout_in_sec;
			else /* use default timeout */
				cmd->param.timeout.timeout = time(NULL) + DEFAULT_TIMEOUT;

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
	_async_rx_context.found = false;
	_async_rx_context.start = _async_rx_context.end = 0;
}
