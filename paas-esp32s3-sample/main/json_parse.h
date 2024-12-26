#include "json_def.h"


typedef enum{
	CALL = 0,
	UPDATE = 1,
	HANG_UP = 2,
	CALL_SOS = 3,
	OPEN_DOOR = 4,
	SET_ALARM = 5,
	UPLOAD_LOG = 6,
	CELL_RADIO = 7,
	DEVICE_INFO = 8,
	REMIND_LIFE = 9,
	CALL_PROPERTY = 10,
	GET_UPDATE_INFO = 11,
	FIRE_FIGHTING_EVENT = 12,
	CHECK_COUNT_CHANNEL_USER = 13,
	UPDATE_RECORD_STATUS_TO_ANSWER = 14,
}cmd_t;

typedef enum{
	CALL_CALLBACK = 0,
	UPDATE_CALLBACK = 1,
	HANG_UP_CALLBACK = 2,
	CALL_SOS_CALLBACK = 3,
	OPEN_DOOR_CALLBACK = 4,
	SET_ALARM_CALLBACK = 5,
	UPLOAD_LOG_CALLBACK = 6,
	CELL_RADIO_CALLBACK = 7,
	DEVICE_INFO_CALLBACK = 8,
	REMIND_LIFE_CALLBACK = 9,
	CALL_PROPERTY_CALLBACK = 10,
	GET_UPDATE_INFO_CALLBACK = 11,
	FIRE_FIGHTING_EVENT_CALLBACK = 12,
	CHECK_COUNT_CHANNEL_USER_CALLBACK = 13,
	UPDATE_RECORD_STATUS_TO_ANSWER_CALLBACK = 14,
}cmd_callback_t;


int build_request_json(cmd_t cmd, char * buf);

response_json_t response_json_parse_cmd(char * json);
response_json_t response_json_parse_data(char * json);
int get_cmd_callback_num(char * cmd);
int get_cmd_num(char * cmd);

int build_response_json(char    * buf);

void tcp_json_parse(char * json);
void show_response_data();

