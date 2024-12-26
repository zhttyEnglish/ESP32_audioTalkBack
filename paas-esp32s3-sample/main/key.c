/* Check operation of ADC button peripheral in board

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_log.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_adc_button.h"
#include "input_key_service.h"
#include "video_doorbell.h"
#include "mqtt.h"
#include "json_parse.h"
#include "ring.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "CHECK_BOARD_BUTTONS";

int g_call_flag = 0;

typedef enum{
	hangup = 0,
	answer = 1,
}hangup_or_answer;
	
int answer_key_status = 0;  // 如果 = 1 被呼叫 
extern int talkback_run;
extern char publish_topic[48];
extern long long hang_up_ts;
extern long long answer_ts;

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGD(TAG, "[ * ] input key id is %d, %d", (int)evt->data, evt->type);
    const char *key_types[INPUT_KEY_SERVICE_ACTION_PRESS_RELEASE + 1] = {"UNKNOWN", "CLICKED", "CLICK RELEASED", "PRESSED", "PRESS RELEASED"};
    switch ((int)evt->data) {
        case INPUT_KEY_USER_ID_REC:    //  sos key
	        if(evt->type == INPUT_KEY_SERVICE_ACTION_CLICK)
			{
	            ESP_LOGI(TAG, "[ * ] [SOS] KEY ");
				mqtt_publish_pre(CALL_SOS);
        	}
            break;
			
        case INPUT_KEY_USER_ID_SET:    //  喇叭形状 暂时为 挂断
        	if(evt->type == INPUT_KEY_SERVICE_ACTION_CLICK)
			{
	            ESP_LOGI(TAG, "[ * ] [HANG UP] KEY ");
				if(hang_up_ts == 0 && answer_ts == 0){
					ring_stop();
					vTaskDelay(100/ portTICK_PERIOD_MS);
				}
				
				hang_up_answer(hangup);
				vTaskDelay(200/ portTICK_PERIOD_MS);
				answer_key_status = 0;
				printf("answer_key_status %d\r\n", answer_key_status);
				
				mqtt_publish_pre(HANG_UP);
				
				if(hang_up_ts != 0 && answer_ts != 0){
					mqtt_publish_pre(UPDATE_RECORD_STATUS_TO_ANSWER);
				}
        	}
            break;
			
        case INPUT_KEY_USER_ID_MODE:   //  开门
        	if(evt->type == INPUT_KEY_SERVICE_ACTION_CLICK)
			{
            	ESP_LOGI(TAG, "[ * ] [OPEN] KEY ");
				mqtt_publish_pre(OPEN_DOOR);
        	}
            break;
			
        case INPUT_KEY_USER_ID_VOLUP:  //  接听 
			if(evt->type == INPUT_KEY_SERVICE_ACTION_CLICK)
			{
				ESP_LOGI(TAG, "[ * ] [ANSWER] KEY  TYPE %d", evt->type);
				if(answer_key_status == 1){
					ring_stop();
					vTaskDelay(100/ portTICK_PERIOD_MS);
					mqtt_publish_pre(CHECK_COUNT_CHANNEL_USER);
					//hang_up_answer(answer);
					//talkback_run = 1;
					//answer_key_status = 0;
					printf("answer_key_status %d talkback_run %d\r\n", answer_key_status, talkback_run);
				}else{
					mqtt_publish_pre(CALL_PROPERTY);
					hang_up_answer(answer);
				}
			}
            break;
			
        default:
            ESP_LOGE(TAG, "User Key ID[%d] does not support", (int)evt->data);
            break;
    }

    return ESP_OK;
}

void input_key_init(void)
{
    ESP_LOGI(TAG, "[ 1 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 2 ] Initialize Button peripheral with board init");
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[ 3 ] Create and start input key service");
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    input_cfg.based_cfg.task_stack = 4 * 1024;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);

    input_key_service_add_key(input_ser, input_key_info, 4);
    periph_service_set_callback(input_ser, input_key_service_cb, NULL);

    ESP_LOGW(TAG, "[ 4 ] Waiting for a button to be pressed ...");
	printf("publish topic %s\r\n", publish_topic);
}
