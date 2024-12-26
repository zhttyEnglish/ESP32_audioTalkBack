#include <stdio.h>
#include <string.h>
//#include "json_def.h"
#include "json_parse.h"
#include "tcp.h"
#include "handle_cmd.h"

//#define FORMAT_DEBUG
#ifdef FORMAT_DEBUG
#define jsonlog(format, ...) \
  			printf("[%s:%d] "format, __func__, __LINE__,  ##__VA_ARGS__)
#else
#define	jsonlog(format, ...)
#endif

extern request_json_t request_json;
extern response_json_t response_json;
extern tcp_json_t tcp_json;

long long hang_up_ts = 0;
long long answer_ts = 0;
extern char mac[16];
/******************************* 构建请求�?*****************************************/
//{
//  char * cmd = "\"cmd\":\"CALL_PROPERTY\",
//  \"data\":{\"forcePropertySip\":1},\"device\":\"23DABA047761\",
//  \"requestId\":\"441ded9c-9917-4313-870c-38a1795db031\",\"version\":\"1\"}";

int build_request_json(cmd_t cmd, char *buf)
{
	int pos = 0;
	
	request_json.ts = get_timestamp_ms();
	sprintf(request_json.device, "%s", mac);
	sprintf(request_json.request_id, "%s", response_json.request_id);
	sprintf(request_json.version, "%s", "1");
	
	switch(cmd)
	{
		case HANG_UP:
			jsonlog("cmd hang up\r\n");
			pos += sprintf(&buf[pos], "{");
			pos += sprintf(&buf[pos], "\"cmd\":\"HANG_UP\",");
			pos += sprintf(&buf[pos], "\"data\":{");
			pos += sprintf(&buf[pos], "\"channelName\":\"%s\"", response_json.data.agora_rtc_token_dto.channel_name);
			pos += sprintf(&buf[pos], "},");
			pos += sprintf(&buf[pos], "\"device\":\"%s\",", request_json.device);
			pos += sprintf(&buf[pos], "\"requestId\":\"%s\",", request_json.request_id);
			pos += sprintf(&buf[pos], "\"ts\":\"%lld\",", request_json.ts);
			pos += sprintf(&buf[pos], "\"version\":\"%s\"", request_json.version);
			pos += sprintf(&buf[pos], "}");
			break;

		case CALL_SOS:
			jsonlog("cmd call sos\r\n");
			request_json.data.force_sip = 1;
			pos += sprintf(&buf[pos], "{");
			pos += sprintf(&buf[pos], "\"cmd\":\"CALL_SOS\",");
			pos += sprintf(&buf[pos], "\"data\":{");
			pos += sprintf(&buf[pos], "\"forceSip\":%d", request_json.data.force_sip);
			pos += sprintf(&buf[pos], "},");
			pos += sprintf(&buf[pos], "\"device\":\"%s\",", request_json.device);
			pos += sprintf(&buf[pos], "\"requestId\":\"%s\",", request_json.request_id);
			pos += sprintf(&buf[pos], "\"ts\":\"%lld\",", request_json.ts);
			pos += sprintf(&buf[pos], "\"version\":\"%s\"", request_json.version);
			pos += sprintf(&buf[pos], "}");
			break;
			
		case OPEN_DOOR:
			jsonlog("cmd open door\r\n");
			//sprintf(request_json.data.channel_name, "%s", "p01542127121791239084502");
			pos += sprintf(&buf[pos], "{");
			pos += sprintf(&buf[pos], "\"cmd\":\"OPEN_DOOR\",");
			pos += sprintf(&buf[pos], "\"data\":{");
			pos += sprintf(&buf[pos], "\"channelName\":\"%s\"", response_json.data.agora_rtc_token_dto.channel_name);
			pos += sprintf(&buf[pos], "},");
			pos += sprintf(&buf[pos], "\"device\":\"%s\",", request_json.device);
			pos += sprintf(&buf[pos], "\"requestId\":\"%s\",", request_json.request_id);
			pos += sprintf(&buf[pos], "\"ts\":\"%lld\",", request_json.ts);
			pos += sprintf(&buf[pos], "\"version\":\"%s\"", request_json.version);
			pos += sprintf(&buf[pos], "}");
			break;

		case DEVICE_INFO:
			jsonlog("cmd device info\r\n");
			sprintf(request_json.data.version, "%s", "1");
			pos += sprintf(&buf[pos], "{");
			pos += sprintf(&buf[pos], "\"cmd\":\"DEVICE_INFO\",");
			pos += sprintf(&buf[pos], "\"data\":{");
			pos += sprintf(&buf[pos], "\"version\":\"%s\"", request_json.data.version);
			pos += sprintf(&buf[pos], "},");
			pos += sprintf(&buf[pos], "\"device\":\"%s\",", request_json.device);
			pos += sprintf(&buf[pos], "\"requestId\":\"%s\",", request_json.request_id);
			pos += sprintf(&buf[pos], "\"ts\":\"%lld\",", request_json.ts);
			pos += sprintf(&buf[pos], "\"version\":\"%s\"", request_json.version);
			pos += sprintf(&buf[pos], "}");
			break;

		case CALL_PROPERTY:
			jsonlog("cmd call property\r\n");
			request_json.data.force_property_sip = 1;
			pos += sprintf(&buf[pos], "{");
			pos += sprintf(&buf[pos], "\"cmd\":\"CALL_PROPERTY\",");
			pos += sprintf(&buf[pos], "\"data\":{");
			pos += sprintf(&buf[pos], "\"forcePropertySip\":%d", request_json.data.force_property_sip);
			pos += sprintf(&buf[pos], "},");
			pos += sprintf(&buf[pos], "\"device\":\"%s\",", request_json.device);
			pos += sprintf(&buf[pos], "\"requestId\":\"%s\",", request_json.request_id);
			pos += sprintf(&buf[pos], "\"ts\":\"%lld\",", request_json.ts);
			pos += sprintf(&buf[pos], "\"version\":\"%s\"", request_json.version);
			pos += sprintf(&buf[pos], "}");
			break;

		case GET_UPDATE_INFO:
			jsonlog("cmd get updata info\r\n");
			pos += sprintf(&buf[pos], "{");
			pos += sprintf(&buf[pos], "\"cmd\":\"GET_UPDATE_INFO\",");
			pos += sprintf(&buf[pos], "\"data\":{");
			pos += sprintf(&buf[pos], "},");
			pos += sprintf(&buf[pos], "\"device\":\"%s\",", request_json.device);
			pos += sprintf(&buf[pos], "\"requestId\":\"%s\",", request_json.request_id);
			pos += sprintf(&buf[pos], "\"ts\":\"%lld\",", request_json.ts);
			pos += sprintf(&buf[pos], "\"version\":\"%s\"", request_json.version);
			pos += sprintf(&buf[pos], "}");
			break;

		case CHECK_COUNT_CHANNEL_USER:
			jsonlog("cmd check count channel user\r\n");
			//sprintf(request_json.data.channel_name, "%s", "p01542127121791239084502");
			pos += sprintf(&buf[pos], "{");
			pos += sprintf(&buf[pos], "\"cmd\":\"CHECK_COUNT_CHANNEL_USER\",");
			pos += sprintf(&buf[pos], "\"data\":{");
			pos += sprintf(&buf[pos], "\"channelName\":\"%s\"", response_json.data.agora_rtc_token_dto.channel_name);
			pos += sprintf(&buf[pos], "},");
			pos += sprintf(&buf[pos], "\"device\":\"%s\",", request_json.device);
			pos += sprintf(&buf[pos], "\"requestId\":\"%s\",", request_json.request_id);
			pos += sprintf(&buf[pos], "\"ts\":\"%lld\",", request_json.ts);
			pos += sprintf(&buf[pos], "\"version\":\"%s\"", request_json.version);
			pos += sprintf(&buf[pos], "}");
			break;

		case UPDATE_RECORD_STATUS_TO_ANSWER:
			//sprintf(request_json.data.channel_name, "%s", "p01542127121791239084502");
			request_json.data.call_duration = (hang_up_ts - answer_ts) / 1000;
			hang_up_ts = 0; answer_ts = 0;
			request_json.data.accout_type = 1;
			jsonlog("cmd updata record status to answer\r\n");
			pos += sprintf(&buf[pos], "{");
			pos += sprintf(&buf[pos], "\"cmd\":\"UPDATE_RECORD_STATUS_TO_ANSWER\",");
			pos += sprintf(&buf[pos], "\"data\":{");
			pos += sprintf(&buf[pos], "\"channelName\":\"%s\",", response_json.data.agora_rtc_token_dto.channel_name);
			pos += sprintf(&buf[pos], "\"callDuration\":%d,", request_json.data.call_duration);
			pos += sprintf(&buf[pos], "\"accountType\":%d", request_json.data.accout_type);
			pos += sprintf(&buf[pos], "},");
			pos += sprintf(&buf[pos], "\"device\":\"%s\",", request_json.device);
			pos += sprintf(&buf[pos], "\"requestId\":\"%s\",", request_json.request_id);
			pos += sprintf(&buf[pos], "\"ts\":\"%lld\",", request_json.ts);
			pos += sprintf(&buf[pos], "\"version\":\"%s\"", request_json.version);
			pos += sprintf(&buf[pos], "}");
			break;
		default :
			jsonlog("cmd default error");
			break;
	}
	jsonlog("========== json ==========\r\n");
	jsonlog("%s\r\n", buf);
	jsonlog("==========================\r\n");
	return pos;
}

/******************************* 解析应答�?*****************************************/
//  char * json_ = "{\"code\":0,\"message\":\"6K+35rGC5oiQ5Yqf\",
//  \"requestId\":\"441ded9c-9917-4313-870c-38a1795db031\",
//  \"cmd\":\"CALL_SOS_CALLBACK\",\"data\":{\"type\":\"1\",
//  \"channelType\":\"1\",\"mobile\":\"81111101\",
//  \"agoraRtcTokenDTO\":{\"appId\":\"af3da0dbb65043b88ed91764753d2232\",
//  \"token\":\"006af3da0dbb65043b88ed91764753d2232IADAXfZEoJ5efhM8S0rPsp167EYABIrKivgpwXZF49mg4xOyxFAAAAAAIgDjN42dmJbdZgQAAQBERtxmAgBERtxmAwBERtxmBABERtxm\",
//  \"channelName\":\"p01542127121791239084502\"},\"rtcType\": \"voice\"},
//  \"ts\":\"1726056845000\"}";


response_json_t response_json_parse_cmd(char * json)
{	
	char * code_Start = NULL;
    char * code_ValueStart = NULL;
//    char * code_ValueEnd = NULL;
	
	char * ts_Start = NULL;
    char * ts_ValueStart = NULL;
//    char * ts_ValueEnd = NULL;
	
	char * cmd_Start = NULL;
    char * cmd_ValueStart = NULL;
    char * cmd_ValueEnd = NULL;
	
	char * version_Start = NULL;
    char * version_ValueStart = NULL;
    char * version_ValueEnd = NULL;
	
	char * device_Start = NULL;
    char * device_ValueStart = NULL;
    char * device_ValueEnd = NULL;
	
	char * requestid_Start = NULL;
    char * requestid_ValueStart = NULL;
    char * requestid_ValueEnd = NULL;
	
	char * message_Start = NULL;
    char * message_ValueStart = NULL;
    char * message_ValueEnd = NULL;

	if(json != NULL)
	{
		code_Start = strstr(json, "\"code\":");
		if(code_Start != NULL){
			code_ValueStart = strchr(code_Start + strlen("\"code\""), ':');
//			code_ValueEnd = strchr(code_ValueStart + 1, ',');
			sscanf(code_ValueStart + 1, "%d", &response_json.code);
		}
		
		ts_Start = strstr(json, "\"ts\":");
		if(ts_Start != NULL){
		    ts_ValueStart = strchr(ts_Start + strlen("\"ts\""), ':');
//		    ts_ValueEnd = strchr(ts_ValueStart + 1, ',');
			sscanf(ts_ValueStart + 1, "%lld", &response_json.ts);
		}

		cmd_Start = strstr(json, "\"cmd\":");
		if(cmd_Start != NULL){
		    cmd_ValueStart = strchr(cmd_Start + strlen("\"cmd\":"), '\"');
		    cmd_ValueEnd = strchr(cmd_ValueStart + 1, '\"');
			memset(response_json.cmd, 0, strlen(response_json.cmd));
			strncpy(response_json.cmd, cmd_ValueStart + 1, cmd_ValueEnd - cmd_ValueStart - 1);
		}

		version_Start = strstr(json, "\"version\":");
		if(version_Start != NULL){
		    version_ValueStart = strchr(version_Start + strlen("\"version\":"), '\"');
		    version_ValueEnd = strchr(version_ValueStart + 1, '\"');
			memset(response_json.version, 0, strlen(response_json.version));
			strncpy(response_json.version, version_ValueStart + 1, version_ValueEnd - version_ValueStart - 1);
		}

		device_Start = strstr(json, "\"device\":");
		if(device_Start != NULL){
		    device_ValueStart = strchr(device_Start + strlen("\"device\":"), '\"');
		    device_ValueEnd = strchr(device_ValueStart + 1, '\"');
			memset(response_json.device, 0, strlen(response_json.device));
			strncpy(response_json.device, device_ValueStart + 1, device_ValueEnd - device_ValueStart - 1);
		}

		requestid_Start = strstr(json, "\"requestId\":");
		if(requestid_Start != NULL){
		    requestid_ValueStart = strchr(requestid_Start + strlen("\"requestId\":"), '\"');
		    requestid_ValueEnd = strchr(requestid_ValueStart + 1, '\"');
			memset(response_json.request_id, 0, strlen(response_json.request_id));
			strncpy(response_json.request_id, requestid_ValueStart + 1, requestid_ValueEnd - requestid_ValueStart - 1);
		}

		message_Start = strstr(json, "\"message\":");
		if(message_Start != NULL){
		    message_ValueStart = strchr(message_Start + strlen("\"message\":"), '\"');
		    message_ValueEnd = strchr(message_ValueStart + 1, '\"');
			memset(response_json.message, 0, strlen(response_json.message));
			strncpy(response_json.message, message_ValueStart + 1, message_ValueEnd - message_ValueStart - 1);
		}
	}
	return response_json;
}

response_json_t response_json_parse_data(char * json)
{
	char * type_Start = NULL;
	char * type_ValueStart = NULL;
//    char * type_ValueEnd = NULL;
	
	char * channel_type_Start = NULL;
	char * channel_type_ValueStart = NULL;
//    char * channel_type_ValueEnd = NULL;
	
	char * mobile_Start = NULL;
	char * mobile_ValueStart = NULL;
//    char * mobile_ValueEnd = NULL;
	
	char * count_Start = NULL;
	char * count_ValueStart = NULL;
//    char * count_ValueEnd = NULL;

	char * isopen_Start = NULL;
	char * isopen_ValueStart = NULL;
//    char * isopen_ValueEnd = NULL;

	char * interval_Start = NULL;
	char * interval_ValueStart = NULL;
//    char * interval_ValueEnd = NULL;

	char * upload_url_Start = NULL;
	char * upload_url_ValueStart = NULL;
    char * upload_url_ValueEnd = NULL;

	char * oss_key_Start = NULL;
	char * oss_key_ValueStart = NULL;
    char * oss_key_ValueEnd = NULL;

	char * content_Start = NULL;
	char * content_ValueStart = NULL;
    char * content_ValueEnd = NULL;
	
	char * rtc_type_Start = NULL;
	char * rtc_type_ValueStart = NULL;
    char * rtc_type_ValueEnd = NULL;
	
	char * version_name_Start = NULL;
	char * version_name_ValueStart = NULL;
    char * version_name_ValueEnd = NULL;
	
	char * newest_version_Start = NULL;
	char * newest_version_ValueStart = NULL;
    char * newest_version_ValueEnd = NULL;
	
	char * source_url_Start = NULL;
	char * source_url_ValueStart = NULL;
    char * source_url_ValueEnd = NULL;
	
	char * agora_Start = NULL;
	char * agora_ValueStart = NULL;
    char * agora_ValueEnd = NULL;
	
	char * app_id_Start = NULL;
	char * app_id_ValueStart = NULL;
    char * app_id_ValueEnd = NULL;
	
	char * token_Start = NULL;
	char * token_ValueStart = NULL;
    char * token_ValueEnd = NULL;

	char * license_Start = NULL;
	char * license_ValueStart = NULL;
    char * license_ValueEnd = NULL;
	
	char * channel_name_Start = NULL;
	char * channel_name_ValueStart = NULL;
    char * channel_name_ValueEnd = NULL;

	char * alarm_Start = NULL;
	char * alarm_ValueStart = NULL;
    char * alarm_ValueEnd = NULL;

	char * time_Start = NULL;
	char * time_ValueStart = NULL;
    char * time_ValueEnd = NULL;

	char * mode_Start = NULL;
	char * mode_ValueStart = NULL;
//    char * mode_ValueEnd = NULL;

	char * id_Start = NULL;
	char * id_ValueStart = NULL;
//    char * id_ValueEnd = NULL;

	char * ring_tone_url_Start = NULL;
	char * ring_tone_url_ValueStart = NULL;
    char * ring_tone_url_ValueEnd = NULL;

	char * data_Start = NULL;
    char * data_ValueStart = NULL;
    char * data_ValueEnd = NULL;
	char data[512] = {0};
#if 1
	data_Start = strstr(json, "\"data\":");
	if(data_Start != NULL){
	    data_ValueStart = strchr(data_Start + strlen("\"data\":"), '{');
	    data_ValueEnd = strrchr(data_ValueStart + 1, '}');
		strncpy(data, data_ValueStart + 1, data_ValueEnd - data_ValueStart - 1);
	}
	
	type_Start = strstr(data, "\"type\":");
	if(type_Start != NULL){
		type_ValueStart = strchr(type_Start + strlen("\"type\""), ':');
//		type_ValueEnd = strchr(type_ValueStart + 1, ',');
		sscanf(type_ValueStart + 1, "%d", &response_json.data.type);
	}
	
	channel_type_Start = strstr(data, "\"channelType\":");
	if(channel_type_Start != NULL){
		channel_type_ValueStart = strchr(channel_type_Start + strlen("\"channelType\""), ':');
//		channel_type_ValueEnd = strchr(channel_type_ValueStart + 1, ',');
		sscanf(channel_type_ValueStart + 1, "%d", &response_json.data.channel_type);
	}
	
	mobile_Start = strstr(data, "\"mobile\":");
	if(mobile_Start != NULL){
		mobile_ValueStart = strchr(mobile_Start + strlen("\"mobile\""), ':');
//		mobile_ValueEnd = strchr(mobile_ValueStart + 1, '\"');
		sscanf(mobile_ValueStart + 1, "%d", &response_json.data.mobile);
	}
	
	count_Start = strstr(data, "\"count\":");
	if(count_Start != NULL){
		count_ValueStart = strchr(count_Start + strlen("\"count\""), ':');
//		count_ValueEnd = strchr(count_ValueStart + 1, ',');
		sscanf(count_ValueStart + 1, "%d", &response_json.data.count);
	}

	isopen_Start = strstr(data, "\"is_open\":");
	if(isopen_Start != NULL){
		isopen_ValueStart = strchr(isopen_Start + strlen("\"is_open\""), ':');
//		isopen_ValueEnd = strchr(isopen_ValueStart + 1, ',');
		sscanf(isopen_ValueStart + 1, "%d", &response_json.data.is_open);
	}

	interval_Start = strstr(data, "\"interval\":");
	if(interval_Start != NULL){
		interval_ValueStart = strchr(interval_Start + strlen("\"interval\""), ':');
//		interval_ValueEnd = strchr(interval_ValueStart + 1, '\"');
		sscanf(interval_ValueStart + 1, "%d", &response_json.data.interval);
	}

	upload_url_Start = strstr(data, "\"upload_url\":");
	if(upload_url_Start != NULL){
		upload_url_ValueStart = strchr(upload_url_Start + strlen("\"upload_url\":"), '\"');
		upload_url_ValueEnd = strchr(upload_url_ValueStart + 1, '\"');
		memset(response_json.data.upload_url, 0, strlen(response_json.data.upload_url));
		strncpy(response_json.data.upload_url, upload_url_ValueStart + 1, upload_url_ValueEnd - upload_url_ValueStart - 1);
	}

	oss_key_Start = strstr(data, "\"oss_key\":");
	if(oss_key_Start != NULL){
		oss_key_ValueStart = strchr(oss_key_Start + strlen("\"oss_key\":"), '\"');
		oss_key_ValueEnd = strchr(oss_key_ValueStart + 1, '\"');
		memset(response_json.data.oss_key, 0, strlen(response_json.data.oss_key));
		strncpy(response_json.data.oss_key, oss_key_ValueStart + 1, oss_key_ValueEnd - oss_key_ValueStart - 1);
	}

	content_Start = strstr(data, "\"content\":");
	if(content_Start != NULL){
		content_ValueStart = strchr(content_Start + strlen("\"content\":"), '\"');
		content_ValueEnd = strchr(content_ValueStart + 1, '\"');
		memset(response_json.data.content, 0, strlen(response_json.data.content));
		strncpy(response_json.data.content, content_ValueStart + 1, content_ValueEnd - content_ValueStart - 1);
	}
	
	rtc_type_Start = strstr(data, "\"rtcType\":");
	if(rtc_type_Start != NULL){
		rtc_type_ValueStart = strchr(rtc_type_Start + strlen("\"rtcType\":"), '\"');
		rtc_type_ValueEnd = strchr(rtc_type_ValueStart + 1, '\"');
		memset(response_json.data.rtc_type, 0, strlen(response_json.data.rtc_type));
		strncpy(response_json.data.rtc_type, rtc_type_ValueStart + 1, rtc_type_ValueEnd - rtc_type_ValueStart - 1);
	}
	
	version_name_Start = strstr(data, "\"fileName\":");
	if(version_name_Start != NULL){
		version_name_ValueStart = strchr(version_name_Start + strlen("\"fileName\":"), '\"');
		version_name_ValueEnd = strchr(version_name_ValueStart + 1, '\"');
		memset(response_json.data.file_name, 0, strlen(response_json.data.file_name));
		strncpy(response_json.data.file_name, version_name_ValueStart + 1, version_name_ValueEnd - version_name_ValueStart - 1);
	}

	newest_version_Start = strstr(data, "\"Version\":");
	if(newest_version_Start != NULL){
		newest_version_ValueStart = strchr(newest_version_Start + strlen("\"Version\":"), '\"');
		newest_version_ValueEnd = strchr(newest_version_ValueStart + 1, '\"');
		memset(response_json.version, 0, strlen(response_json.version));
		strncpy(response_json.version, newest_version_ValueStart + 1, newest_version_ValueEnd - newest_version_ValueStart - 1);
	}

	source_url_Start = strstr(data, "\"sourceUrl\":");
	if(source_url_Start != NULL){
		source_url_ValueStart = strchr(source_url_Start + strlen("\"sourceUrl\":"), '\"');
		source_url_ValueEnd = strchr(source_url_ValueStart + 1, '\"');
		memset(response_json.data.source_url, 0, strlen(response_json.data.source_url));
		strncpy(response_json.data.source_url, source_url_ValueStart + 1, source_url_ValueEnd - source_url_ValueStart - 1);
	}
	
	agora_Start = strstr(data, "\"agoraRtcTokenDTO\":");
	if(agora_Start != NULL){
		agora_ValueStart = strchr(agora_Start + strlen("\"agoraRtcTokenDTO\":"), '{');
		agora_ValueEnd = strchr(agora_ValueStart + 1, '}');
		*agora_ValueEnd = '\0';

		app_id_Start = strstr(agora_ValueStart, "\"appId\":");
		app_id_ValueStart = strchr(app_id_Start + strlen("\"appId\":"), '\"');
		app_id_ValueEnd = strchr(app_id_ValueStart + 1, '\"');
//		printf("app_id_ValueStart %s\r\n", app_id_ValueStart);
//		printf("app_id_ValueEnd %s\r\n", app_id_ValueEnd);
		memset(response_json.data.agora_rtc_token_dto.app_id, 0, strlen(response_json.data.agora_rtc_token_dto.app_id));
		strncpy(response_json.data.agora_rtc_token_dto.app_id, app_id_ValueStart + 1, app_id_ValueEnd - app_id_ValueStart - 1);

		token_Start = strstr(agora_ValueStart, "\"token\":");
		token_ValueStart = strchr(token_Start + strlen("\"token\":"), '\"');
		token_ValueEnd = strchr(token_ValueStart + 1, '\"');
		memset(response_json.data.agora_rtc_token_dto.token, 0, strlen(response_json.data.agora_rtc_token_dto.token));
		strncpy(response_json.data.agora_rtc_token_dto.token, token_ValueStart + 1, token_ValueEnd - token_ValueStart - 1);

		channel_name_Start = strstr(agora_ValueStart, "\"channelName\":");
		channel_name_ValueStart = strchr(channel_name_Start + strlen("\"channelName\":"), '\"');
		channel_name_ValueEnd = strchr(channel_name_ValueStart + 1, '\"');
		memset(response_json.data.agora_rtc_token_dto.channel_name, 0, strlen(response_json.data.agora_rtc_token_dto.channel_name));
		strncpy(response_json.data.agora_rtc_token_dto.channel_name, channel_name_ValueStart + 1, channel_name_ValueEnd - channel_name_ValueStart - 1);

		license_Start = strstr(agora_ValueStart, "\"licenseValue\":");
		license_ValueStart = strchr(license_Start + strlen("\"licenseValue\":"), '\"');
		license_ValueEnd = strchr(license_ValueStart + 1, '\"');
		memset(response_json.data.agora_rtc_token_dto.license_value, 0, strlen(response_json.data.agora_rtc_token_dto.license_value));
		strncpy(response_json.data.agora_rtc_token_dto.license_value, license_ValueStart + 1, license_ValueEnd - license_ValueStart - 1);
	}

	alarm_Start = strstr(data, "\"setAlarm\":");
	if(alarm_Start != NULL){
		alarm_ValueStart = strchr(alarm_Start + strlen("\"setAlarm\":"), '{');
		alarm_ValueEnd = strchr(alarm_ValueStart + 1, '}');
		*alarm_ValueEnd = '\0';
		
		id_Start = strstr(alarm_ValueStart, "\"id\":");
		id_ValueStart = strchr(id_Start + strlen("\"id\""), ':');
//		id_ValueEnd = strchr(id_ValueStart + 1, ',');
		sscanf(id_ValueStart + 1, "%d", &response_json.data.alarm.id);

		mode_Start = strstr(alarm_ValueStart, "\"mode\":");
		mode_ValueStart = strchr(mode_Start + strlen("\"mode\""), ':');
//		mode_ValueEnd = strchr(mode_ValueStart + 1, ',');
		sscanf(mode_ValueStart + 1, "%d", &response_json.data.alarm.mode);

		time_Start = strstr(alarm_ValueStart, "\"date\":");
		time_ValueStart = strchr(time_Start + strlen("\"date\":"), '\"');
		time_ValueEnd = strchr(time_ValueStart + 1, '\"');
		memset(response_json.data.alarm.date, 0, strlen(response_json.data.alarm.date));
		strncpy(response_json.data.alarm.date, time_ValueStart + 1, time_ValueEnd - time_ValueStart - 1);

		ring_tone_url_Start = strstr(alarm_ValueStart, "\"ringToneUrl\":");
		ring_tone_url_ValueStart = strchr(ring_tone_url_Start + strlen("\"ringToneUrl\":"), '\"');
		ring_tone_url_ValueEnd = strchr(ring_tone_url_ValueStart + 1, '\"');
		memset(response_json.data.alarm.ring_tone_url, 0, strlen(response_json.data.alarm.ring_tone_url));
		strncpy(response_json.data.alarm.ring_tone_url, ring_tone_url_ValueStart + 1, ring_tone_url_ValueEnd - ring_tone_url_ValueStart - 1);
	
	}
#endif	
	return response_json;
}


/******************************* 构建应答�?***************************************/

int get_cmd_num(char * cmd)
{
	int ret = -1;
	if(strcmp(cmd, "CALL") == 0){
		ret = 0;
	}else if(strcmp(cmd, "UPDATE") == 0){
		ret = 1;
	}else if(strcmp(cmd, "HANG_UP") == 0){
		ret = 2;
	}else if(strcmp(cmd, "CALL_SOS") == 0){
		ret = 3;
	}else if(strcmp(cmd, "OPEN_DOOR") == 0){
		ret = 4;
	}else if(strcmp(cmd, "SET_ALARM") == 0){
		ret = 5;
	}else if(strcmp(cmd, "UPLOAD_LOG") == 0){
		ret = 6;
	}else if(strcmp(cmd, "CELL_RADIO") == 0){
		ret = 7;
	}else if(strcmp(cmd, "DEVICE_INFO") == 0){
		ret = 8;
	}else if(strcmp(cmd, "REMIND_LIFE") == 0){
		ret = 9;
	}else if(strcmp(cmd, "CALL_PROPERTY") == 0){
		ret = 10;
	}else if(strcmp(cmd, "GET_UPDATE_INFO") == 0){
		ret = 11;
	}else if(strcmp(cmd, "FIRE_FIGHTING_EVENT") == 0){
		ret = 12;
	}else if(strcmp(cmd, "CHECK_COUNT_CHANNEL_USER") == 0){
		ret = 13;
	}else if(strcmp(cmd, "UPDATE_RECORD_STATUS_TO_ANSWER") == 0){
		ret = 14;
	}

	return ret;
}

int get_cmd_callback_num(char * cmd)
{
	int ret = -1;
	if(strcmp(cmd, "CALL_CALLBACK") == 0){
		ret = 0;
	}else if(strcmp(cmd, "UPDATE_CALLBACK") == 0){
		ret = 1;
	}else if(strcmp(cmd, "HANG_UP_CALLBACK") == 0){
		ret = 2;
	}else if(strcmp(cmd, "CALL_SOS_CALLBACK") == 0){
		ret = 3;
	}else if(strcmp(cmd, "OPEN_DOOR_CALLBACK") == 0){
		ret = 4;
	}else if(strcmp(cmd, "SET_ALARM_CALLBACK") == 0){
		ret = 5;
	}else if(strcmp(cmd, "UPLOAD_LOG_CALLBACK") == 0){
		ret = 6;
	}else if(strcmp(cmd, "CELL_RADIO_CALLBACK") == 0){
		ret = 7;
	}else if(strcmp(cmd, "DEVICE_INFO_CALLBACK") == 0){
		ret = 8;
	}else if(strcmp(cmd, "REMIND_LIFE_CALLBACK") == 0){
		ret = 9;
	}else if(strcmp(cmd, "CALL_PROPERTY_CALLBACK") == 0){
		ret = 10;
	}else if(strcmp(cmd, "GET_UPDATE_INFO_CALLBACK") == 0){
		ret = 11;
	}else if(strcmp(cmd, "FIRE_FIGHTING_EVENT_CALLBACK") == 0){
		ret = 12;
	}else if(strcmp(cmd, "CHECK_COUNT_CHANNEL_USER_CALLBACK") == 0){
		ret = 13;
	}else if(strcmp(cmd, "UPDATE_RECORD_STATUS_TO_ANSWER_CALLBACK") == 0){
		ret = 14;
	}

	return ret;
}


int build_response_json(char *buf)
{
	int pos = 0;

	response_json.ts = get_timestamp_ms();
	
	pos += sprintf(&buf[pos], "{");
	pos += sprintf(&buf[pos], "\"code\":\"0\",");
	pos += sprintf(&buf[pos], "\"requestId\":\"%s\",",response_json.request_id);
	switch(get_cmd_num(response_json.cmd))   //平台向设备发命令�?设备处理和回�?
	{
		case CALL:
			pos += sprintf(&buf[pos], "\"cmd\":\"CALL_CALLBACK\",");
			break;

		case UPDATE:
			pos += sprintf(&buf[pos], "\"cmd\":\"UPDATE_CALLBACK\",");
			break;
		
		case HANG_UP:
			pos += sprintf(&buf[pos], "\"cmd\":\"HANG_UP_CALLBACK\",");
			break;
		
		case SET_ALARM:
			pos += sprintf(&buf[pos], "\"cmd\":\"SET_ALARM_CALLBACK\",");
			break;
		
		case UPLOAD_LOG:
			pos += sprintf(&buf[pos], "\"cmd\":\"UPLOAD_LOG_CALLBACK\",");
			break;
		
		case CELL_RADIO:
			pos += sprintf(&buf[pos], "\"cmd\":\"CELL_RADIO_CALLBACK\",");
			break;
		
		case REMIND_LIFE:
			pos += sprintf(&buf[pos], "\"cmd\":\"REMIND_LIFE_CALLBACK\",");
			break;

		case FIRE_FIGHTING_EVENT:
			pos += sprintf(&buf[pos], "\"cmd\":\"FIRE_FIGHTING_EVENT_CALLBACK\",");
			break;
		
		default:
			jsonlog("receive callback packet\r\n");
			goto EXIT;
			break;
	}
	
	pos += sprintf(&buf[pos], "\"data\":{");
	pos += sprintf(&buf[pos], "},");
	pos += sprintf(&buf[pos], "\"ts\":\"%lld\",",response_json.ts);
	pos += sprintf(&buf[pos], "\"version\":\"%s\"",response_json.version);
	pos += sprintf(&buf[pos], "}");	

	jsonlog("========== json =========\r\n");
	jsonlog("%s\r\n", buf);
	jsonlog("=========================\r\n");
	printf("build_response_json cmd = %s  pos = %d\r\n", response_json.cmd, pos);
	return pos;
EXIT:
	memset(buf, 0, strlen(buf));
	return -1;
}


void tcp_json_parse(char * json)
{
	jsonlog("strat tcp_json_parse\r\n");

	char * code_Start = NULL;
    char * code_ValueStart = NULL;
    char * code_ValueEnd = NULL; 
	
	char * msg_Start = NULL;
    char * msg_ValueStart = NULL;
    char * msg_ValueEnd = NULL;
	
	char * data_Start = NULL;
    char * data_ValueStart = NULL;
    char * data_ValueEnd = NULL;
	
	char * auth_Start = NULL;
    char * auth_ValueStart = NULL;
    char * auth_ValueEnd = NULL;
	
	char * broker_Start = NULL;
    char * broker_ValueStart = NULL;
    char * broker_ValueEnd = NULL;
	
	char * auth_sec_key_Start = NULL;
    char * auth_sec_key_ValueStart = NULL;
    char * auth_sec_key_ValueEnd = NULL;
	
	char * comm_sec_key_Start = NULL;
    char * comm_sec_key_ValueStart = NULL;
    char * comm_sec_key_ValueEnd = NULL;
	
	char * comm_server_code_Start = NULL;
    char * comm_server_code_ValueStart = NULL;
    char * comm_server_code_ValueEnd = NULL;
    
	
	data_Start = strstr(json, "\"data\":");
	if(data_Start != NULL){
	    data_ValueStart = strchr(data_Start + strlen("\"data\":"), '{');
	    data_ValueEnd = strrchr(data_ValueStart + 1, '}');
	}
	
	code_Start = strstr(json, "\"code\"");
	if(code_Start != NULL){
		code_ValueStart = strchr(code_Start + strlen("\"code\""), ':');
		code_ValueEnd = strchr(code_ValueStart + 1, '}');
		sscanf(code_ValueStart + 1, "%d", &tcp_json.code);
	}

	msg_Start = strstr(json, "\"message\":");
	if(msg_Start != NULL){
	    msg_ValueStart = strchr(msg_Start + strlen("\"message\":"), '\"');
	    msg_ValueEnd = strrchr(msg_ValueStart + 1, ',');
		strncpy(tcp_json.message, msg_ValueStart + 1, msg_ValueEnd - msg_ValueStart - 2);
	}

	if(data_ValueStart != NULL)
	{
		auth_Start = strstr(data_ValueStart, "\"auth\":");
		if(auth_Start != NULL){
		    auth_ValueStart = strchr(auth_Start + strlen("\"auth\":"), '\"');
		    auth_ValueEnd = strchr(auth_ValueStart + 1, '\"');
			strncpy(tcp_json.data.auth, auth_ValueStart + 1, auth_ValueEnd - auth_ValueStart - 1);
		}
		
		broker_Start = strstr(data_ValueStart, "\"broker\":");
		if(broker_Start != NULL){
		    broker_ValueStart = strchr(broker_Start + strlen("\"broker\":"), '\"');
		    broker_ValueEnd = strchr(broker_ValueStart + 1, '\"');
			strncpy(tcp_json.data.broker, broker_ValueStart + 1, broker_ValueEnd - broker_ValueStart - 1);
		}

		auth_sec_key_Start = strstr(data_ValueStart, "\"authSecKey\":");
		if(auth_sec_key_Start != NULL){
		    auth_sec_key_ValueStart = strchr(auth_sec_key_Start + strlen("\"authSecKey\":"), '\"');
		    auth_sec_key_ValueEnd = strchr(auth_sec_key_ValueStart + 1, '\"');
			strncpy(tcp_json.data.auth_sec_key, auth_sec_key_ValueStart + 1, auth_sec_key_ValueEnd - auth_sec_key_ValueStart - 1);
		}

		comm_sec_key_Start = strstr(data_ValueStart, "\"commSecKey\":");
		if(comm_sec_key_Start != NULL){
		    comm_sec_key_ValueStart = strchr(comm_sec_key_Start + strlen("\"commSecKey\":"), '\"');
		    comm_sec_key_ValueEnd = strchr(comm_sec_key_ValueStart + 1, '\"');
			strncpy(tcp_json.data.comm_sec_key, comm_sec_key_ValueStart + 1, comm_sec_key_ValueEnd - comm_sec_key_ValueStart - 1);
		}

		comm_server_code_Start = strstr(data_ValueStart, "\"commServerCode\"");
		if(comm_server_code_Start != NULL){
		    comm_server_code_ValueStart = strchr(comm_server_code_Start + strlen("\"commServerCode\""), ':');
		    comm_server_code_ValueEnd = strchr(comm_server_code_ValueStart + 1, '}');
			sscanf(comm_server_code_ValueStart + 1, "%d", &tcp_json.data.comm_server_code);
		}
	}
}
//  {"data":{"auth":"8a8a73f204bffe317ace05df044b53e6669f568bac257245c31ba6baf4557147",
//  "broker":"cb40c1b93f11465e76f7cc1aee391ef029404e01578bcaf842b86dd22d59f2e3",
//  "authSecKey":"rjElFVxk5kjQlyzu","commSecKey":"iakd3S5iLMl3U0jw","commServerCode":2},"message":"success","code":0}


void show_response_data()
{
	printf("=======================================================================================================================\r\n");
	printf("code %d\t ts %lld\t version %s\t message %s\t cmd %s\r\n", response_json.code, response_json.ts, response_json.version,response_json.message, response_json.cmd);
	printf("device %s\t requestid %s\r\n", response_json.device, response_json.request_id);
	printf("data:{\r\n");
	printf("\t type %d\t channeltype %d\t mobile %d\r\n", response_json.data.type, response_json.data.channel_type, response_json.data.mobile);
	printf("\t count %d\t isopen %d\t interval %d\r\n", response_json.data.count, response_json.data.is_open, response_json.data.interval);
	printf("\t uploadurl %s\t osskey %s\r\n", response_json.data.upload_url, response_json.data.oss_key);
	printf("\t content %s\t rtctype %s\t versionname %s\r\n", response_json.data.content, response_json.data.rtc_type, response_json.data.file_name);
	printf("\t newversion %s\t sourceurl %s\r\n", response_json.version, response_json.data.source_url);
	printf("\t agora {\r\n");
	printf("\t \t appid %s\r\n\t \t channelname %s\r\n", response_json.data.agora_rtc_token_dto.app_id, response_json.data.agora_rtc_token_dto.channel_name);
	printf("\t \t token %s\r\n\t \t license_value %s\r\n}", response_json.data.agora_rtc_token_dto.token, response_json.data.agora_rtc_token_dto.license_value);
	printf("\t alarm {\r\n");
	printf("\t \t id %d\t mode %d\t date %s\t ringurl %s\r\n}", response_json.data.alarm.id, response_json.data.alarm.mode, response_json.data.alarm.date, response_json.data.alarm.ring_tone_url);
	printf("=======================================================================================================================\r\n");
}
