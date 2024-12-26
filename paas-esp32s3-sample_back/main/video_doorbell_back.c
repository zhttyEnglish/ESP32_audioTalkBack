/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Agora Lab, Inc (http://www.agora.io/)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

#include "agora_rtc_api.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_peripherals.h"
#include "periph_touch.h"
#include "periph_adc_button.h"
#include "periph_button.h"
#include "audio_element.h"
#include "audio_hal.h"
#include "audio_mem.h"
#include "audio_sys.h"
#include "audio_thread.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"

#include "algorithm_stream.h"
#include "i2c_bus.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "board.h"
#include "board_pins_config.h"

//#include "gpio.h"

// #include "esp_pm.h"
// #include "input_key_service.h"
// #include "amf_ipcam.h"
// #include "license_activator_v2.h"

#define CONFIG_SEND_PCM_DATA
#define REAL_HARDWARE

#define DEFAULT_SDK_LOG_PATH "io.agora.rtc_sdk"
#define DEFAULT_AREA_CODE AREA_CODE_GLOB

#define DEFAULT_LISTEN_INTERVAL CONFIG_EXAMPLE_WIFI_LISTEN_INTERVAL

#if CONFIG_EXAMPLE_POWER_SAVE_MIN_MODEM
#define DEFAULT_PS_MODE WIFI_PS_MIN_MODEM
#elif CONFIG_EXAMPLE_POWER_SAVE_MAX_MODEM
#define DEFAULT_PS_MODE WIFI_PS_MAX_MODEM
#elif CONFIG_EXAMPLE_POWER_SAVE_NONE
#define DEFAULT_PS_MODE WIFI_PS_NONE
#else
#define DEFAULT_PS_MODE WIFI_PS_NONE
#endif

#if defined(CONFIG_USE_G722_CODEC)
#ifndef REAL_HARDWARE
#include "pcm_test_data_16k_5s.h"
#endif //#ifndef REAL_HARDWARE
#define AUDIO_CODEC_TYPE AUDIO_CODEC_TYPE_G722
#define CONFIG_PCM_SAMPLE_RATE (16000)
#define CONFIG_PCM_DATA_LEN  640

#elif defined(CONFIG_USE_G711U_CODEC)
#ifndef REAL_HARDWARE
#include "pcm_test_data_8k.h"
#endif //#ifndef REAL_HARDWARE
#define AUDIO_CODEC_TYPE AUDIO_CODEC_TYPE_G711A
#define CONFIG_PCM_SAMPLE_RATE (8000)
#define CONFIG_PCM_DATA_LEN  320

#elif defined(CONFIG_USE_OPUS_CODEC)
#define AUDIO_CODEC_TYPE AUDIO_CODEC_TYPE_OPUS
#define CONFIG_PCM_SAMPLE_RATE (16000)
#define CONFIG_PCM_DATA_LEN  640
#endif

#define CONFIG_PCM_CHANNEL_NUM (1)
#define CONFIG_AUDIO_FRAME_DURATION_MS                         \
  (CONFIG_PCM_DATA_LEN * 1000 / CONFIG_PCM_SAMPLE_RATE / CONFIG_PCM_CHANNEL_NUM / sizeof(int16_t))

#define CONFIG_SEND_FRAME_RATE   6

#define ESP_READ_BUFFER_SIZE 320

#define I2S_SAMPLE_RATE 8000

typedef struct {
  bool b_wifi_connected;
  bool b_call_session_started;
  bool b_exit;
} app_t;

static const char *TAG = "Agora";

static app_t g_app = {
  .b_call_session_started = false,
  .b_wifi_connected = false,
  .b_exit = false,
};

#define PRIO_TASK_FETCH (21)
#define ESP_RING_BUFFER_SIZE    CONFIG_PCM_DATA_LEN
#define DEFAULT_REF_DELAY_MS	0
#define _RECORD_HARDWARE_AEC	true

static audio_pipeline_handle_t recorder;
static audio_pipeline_handle_t player, key;

static audio_element_handle_t i2s_stream_reader, encoder, raw_read, element_algo;
static audio_element_handle_t i2s_stream_writer, decoder;
audio_event_iface_handle_t evt;
audio_board_handle_t board_handle;
int volume = 80;

static SemaphoreHandle_t g_audio_capture_sem  = NULL;
static audio_thread_t *g_audio_send_thread;
static audio_thread_t *g_key_thread;

ringbuf_handle_t recv_ringbuf = NULL; 
#if !_RECORD_HARDWARE_AEC
ringbuf_handle_t algo_ringbuf = NULL;
#endif

static connection_id_t g_conn_id;
static connection_info_t g_conn_info;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  printf( "wifi event %ld\n", event_id);
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    //g_app.b_wifi_connected = true;
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    g_app.b_wifi_connected = true;
    printf( "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
  }
}

/*init wifi as sta and set power save mode*/
static void setup_wifi(void)
{
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = "DP-AP",//CONFIG_WIFI_SSID,
      .password = "dp12345678",//CONFIG_WIFI_PASSWORD,
      .listen_interval = DEFAULT_LISTEN_INTERVAL,
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  printf( "esp_wifi_set_ps().");
#ifdef CONFIG_ENABLE_LOW_POWER_MODE
  esp_wifi_set_ps(DEFAULT_PS_MODE);
#else
  esp_wifi_set_ps(WIFI_PS_NONE);
#endif
}

static void setup_audio(void)
{
   board_handle = audio_board_init();
   //audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);
   audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
   audio_hal_enable_pa(board_handle->audio_hal, true);
   //printf("after pa enable ret = %d\r\n", ret);
   audio_hal_set_volume(board_handle->audio_hal, &volume);
   
}

static esp_err_t peripherals_key_open(void)
{
	ESP_LOGI(TAG, "Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "Initialize keys on board");
    audio_board_key_init(set);

    ESP_LOGI(TAG, " Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, " Listening event from all elements of pipeline");
    audio_pipeline_set_listener(player, evt);

    ESP_LOGI(TAG, " Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
	return 0;
}

static int i2s_write_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int bytes_write = 0;
#if !_RECORD_HARDWARE_AEC
    if (rb_write(algo_ringbuf, buf, len, wait_time) <= 0) {
        ESP_LOGW(TAG, "ringbuf write timeout");
    }
    algorithm_mono_fix((uint8_t *)buf, len);
#endif	
    bytes_write = audio_element_output(i2s_stream_writer, buf, len);
    if (bytes_write < 0) {
        ESP_LOGE(TAG, "i2s write failed");
    }
    return bytes_write;
}

static esp_err_t play_pipeline_open(void)
{
	audio_pipeline_cfg_t player_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	player = audio_pipeline_init(&player_pipeline_cfg);
	AUDIO_NULL_CHECK(TAG, player, return ESP_FAIL);

	recv_ringbuf = rb_create(4096, 1);
	
	i2s_stream_cfg_t i2s_cfg_write = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(I2S_NUM_0, CONFIG_PCM_SAMPLE_RATE, 
															16, AUDIO_STREAM_WRITER, I2S_SLOT_MODE_MONO);
    i2s_cfg_write.type = AUDIO_STREAM_WRITER;
	i2s_cfg_write.task_stack = -1;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg_write);

	audio_element_set_input_ringbuf(i2s_stream_writer, recv_ringbuf);
	audio_pipeline_register(player, i2s_stream_writer, "i2s_write");	
	const char *link_tag[1] = {"i2s_write"};
	audio_pipeline_link(player, &link_tag[0], 1);

	printf( "audio player has been created");
  	return ESP_OK;
}

static int i2s_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int bytes_read = audio_element_input(i2s_stream_reader, buf, len);
    if (bytes_read > 0) {
#if !_RECORD_HARDWARE_AEC		
        algorithm_mono_fix((uint8_t *)buf, bytes_read);
#endif
    } else {
        ESP_LOGE(TAG, "i2s read failed");
    }
    return bytes_read;
}

static esp_err_t recorder_pipeline_open(void)
{
	audio_pipeline_cfg_t record_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	recorder = audio_pipeline_init(&record_pipeline_cfg);
	AUDIO_NULL_CHECK(TAG, recorder, return ESP_FAIL);
	mem_assert(recorder);
	
	//es7210_adc_set_gain(ES7210_INPUT_MIC3, GAIN_0DB);
	// create i2s element handle
	i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, CONFIG_PCM_SAMPLE_RATE, 16, AUDIO_STREAM_READER);
	i2s_cfg_read.type = AUDIO_STREAM_READER;
	i2s_cfg_read.task_stack = -1;
	i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);

	// create algorithm element handle
	algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
#if !_RECORD_HARDWARE_AEC	
	algo_config.input_type = ALGORITHM_STREAM_INPUT_TYPE2;
#endif
	algo_config.sample_rate = CONFIG_PCM_SAMPLE_RATE;
	algo_config.out_rb_size = ESP_RING_BUFFER_SIZE;
	algo_config.debug_input = true;
	algo_config.swap_ch = true;
	element_algo = algo_stream_init(&algo_config);
	audio_element_set_music_info(element_algo, CONFIG_PCM_SAMPLE_RATE, 1, ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT);
	audio_element_set_read_cb(element_algo, i2s_read_cb, NULL);
	audio_element_set_input_timeout(element_algo, portMAX_DELAY);

	// create raw element handle
	raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
	raw_cfg.type        = AUDIO_STREAM_READER;
	raw_read = raw_stream_init(&raw_cfg);
	audio_element_set_output_timeout(raw_read, portMAX_DELAY);

	// codec -> algo -> encode -> send
	audio_pipeline_register(recorder, element_algo, "algo");
	audio_pipeline_register(recorder, raw_read, "raw");
	
	const char *link_tag[2] = {"algo", "raw"};
	audio_pipeline_link(recorder, &link_tag[0], 2);
	
#if !_RECORD_HARDWARE_AEC    
	printf("#defined RECORD_HARDWARE_AEC \r\n");
	algo_ringbuf = rb_create(2048, 1);
    audio_element_set_multi_input_ringbuf(element_algo, algo_ringbuf, 0);
    algo_stream_set_delay(element_algo, algo_ringbuf, DEFAULT_REF_DELAY_MS);
#endif

	printf( "audio recorder has been created");
	return ESP_OK;
}

static esp_err_t _pipeline_close(audio_pipeline_handle_t handle)
{
  audio_pipeline_stop(handle);
  audio_pipeline_wait_for_stop(handle);
  audio_pipeline_deinit(handle);

  return ESP_OK;
}

static void __on_join_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed)
{
  g_app.b_call_session_started = true;
  agora_rtc_get_connection_info(conn_id, &g_conn_info);
  printf("[conn-%lu] Join the channel %s successfully, uid %lu elapsed %d ms\n", conn_id, g_conn_info.channel_name, uid, elapsed);
}

static void __on_connection_lost(connection_id_t conn_id)
{
  g_app.b_call_session_started = false;
  printf("[conn-%lu] Lost connection from the channel\n", conn_id);
}

static void __on_rejoin_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
  g_app.b_call_session_started = true;
  printf("[conn-%lu] Rejoin the channel successfully, uid %lu elapsed %d ms\n", conn_id, uid, elapsed_ms);
}
static int userjoin = 0;
static int call_flag = 0;

static void __on_user_joined(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
	userjoin = 1;
  printf("[conn-%lu] Remote user \"%lu\" has joined the channel, elapsed %d ms\n", uid, conn_id, elapsed_ms);
}

static void __on_user_offline(connection_id_t conn_id, uint32_t uid, int reason)
{
	userjoin = 0;
  printf("[conn-%lu] Remote user \"%lu\" has left the channel, reason %d\n", conn_id, uid, reason);
}

static void __on_user_mute_audio(connection_id_t conn_id, uint32_t uid, bool muted)
{
  printf("[conn-%lu] audio: uid=%lu muted=%d\n", conn_id, uid, muted);
}

static void __on_error(connection_id_t conn_id, int code, const char *msg)
{
  if (code == ERR_VIDEO_SEND_OVER_BANDWIDTH_LIMIT) {
    printf("Not enough uplink bandwdith. Error msg \"%s\"\n", msg);
    return;
  }

  if (code == ERR_INVALID_APP_ID) {
    printf("Invalid App ID. Please double check. Error msg \"%s\"\n", msg);
  } else if (code == ERR_INVALID_CHANNEL_NAME) {
    printf("Invalid channel name. Please double check. Error msg \"%s\"\n", msg);
  } else if (code == ERR_INVALID_TOKEN || code == ERR_TOKEN_EXPIRED) {
    printf("Invalid token. Please double check. Error msg \"%s\"\n", msg);
  } else if (code == ERR_DYNAMIC_TOKEN_BUT_USE_STATIC_KEY) {
    printf("Dynamic token is enabled but is not provided. Error msg \"%s\"\n", msg);
  } else {
    printf("Error %d is captured. Error msg \"%s\"\n", code, msg);
  }
}

#ifdef CONFIG_ENABLE_AUDIO_MIXING
static void __on_mixed_audio_data(connection_id_t conn_id, const void *data, size_t len,
                                  const audio_frame_info_t *info_ptr)
{
  //LOG_I("[conn-%u] on_mixed_audio_data, data_type %d, len %zu\n", conn_id, info_ptr->data_type, len);
}
#else
static void __on_audio_data(connection_id_t conn_id, const uint32_t uid, uint16_t sent_ts, const void *data, size_t len,
                            const audio_frame_info_t *info_ptr)
{
  /*LOG_I("[conn-%u] on_audio_data, uid %u sent_ts %u data_type %d, len %zu\n", conn_id, uid, sent_ts,
         info_ptr->data_type, len);*/
if(userjoin == 1 && call_flag == 1){
	int ret = audio_element_output(i2s_stream_writer, (char *)data, len);
	//int ret = audio_element_output(decoder, (char *)data, len);
#if 1
	struct timeval tp;
	gettimeofday(&tp, NULL);

  	if(ret == len){
		printf("on_audio_data receive len = %d data_type %d  time = %ld\r\n", ret, info_ptr->data_type, tp.tv_usec / 1000);
	}else{
		printf("on_audio_data receive error ret = %d\r\n", ret);
	}

//	ret = rb_bytes_filled(recv_ringbuf);
//	printf("recv_ringbuf len = %d \r\n", ret);
#endif
}
	//printf("[conn-%lu] on_audio_data, ret %d data_type %d, len %zu\n", conn_id, ret, info_ptr->data_type, len);
}
#endif //#ifdef CONFIG_ENABLE_AUDIO_MIXING

#ifndef CONFIG_AUDIO_ONLY
static void __on_video_data(connection_id_t conn_id, const uint32_t uid, uint16_t sent_ts, const void *data, size_t len,
                            const video_frame_info_t *info_ptr)
{
  /*LOG_I("[conn-%u] on_video_data: uid %u sent_ts %u data_type %d frame_type %d stream_type %d len %zu\n", conn_id, uid,
         sent_ts, info_ptr->data_type, info_ptr->frame_type, info_ptr->stream_type, len);*/
}

static void __on_target_bitrate_changed(connection_id_t conn_id, uint32_t target_bps)
{
  printf("[conn-%lu] Bandwidth change detected. Please adjust encoder bitrate to %lu kbps\n", conn_id, target_bps / 1000);
}

static void __on_key_frame_gen_req(connection_id_t conn_id, uint32_t uid, video_stream_type_e stream_type)
{
  printf("[conn-%lu] Frame loss detected. Please notify the encoder to generate key frame immediately\n", conn_id);
}

static void __on_user_mute_video(connection_id_t conn_id, uint32_t uid, bool muted)
{
  printf("[conn-%lu] video: uid=%lu muted=%d\n", conn_id, uid, muted);
}
#endif //#ifndef CONFIG_AUDIO_ONLY

static int send_rtc_audio_frame(uint8_t *data, uint32_t len)
{
  // API: send audio data
  	audio_frame_info_t info = { 0 };
//	info.data_type = AUDIO_DATA_TYPE_OPUS;
  	info.data_type = AUDIO_DATA_TYPE_PCM;
	struct timeval tp;
	gettimeofday(&tp, NULL);
	
  int rval = agora_rtc_send_audio_data(g_conn_id, data, len, &info);
  if (rval < 0) {
    printf("Failed to send audio data, reason: %s\n", agora_rtc_err_2_str(rval));
    return -1;
  }
//  else{printf("agora_rtc_send_audio_data success len = %ld time = %ld\r\n", len, tp.tv_usec / 1000);}

  return 0;
}

static esp_err_t i2s_data_divided(int16_t *raw_buff, int len, int16_t *buf)
{
	int i;
	for (i = 0; i < len / 4; i++) {
		buf[i] = raw_buff[i * 2 + 1];
	}
	return ESP_OK;
}


static void key_thread(void *arg)
{
	//peripherals_key_open();

	while(1){
		audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            continue;
        }
		
		if ((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN)
            && (msg.cmd == PERIPH_TOUCH_TAP || msg.cmd == PERIPH_BUTTON_PRESSED || msg.cmd == PERIPH_ADC_BUTTON_PRESSED)) {
            if ((int) msg.data == get_input_sos_id()) {
				ESP_LOGI(TAG, "[ * ] [SOS][Vol-] touch tap event");
                volume -= 10;
                if (volume < 0) {
                    volume = 0;
                }
                audio_hal_set_volume(board_handle->audio_hal, volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", volume);
            } 
			else if ((int) msg.data == get_input_open_id()) {
                ESP_LOGI(TAG, "[ * ] [OPEN] touch tap event");
				ESP_LOGI(TAG, "===== open the door ===== ");
            }
			else if ((int) msg.data == get_input_reserve_id()){
				ESP_LOGI(TAG, "[ * ] [RESERVE][Vol+] touch tap event");
                volume += 10;
                if (volume > 100) {
                    volume = 100;
                }
                audio_hal_set_volume(board_handle->audio_hal, volume);
				ESP_LOGI(TAG, "[ * ] Volume set to %d %%", volume);
            }
			else if ((int) msg.data == get_input_call_id()) {
                ESP_LOGI(TAG, "[ * ] [CALL] touch tap event");
				call_flag++;
				if(call_flag == 2){
					call_flag = 0;
					ESP_LOGI(TAG, "===== hang up ===== ");
                }else if(call_flag == 1){
					ESP_LOGI(TAG, "===== answer the phone ===== ");
				}
            }
        }
		vTaskDelay(5 / portTICK_PERIOD_MS);
	}
	rb_destroy(recv_ringbuf);
	vTaskDelete(NULL);
}

static void audio_send_thread(void *arg)
{
  int ret = 0, i;

  int read_len = CONFIG_PCM_DATA_LEN * 2;
  uint8_t *raw_buf = heap_caps_malloc(read_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!raw_buf) {
    printf( "Failed to alloc audio buffer!");
    return;
  }

#if 1
  short *pcm_buf = heap_caps_malloc(CONFIG_PCM_DATA_LEN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!pcm_buf) {
    printf( "Failed to alloc audio buffer!");
    return;
  }
#endif
	memset(raw_buf, 0, CONFIG_PCM_DATA_LEN * 2);
  	memset(pcm_buf, 0, CONFIG_PCM_DATA_LEN);
  while (1) {
	printf("audio_send_thread start\r\n");

	recorder_pipeline_open();
  	play_pipeline_open();
	peripherals_key_open();
	
	audio_pipeline_run(player);
	audio_pipeline_run(recorder);
	
    while (g_app.b_call_session_started) {
		
		
		//AUDIO_MEM_SHOW(TAG);
		
		if(userjoin == 1 && call_flag == 1){
			ret = raw_stream_read(raw_read, (char *)raw_buf, read_len);
			if (ret != read_len) {
				printf( "raw read error, expect %d, but only %d\n", read_len, ret);
			}
			i2s_data_divided((int16_t *)raw_buf, read_len, (int16_t *)pcm_buf);
			
			send_rtc_audio_frame((uint8_t *)pcm_buf, CONFIG_PCM_DATA_LEN);
			//audio_sys_get_real_time_stats();
		}
		
	  	vTaskDelay(5 / portTICK_PERIOD_MS);
    }
	printf("_pipeline_close close\r\n");
    //deinit
    _pipeline_close(recorder); 
	_pipeline_close(player);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  free(raw_buf);
  free(pcm_buf);
#if !_RECORD_HARDWARE_AEC
  rb_destroy(algo_ringbuf);
#endif 
	rb_destroy(recv_ringbuf);

  vTaskDelete(NULL);
}

#if 0
#ifndef CONFIG_AUDIO_ONLY
#include "jpeg_test_data_640x480.h"
// static audio_thread_t *g_video_thread;

static int send_rtc_video_frame(uint8_t *data, uint32_t len)
{
  // API: send video data
    video_frame_info_t info = {
#ifdef CONFIG_SEND_JPEG_FRAMES
      .data_type    = VIDEO_DATA_TYPE_GENERIC_JPEG,
#else
      .data_type = VIDEO_DATA_TYPE_H264,
#endif
      .stream_type  = VIDEO_STREAM_HIGH,
      .frame_type   = VIDEO_FRAME_KEY,
      // .rotation     = VIDEO_ORIENTATION_0,
      .frame_rate   = CONFIG_SEND_FRAME_RATE
    };

  int rval = agora_rtc_send_video_data(g_conn_id, data, len, &info);
  if (rval < 0) {
    printf("Failed to send video data, reason: %s\n", agora_rtc_err_2_str(rval));
    return -1;
  }

  return 0;
}

static void video_send_thread(void *arg)
{
  int video_send_interval_ms = 1000 / CONFIG_SEND_FRAME_RATE;
  uint32_t frame_count = 0;

  int num_frames = sizeof(test_video_frames) / sizeof(test_video_frames[0]);
  while (1) {
    // LOG_I("video_send_thread pcm_offset = %d\r\n", buf_offset);
    int i = (frame_count++ % num_frames); // calculate frame index
    send_rtc_video_frame(test_video_frames[i].data, test_video_frames[i].len);
  
    // sleep and wait until time is up for next send
    usleep(video_send_interval_ms * 1000);
  }

  return;
}
#endif 
#endif

static void app_init_event_handler(agora_rtc_event_handler_t *event_handler)
{
  event_handler->on_join_channel_success = __on_join_channel_success;
  event_handler->on_connection_lost = __on_connection_lost;
  event_handler->on_rejoin_channel_success = __on_rejoin_channel_success;
  event_handler->on_user_joined = __on_user_joined;
  event_handler->on_user_offline = __on_user_offline;
  event_handler->on_user_mute_audio = __on_user_mute_audio;
#ifndef CONFIG_AUDIO_ONLY
  event_handler->on_user_mute_video = __on_user_mute_video;
  event_handler->on_target_bitrate_changed = __on_target_bitrate_changed;
  event_handler->on_key_frame_gen_req = __on_key_frame_gen_req;
  event_handler->on_video_data = __on_video_data;
#endif
  event_handler->on_error = __on_error;

#ifdef CONFIG_ENABLE_AUDIO_MIXING
  event_handler->on_mixed_audio_data = __on_mixed_audio_data;
#else
  event_handler->on_audio_data = __on_audio_data;
#endif
}


int app_main(void)
{
   int rval;
  //  char license_value[32] = { 0 };

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  setup_wifi();

  // Wait until WiFi is connected
  while (!g_app.b_wifi_connected) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  //license_activator_v2(CONFIG_AGORA_APP_ID, PID, CUSTOMER_KEY, CUSTOMER_SECRET, DEVICE_ID, license_value);
  setup_audio();

  g_audio_capture_sem = xSemaphoreCreateBinary();
  if (NULL == g_audio_capture_sem) {
    printf( "Unable to create audio capture semaphore!\n");
    return -1;
  }

#if 1
  // 2. API: init agora rtc sdk
  agora_rtc_event_handler_t event_handler = { 0 };
  app_init_event_handler(&event_handler);

  rtc_service_option_t service_opt = { 0 };
  service_opt.area_code            = AREA_CODE_GLOB;
  service_opt.log_cfg.log_disable  = true;
  service_opt.log_cfg.log_level    = RTC_LOG_DEBUG;
  service_opt.log_cfg.log_path     = DEFAULT_SDK_LOG_PATH;
  service_opt.license_value[0]     = '\0';
  service_opt.domain_limit         = false;

  rval = agora_rtc_init(CONFIG_AGORA_APP_ID, &event_handler, &service_opt);
  if (rval < 0) {
    printf("Failed to initialize Agora sdk, license %p, reason: %s\n", service_opt.license_value, agora_rtc_err_2_str(rval));
    return -1;
  }
  
  char * version = agora_rtc_get_version();
  printf("\r\n version:%s\r\n", version);
	
  // 3. API: Create connection
  rval = agora_rtc_create_connection(&g_conn_id);
  if (rval < 0) {
    printf("Failed to create connection, reason: %s\n", agora_rtc_err_2_str(rval));
    return -1;
  }
  
  // 4. API: join channel
  rtc_channel_options_t channel_options = { 0 };
  channel_options.auto_subscribe_audio = true;
  channel_options.auto_subscribe_video = false;
#ifdef CONFIG_SEND_PCM_DATA
  /* If we want to send PCM data instead of encoded audio like AAC or Opus, here please enable
   * audio codec, as well as configure the PCM sample rate and number of channels
   */
  channel_options.audio_codec_opt.audio_codec_type = AUDIO_CODEC_TYPE; 
  channel_options.audio_codec_opt.pcm_sample_rate  = CONFIG_PCM_SAMPLE_RATE; 
  channel_options.audio_codec_opt.pcm_channel_num  = CONFIG_PCM_CHANNEL_NUM;
  channel_options.audio_codec_opt.pcm_duration = CONFIG_AUDIO_FRAME_DURATION_MS;
  channel_options.audio_process_opt.enable_aec = false;
  channel_options.audio_process_opt.enable_audio_process = false;
  channel_options.audio_process_opt.enable_ns = false;
#endif
  printf("~~~~~rtc_channel_options_t success~~~~\r\n");

  rval = agora_rtc_join_channel(g_conn_id, DEFAULT_CHANNEL_NAME, DEFAULT_USER_ID, DEFAULT_TOKEN, &channel_options);
  if (rval < 0) {
    printf("Failed to join channel \"%s\", reason: %s\n", DEFAULT_CHANNEL_NAME, agora_rtc_err_2_str(rval));
    return -1;
  }

  while(!g_app.b_call_session_started) {
    usleep(200 * 1000);
  }
  printf("~~~~~agora_rtc_join_channel success~~~~\r\n");
#endif
#ifndef CONFIG_AUDIO_ONLY
  // rval = audio_thread_create(g_video_thread, "video_task", video_send_thread, NULL, 5 * 1024, PRIO_TASK_FETCH, true, 0);
  // if (rval != ESP_OK) {
  //   printf( "Unable to create video capture thread!\n");
  //   return -1;
  // }
#endif
#if 0
	rval = audio_thread_create(g_audio_play_thread, "play_task", audio_play_thread, NULL, 5 * 1024, PRIO_TASK_FETCH, true, 0);
   	if (rval != ESP_OK) {
     	printf( "Unable to create audio capture thread!\n");
     	return -1;
  	 }
#endif

   	rval = audio_thread_create(g_audio_send_thread, "send_task", audio_send_thread, NULL, 8 * 1024, PRIO_TASK_FETCH, true, 0);
   	if (rval != ESP_OK) {
     	printf( "Unable to create audio capture thread!\n");
     	return -1;
  	 }
	rval = audio_thread_create(g_key_thread, "key_task", key_thread, NULL, 2 * 1024, PRIO_TASK_FETCH, true, 0);
   	if (rval != ESP_OK) {
     	printf( "Unable to create audio capture thread!\n");
     	return -1;
  	 }
	
  while (!g_app.b_exit) {
    // audio_sys_get_real_time_stats();
    // printf( "MEM Total:%ld Bytes, Inter:%d Bytes, Dram:%d Bytes\n", esp_get_free_heap_size(),
    //           heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
    //           heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    sleep(10);
  }
  
  agora_rtc_leave_channel(g_conn_id);

  return 0;
}
