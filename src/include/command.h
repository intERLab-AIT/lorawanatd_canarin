/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#ifndef __COMMAND_H__
#define __COMMAND_H__
#include <sys/queue.h>
#include <time.h>
#include "lorawanatd.h"

/* Reset the lora board */
#define AT_CMD_RESET "ATZ"
#define TOKEN_AT_RESET "reset"

/* Get OK status */
#define AT_CMD "AT"
#define TOKEN_AT "status"

/* Get the device EUI */
#define AT_CMD_DEUI "AT+DEUI"
#define TOKEN_AT_DEUI "device_eui"

/* Device Address */
#define AT_CMD_DADDR "AT+DADDR"
#define TOKEN_AT_DADDR "device_address"

/* Application Key */
#define AT_CMD_APPKEY "AT+APPKEY"
#define TOKEN_AT_APPKEY "application_key"

/* Network Session Key */
#define AT_CMD_NWKSKEY "AT+NWKSKEY"
#define TOKEN_AT_NWKSKEY "network_session_key"

/* Application Session Key */
#define AT_CMD_APPSKEY "AT+APPSKEY"
#define TOKEN_AT_APPSKEY "application_session_key"

/* App EUI */
#define AT_CMD_APPEUI "AT+APPEUI"
#define TOKEN_AT_APPEUI "application_eui"

/* Adaptive Data Rate , 0: Disabled, 1: Enabled */
#define AT_CMD_ADR "AT+ADR"
#define TOKEN_AT_ADR "adaptive_data_rate"

/* Transmit Power , 0 - 5 */
#define AT_CMD_TXP "AT+TXP"
#define TOKEN_AT_TXP "transmit_power"

/* Data Rate , 0 - 7 */
#define AT_CMD_DR "AT+DR"
#define TOKEN_AT_DR "data_rate"

/* RX2 window frequency */
#define AT_CMD_RX2FQ "AT+RX2FQ"
#define TOKEN_AT_RX2FQ "rx2_frequency"

/* Rx2 window data rate, 0 - 7 */
#define AT_CMD_RX2DR "AT+RX2DR"
#define TOKEN_AT_RX2DR "rx2_data_rate"

/* Delay between the end of tx and rx window 1 in ms */
#define AT_CMD_RX1DL "AT+RX1DL"
#define TOKEN_AT_RX1DL "rx1_delay"

/* Delay between the end of tx and rx window 2 in ms */
#define AT_CMD_RX2DL "AT+RX2DL"
#define TOKEN_AT_RX2DL "rx2_delay"

/* Join Accept Delay between the end of tx and join rx window 1 in ms */
#define AT_CMD_JN1DL "AT+JN1DL"
#define TOKEN_AT_JN1DL "join1_delay"

/* Join Accept Delay between the end of tx and join rx window 2 in ms */
#define AT_CMD_JN2DL "AT+JN2DL"
#define TOKEN_AT_JN2DL "join2_delay"

/* Network Join Mode, 0: ABP, 1: OTA */
#define AT_CMD_NJM "AT+NJM"
#define TOKEN_AT_NJM "network_join_mode"

/* Network id */
#define AT_CMD_NWKID "AT+NWKID"
#define TOKEN_AT_NWKID "network_id"

/* Class of device */
#define AT_CMD_CLASS "AT+CLASS"
#define TOKEN_AT_CLASS "class"

/* Join */
#define AT_CMD_JOIN "AT+JOIN"
#define TOKEN_AT_JOIN "join"

/* Network Join Status, 0: Not Joined, 1: Joined */
#define AT_CMD_NJS "AT+NJS"
#define TOKEN_AT_NJS "network_join_status"

/* Send Hex */
#define AT_CMD_SENDB "AT+SENDB"
#define TOKEN_AT_SENDB "sendb"

/* Send Text */
#define AT_CMD_SEND "AT+SEND"
#define TOKEN_AT_SEND "send"

/* Confirmation Mode, 0: No confirmation, 1: Confirmation */
#define AT_CMD_CFM "AT+CFM"
#define TOKEN_AT_CFM "confirmation_mode"

/* Get confirmation status of last AT+SEND (0-1) */
#define AT_CMD_CFS "AT+CFS"
#define TOKEN_AT_CFS "confirmation_status"

/* SNR */
#define AT_CMD_SNR "AT+SNR"
#define TOKEN_AT_SNR "snr"

/* RSSI */
#define AT_CMD_RSSI "AT+RSSI"
#define TOKEN_AT_RSSI "rssi"

/* Frame Counters [up]:[down] */
#define AT_CMD_FCNT "AT+FCNT"
#define TOKEN_AT_FCNT "frame_counter"


enum cmd_group {
	CMD_ASYNC, /* Asynchronous events like RECV */
	CMD_ACTION, /* Single one shot commands like AT, ATZ and AT+JOIN */
	CMD_GET, /* Get value from Mcu */
	CMD_SET, /* Set value in Mcu */
	CMD_SEND /* Send over LoRaWAN */
};

enum cmd_type {
	CMD_RESET,
	CMD_STATUS,
	CMD_JOIN,
	CMD_GET_DEUI,
	CMD_GET_DADDR,
	CMD_GET_APPKEY,
	CMD_GET_APPEUI,
	CMD_GET_ADR,
	CMD_GET_TXP,
	CMD_GET_DR,
	CMD_GET_RX2FQ,
	CMD_GET_RX2DR,
	CMD_GET_RX1DL,
	CMD_GET_RX2DL,
	CMD_GET_JN1DL,
	CMD_GET_JN2DL,
	CMD_GET_NJM,
	CMD_GET_NWKID,
	CMD_GET_CLASS,
	CMD_GET_NJS,
	CMD_GET_CFM,
	CMD_GET_CFS,
	CMD_GET_SNR,
	CMD_GET_RSSI,
	CMD_SET_DADDR,
	CMD_SET_APPKEY,
	CMD_SET_NWKSKEY,
	CMD_SET_APPSKEY,
	CMD_SET_APPEUI,
	CMD_SET_ADR,
	CMD_SET_TXP,
	CMD_SET_DR,
	CMD_SET_RX2FQ,
	CMD_SET_RX2DR,
	CMD_SET_RX1DL,
	CMD_SET_RX2DL,
	CMD_SET_JN1DL,
	CMD_SET_JN2DL,
	CMD_SET_NJM,
	CMD_SET_NWKID,
	CMD_SET_CLASS,
	CMD_SET_CFM,
	CMD_SET_FCNT,
	CMD_SEND_TEXT,
	CMD_SEND_BINARY,
	CMD_ASYNC_RECV,
	CMD_ASYNC_MORE_TX,
	CMD_TYPE_MAX,
};

enum cmd_state {
	CMD_NEW,
	CMD_EXECUTING,
	CMD_EXECUTED,
	CMD_ERROR,
};

enum cmd_res_code {
	CMD_RES_IGNORE = -3,
	CMD_RES_TIMEOUT = -2,
	CMD_RES_WAITING = -1,
	CMD_RES_OK = 0,
};


struct command_def; /* command definition */
struct command;

typedef char * (*get_cmd_fp)(struct command *);
typedef enum cmd_res_code (*process_cmd_fp)(struct command *);
typedef void (*async_cmd_fp)(struct lrwanatd *lw, char *buf, size_t buflen);

STAILQ_HEAD(cmd_queue_head, command);

struct command_def {
	enum cmd_type type;
	enum cmd_group group;
	char *token;
	size_t token_len;
	char *cmd;
	size_t cmd_len;
	get_cmd_fp get_cmd;
	process_cmd_fp process_cmd;
	async_cmd_fp async_cmd;
};

struct command_param_set {
	time_t timeout;
	char *param;
	size_t param_len;
};

struct command_param_send {
	time_t timeout;
	char *param;
	size_t param_len;
	char *port;
	size_t port_len;
};

struct command_param_timeout {
	time_t timeout;
};

union command_param {
	struct command_param_set set;
	struct command_param_send send;
	struct command_param_timeout timeout;
};

struct command {
	STAILQ_ENTRY(command) entries;
	struct command_def def;
	union command_param param;
	char buf[1024];
	size_t buf_len;
	enum cmd_state state;
};


struct cmd_queue_head *init_cmd_queue();

struct command *make_cmd(char *token, size_t token_len,
		char *param, size_t param_len,
		char *port, size_t port_len,
		time_t timeout_in_sec,
		enum cmd_group group);

void run_async_cmd(struct lrwanatd *lw, char *buf, size_t buflen);

void set_active_cmd_uart_buf(struct cmd_queue_head *cmdq_head, char *buf, size_t len);

void clear_uart_buf(size_t *buflen);

void free_cmd_queue(struct cmd_queue_head *cmdq_head);

#endif
