#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "json_parse.h"
#include "mqtt.h"
#include "video_doorbell.h"
#include "ring.h"

extern response_json_t response_json;
extern char publish_topic[48];
extern int talkback_run;
extern int answer_key_status;
extern long long hang_up_ts;
extern long long answer_ts;
int handle_cmd_callback(int cmd)
{
	switch(cmd) //平台在收到请求后的回复， 包含需要的信息
	{
		case HANG_UP_CALLBACK : 
			//todo 挂断
			printf("receive HANG_UP_CALLBACK\r\n");
			mqtt_publish_pre(UPDATE_RECORD_STATUS_TO_ANSWER);
			break;
		case CALL_SOS_CALLBACK :
			//todo : 用保存的信息拨号
			talkback_run = 1;
			break;
		case OPEN_DOOR_CALLBACK : 
			// todo : 开门
			printf("receive OPEN_DOOR_CALLBACK\r\n");
			break;
		case CALL_PROPERTY_CALLBACK : 
			// todo : 用保存的信息拨号
			talkback_run = 1;
			break;
		
		case GET_UPDATE_INFO_CALLBACK :
			// todo 用保存信息 ota
			
			break;
			
		case CHECK_COUNT_CHANNEL_USER_CALLBACK :
			//返回频道人数
			if(response_json.data.count == 1){
				hang_up_answer(1);
				talkback_run = 1;
				answer_key_status = 0;
				printf("CHECK_COUNT_CHANNEL_USER_CALLBACK num = %d\r\n", response_json.data.count);
			}
			else if(response_json.data.count == 0){
				printf("Answering failed (the calling end has hung up) !\r\n");
			}
			else if(response_json.data.count == 2){
				printf("Answered by other device !\r\n");
			}
			break;
			
		case UPDATE_RECORD_STATUS_TO_ANSWER_CALLBACK :
			printf("receive UPDATE_RECORD_STATUS_TO_ANSWER_CALLBACK\r\n");
			break;
			
		default :
			break;
	}
	return 0;
}

int handle_cmd(int cmd)
{
	switch(cmd)
	{
		case CALL:
			answer_key_status = 1;
			ring_play();
			printf("receive CALL answer_key_status %d\r\n", answer_key_status);
			//video_doorbell();
			break;

		case UPDATE:

			break;
		
		case HANG_UP:
			if(answer_key_status == 1 && hang_up_ts == 0 && answer_ts == 0){ 
				ring_stop();
			}
			hang_up_answer(0);
			vTaskDelay(200/ portTICK_PERIOD_MS);
			answer_key_status = 0;
			printf("receive HANG_UP answer_key_status %d\r\n", answer_key_status);
			
			if(hang_up_ts != 0 && answer_ts != 0){ //有进行通话 是通话后挂断 要上报
				mqtt_publish_pre(UPDATE_RECORD_STATUS_TO_ANSWER);
			}
			
			break;
		
		case SET_ALARM:

			break;
		
		case UPLOAD_LOG:

			break;
		
		case CELL_RADIO:

			break;
		
		case REMIND_LIFE:

			break;

		case FIRE_FIGHTING_EVENT:

			break;


	}
	return 0;
}


