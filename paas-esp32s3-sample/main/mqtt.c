/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "esp_log.h"

#include "mqtt_client.h"
#include "json_parse.h"
#include "tcp.h"
#include "sm4.h"
#include "handle_cmd.h"

#define MAIN_DEBUG
#ifdef MAIN_DEBUG
#define mainlog(format, ...) \
  			printf("[%s:%d] "format, __func__, __LINE__,  ##__VA_ARGS__)
#else
#define	mainlog(format, ...)
#endif

static const char *TAG = "mqtt_example";

esp_mqtt_event_handle_t event;
esp_mqtt_client_handle_t client;

//const char * receive_topic = "/j16an3a9svs/BA2401000001/user/get"; 
//const char * publish_topic = "/j16an3a9svs/BA2401000001/user/update";  

char receive_topic[48] = {0};
char publish_topic[48] = {0};

request_json_t request_json = {0};
response_json_t response_json = {0};
tcp_json_t tcp_json = {0};

static bool is_connected = false; 
extern char mac[16];

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

int StringToHex(char *str, unsigned char *out, int *outlen)
{
    char *p = str;
    char high = 0, low = 0;
    int tmplen = strlen(p), cnt = 0;
    tmplen = strlen(p);
    while(cnt < (tmplen / 2))
    {
        high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
        low = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
        out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
        p ++;
        cnt ++;
    }
    if(tmplen % 2 != 0) out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
 
    if(outlen != NULL) *outlen = tmplen / 2 + tmplen % 2;
    return tmplen / 2 + tmplen % 2;
}

/**
 * func  锛歨ex_to_char() 
 * desc  锛歵ransform hex to ascii 
 * input 锛歨ex 
 * return锛歛scii
 */
int hex_to_ascii(unsigned char * hex, int len, char * ascii)
{
	 
	for(int i = 0; i < len; i ++){
		 if((hex[i] >= 0x30) && (hex[i] <= 0x39))
	        ascii[i] = '0' + hex[i] - 0x30;
	    else if((hex[i] >= 0x41) && (hex[i] <= 0x5A)) // capital
	        ascii[i] = 'A' + (hex[i] - 0x41);
	    else if((hex[i] >= 0x61) && (hex[i] <= 0x7A)) // little case
	        ascii[i] = 'a' + (hex[i] - 0x61);
	    else 
	        ascii[i] = 0xff;
	} 
   
    return len;
}

void hex2str(uint8_t *input, uint16_t input_len, char *output)
{
    char *hexEncode = "0123456789ABCDEF";
    int i = 0, j = 0;

    for (i = 0; i < input_len; i++) {
        output[j++] = hexEncode[(input[i] >> 4) & 0xf];
        output[j++] = hexEncode[(input[i]) & 0xf];
    }
}


int  handle_mqtt_received_data(char * data, char * response_buf)
{
	response_json_parse_cmd(data);
	
	response_json_parse_data(data);

	int len = build_response_json(response_buf);

	return len;
}

int mqtt_packet_publish(esp_mqtt_client_handle_t client, char * topic, cmd_t cmd_type, char * sm4_key)
{
	char temp[512] = {0};
	char cmd[512] = {0};
	int len;
	
	//HANG_UP  CALL_SOS  OPEN_DOOR  DEVICE_INFO  CALL_PROPERTY  GET_UPDATE_INFO  CHECK_COUNT_CHANNEL_USER  UPDATE_RECORD_STATUS_TO_ANSWER
	len = build_request_json(cmd_type, cmd); 
	printf("build_request_json %s  %d\r\n", cmd, len);
//	printf("sm4_setkey_enc key %s\r\n", sm4_key);
	
	sm4_context ctx;
	sm4_setkey_enc(&ctx, (unsigned char *)sm4_key);
	int sm4len = sm4_crypt_ecb(&ctx, SM4_ENCRYPT, len, (unsigned char *)cmd, (unsigned char *)temp);
	
	memset(cmd, 0, 512);
	//printf("sm4_crypt_ecb len %d\r\n", sm4len);
	hex2str((unsigned char *)temp, sm4len, cmd);
	strcat(cmd, sm4_key);
//	printf("publish  %s\r\n", cmd);
	
	int msg_id = esp_mqtt_client_publish(client, topic, cmd, strlen(cmd), 1, 0);
//	printf("esp_mqtt_client_publish send %d\r\n", msg_id);
    ESP_LOGI(TAG, "sent publish publish_topic successful, msg_id=%d, len = %d", msg_id, strlen(cmd));
	return msg_id;
}


int mqtt_publish_pre(int cmd_type)
{
	printf("mqtt_publish_pre\r\n");
	mqtt_packet_publish(client, publish_topic, cmd_type, tcp_json.data.comm_sec_key);
	return 0;
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
 
char * p = NULL;
int total_len = 0;
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    event = event_data;
    client = event->client;
    int msg_id;
	
	
	//char * cmd = "{\"requestId\":\"441ded9c-9917-4313-870c-38a1795db031\",\"cmd\":\"CALL_PROPERTY\",\"data\":{\"type\":\"1\",\"channelType\":\"1\",\"mobile\":\"81111101\",\"agoraRtcTokenDTO\":{\"appId\":\"af3da0dbb65043b88ed91764753d2232\",\"token\":\"006af3da0dbb65043b88ed91764753d2232IADAXfZEoJ5efhM8S0rPsp167EYABIrKivgpwXZF49mg4xOyxFAAAAAAIgDjN42dmJbdZgQAAQBERtxmAgBERtxmAwBERtxmBABERtxm\",\"channelName\":\"p01542127121791239084502\"},\"rtcType\":\"voice\"}}";
	//char * cmd = "{\"cmd\":\"SET_ALARM\",\"data\":{\"setAlarm\":{\"date\":\"12:20\",\"mode\":\"0\",\"ringToneUrl\":"",\"id\":\"0\"}},\"device\":\"23DABA047761\",\"requestId\":\"441ded9c-9917-4313-870c-38a1795db031\"}";

	switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED  1");
		is_connected = true;
	
		msg_id = esp_mqtt_client_subscribe(client, publish_topic, 0);
        ESP_LOGI(TAG, "sent subscribe publish_topic successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, receive_topic, 0);
        ESP_LOGI(TAG, "sent subscribe receive_topic successful, msg_id=%d", msg_id);

		mqtt_packet_publish(client, publish_topic, DEVICE_INFO, tcp_json.data.comm_sec_key);

        break;
		
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED  2");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED  3, msg_id=%d", event->msg_id);
        break;
		
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED  4, msg_id=%d", event->msg_id);
        break;
	
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED  5 , msg_id=%d", event->msg_id);
        break;
	
    case MQTT_EVENT_DATA:    // 接收数据
        ESP_LOGI(TAG, "MQTT_EVENT_DATA  6");
		if(strncmp(event->topic, publish_topic, strlen(publish_topic)) == 0){
			printf("\r\n");
			break;
		}
		if(event->data != NULL )
		{
			total_len += event->data_len;
			if(event->total_data_len != total_len){
				printf("receive data len < total_data_len\r\n");
				
				p = (char *)malloc(event->total_data_len + 1);
				memset(p, 0, event->total_data_len);
				memcpy(p, event->data, event->data_len);
				break;
			}
			total_len = 0;
			char temp[600] = {0};
			char cmd[600] = {0};
			unsigned char key[16] = {0};
			int outlen;
			
			if(p != NULL)
			{
				strncat(p, event->data, event->data_len);
				strncpy((char *)key, p + strlen(p) - 16, 16);
				
				StringToHex(p, (unsigned char *)cmd, &outlen);
				free(p);
				p = NULL;
			}
			else{	
				strncpy((char *)key, event->data + (event->data_len - 16), 16);
				StringToHex(event->data, (unsigned char *)cmd, &outlen);
			}
			
			sm4_context ctx;
			sm4_setkey_dec(&ctx, key);
			sm4_crypt_ecb(&ctx, SM4_DECRYPT, outlen, (unsigned char *)cmd, (unsigned char *)temp);
			memset(cmd, 0, 600);
			
			printf("***** decrypt *****\r\n");
			mainlog("eventid %d receive temp : %s\r\n", event->event_id, temp);
			total_len = 0;
			
			int len = handle_mqtt_received_data(temp, cmd);
//			show_response_data();
			if(len > 0)// 大于0表示收到命令 		  	= -1 表示收到回复
			{  
				ESP_LOGI(TAG, "MQTT_EVENT_DATA receive cmd data ");
				printf("handle_received callback data %s\r\n", cmd);
				memset(temp, 0, 600);
				
				sm4_setkey_enc(&ctx, (unsigned char *)tcp_json.data.comm_sec_key);
				int sm4len = sm4_crypt_ecb(&ctx, SM4_ENCRYPT, len, (unsigned char *)cmd, (unsigned char *)temp);
				memset(cmd, 0, 600);
				
				hex2str((unsigned char *)temp, sm4len, cmd);
				strcat(cmd, tcp_json.data.comm_sec_key);
				
				vTaskDelay(100 / portTICK_PERIOD_MS);
				msg_id = esp_mqtt_client_publish(client, publish_topic, cmd, strlen(cmd), 1, 0);
				
				//printf("MQTT_EVENT_DATA sent %s\r\n", cmd);
				ESP_LOGI(TAG, "MQTT_EVENT_DATA  send cmd call back, msg_id=%d", event->msg_id);
				
				handle_cmd(get_cmd_num(response_json.cmd));
			}
			else if(len == -1)
			{
				ESP_LOGI(TAG, "MQTT_EVENT_DATA  receive call back data, msg_id=%d", event->msg_id);
				mainlog("receive call back data   client --> server --> client success\r\n\r\n");
				handle_cmd_callback(get_cmd_callback_num(response_json.cmd));
				//请求得到的数据保存在 request_json 里
			}
		}
		else{
			printf("receive call back data error\r\n\r\n");
		}
        break;
		
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR  7");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
		
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void get_hostname(char * url, char * host, char * port)
{
	char * host_end = NULL;
	char * port_start = NULL;

	host_end = strchr(url, ':');
	if(host_end != NULL){
		port_start = host_end + 1;
		strncpy(host, url, host_end - url);
		strcpy(port, port_start);
	}
}

void get_password(char    *   auth, char * password)
{
	char * name_end = NULL;
	char * password_start = NULL;

	name_end = strchr(auth, ':');
	if(name_end != NULL){
		password_start = name_end + 1;
		strcpy(password, password_start);
	}
	
}

static void mqtt_app_start(void)
{
	unsigned char * key = (unsigned char *)tcp_json.data.auth_sec_key;
	unsigned char broker[64] = {0};
	unsigned char auth[64] = {0};
	unsigned char url[32] = {0}, auth_str[64] = {0};
	int outlen = 0;
	
	StringToHex(tcp_json.data.broker, broker, &outlen);
	StringToHex(tcp_json.data.auth, auth, &outlen);
	
	sm4_context ctx;
	
	sm4_setkey_dec(&ctx, key);
	sm4_crypt_ecb(&ctx, SM4_DECRYPT, 32, broker, url);
	sm4_crypt_ecb(&ctx, SM4_DECRYPT, 32, auth, auth_str);

	printf("\r\n url  %s \r\n", url);
	
//	for(int i = 0; i < 32; i ++){
//		printf("%02x ", auth_str[i]);
//	}
	printf("auth_str  %s \r\n", auth_str);

	char host[16] = {0}; char port [16] = {0};
	char password [16] = {0};
	get_hostname((char *)url, host, port);
	get_password((char *)auth_str,password);
	
	printf("host: %s, port: %s, name: %s, password: %s\r\n", host, port, mac, password);
	
    esp_mqtt_client_config_t mqtt_cfg = {
		//.broker.address.uri = (char *)url,
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .broker.address.hostname = host,
        .broker.address.port = atoi(port),
        .credentials.username = mac,
        .credentials.client_id = mac,
        .credentials.authentication.password = password,
    };	
		
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	
    esp_mqtt_client_start(client);
}

void mqtt_init(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

	sprintf(publish_topic, "/j16an3a9svs/%s/user/update", mac);
	sprintf(receive_topic, "/j16an3a9svs/%s/user/get", mac);
	printf("receive topic  %s\tpublish topic  %s\r\n", receive_topic, publish_topic);
	
	tcp_connect_server(2, 1);   // type: 1 timing 2: auth 3 heartbeat  

    mqtt_app_start();
}
