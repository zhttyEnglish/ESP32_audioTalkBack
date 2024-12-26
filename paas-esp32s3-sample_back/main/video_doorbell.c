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
#include "opus_decoder.h"
#include "opus_encoder.h"

#define AUDIOLOG_DEBUG
#ifdef AUDIOLOG_DEBUG
#define audiolog(format, ...) \
  			printf("[%s:%d] "format, __func__, __LINE__,  ##__VA_ARGS__)
#else
#define	audiolog(format, ...)
#endif


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

#define AUDIO_CODEC_TYPE AUDIO_CODEC_DISABLED
#define CONFIG_PCM_SAMPLE_RATE (16000)
#define CONFIG_PCM_DATA_LEN  640

#define CONFIG_PCM_CHANNEL_NUM (1)
#define CONFIG_AUDIO_FRAME_DURATION_MS                         \
  (CONFIG_PCM_DATA_LEN * 1000 / CONFIG_PCM_SAMPLE_RATE / CONFIG_PCM_CHANNEL_NUM / sizeof(int16_t))

#define CONFIG_SEND_FRAME_RATE   6

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

static audio_pipeline_handle_t recorder;
static audio_pipeline_handle_t player, key;

static audio_element_handle_t i2s_stream_reader, encoder, raw_read_el, element_algo;
static audio_element_handle_t i2s_stream_writer, decoder, raw_write_el;
audio_event_iface_handle_t evt;
audio_board_handle_t board_handle;
int volume = 30;

static SemaphoreHandle_t g_audio_capture_sem  = NULL;
static audio_thread_t *g_audio_send_thread;
static audio_thread_t *g_key_thread;

ringbuf_handle_t recv_ringbuf = NULL; 

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
   audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
   audio_hal_enable_pa(board_handle->audio_hal, true);
   audio_hal_set_volume(board_handle->audio_hal, volume);
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
    int bytes_write = audio_element_output(i2s_stream_writer, buf, len);
    if (bytes_write < 0) {
        printf("i2s_write_cb failed\r\n");
    }
    return bytes_write;
}

static esp_err_t play_pipeline_open(void)
{
	audio_pipeline_cfg_t player_pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	player = audio_pipeline_init(&player_pipeline_cfg);
	AUDIO_NULL_CHECK(TAG, player, return ESP_FAIL);
	
//	recv_ringbuf = rb_create(2048, 1);
	
	i2s_stream_cfg_t i2s_cfg_write = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(I2S_NUM_0, 16000, 
															16, AUDIO_STREAM_WRITER, I2S_SLOT_MODE_MONO);
//	i2s_stream_cfg_t i2s_cfg_write = I2S_STREAM_CFG_DEFAULT();
	i2s_cfg_write.type = AUDIO_STREAM_WRITER;
//	i2s_cfg_write.task_stack = -1;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg_write);
	audio_pipeline_register(player, i2s_stream_writer, "i2s_write");
	audio_element_set_output_timeout(i2s_stream_writer, portMAX_DELAY);

	raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
	raw_cfg.type        = AUDIO_STREAM_WRITER;
//	raw_cfg.out_rb_size = -1;
	raw_write_el = raw_stream_init(&raw_cfg);
	audio_element_set_output_timeout(raw_read_el, portMAX_DELAY);
	audio_pipeline_register(player, raw_write_el, "raw_write");

	opus_decoder_cfg_t opus_cfg = DEFAULT_OPUS_DECODER_CONFIG();
    decoder = decoder_opus_init(&opus_cfg);
	audio_pipeline_register(player, decoder, "opus_decode");
	audio_element_set_output_timeout(decoder, portMAX_DELAY);
	
	const char *link_tag[3] = {"raw_write", "opus_decode", "i2s_write"};
	audio_pipeline_link(player, &link_tag[0], 3);
	
	audio_element_info_t music_info = {0};
	music_info.bits = 16;
	music_info.bps = 64000;
	music_info.channels = 1;
	music_info.duration = 20;
	music_info.sample_rates = 16000;
	audio_element_setinfo(i2s_stream_writer, &music_info);
	audio_element_setinfo(decoder, &music_info);
	audio_element_setinfo(raw_write_el, &music_info);
	
//	printf("music info sample-rate %d  channel %d bits %d bps %d \r\n", music_info.sample_rates, music_info.channels, music_info.bits, music_info.bps);
//	printf("music info bytes_pos %lld  total_bytes %lld duration %d codec %d \r\n", music_info.byte_pos, music_info.total_bytes, music_info.duration, music_info.codec_fmt);

//	audio_element_set_input_ringbuf(decoder, recv_ringbuf);
//	audio_element_set_write_cb(decoder, i2s_write_cb, NULL);
//	audio_element_set_output_timeout(decoder, portMAX_DELAY);
//	audio_element_set_music_info(decoder, CONFIG_PCM_SAMPLE_RATE, 1, 16);

	printf("audio player has been created\r\n");
  	return ESP_OK;
}

static int i2s_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int bytes_read = audio_element_input(i2s_stream_reader, buf, len);
    if (bytes_read < 0) {
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
	
	// create raw element handle
	raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
	raw_cfg.type        = AUDIO_STREAM_READER;
	raw_read_el = raw_stream_init(&raw_cfg);
	audio_element_set_output_timeout(raw_read_el, portMAX_DELAY);
	audio_pipeline_register(recorder, raw_read_el, "raw_read");

	i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, 16000, 16, AUDIO_STREAM_READER);
	i2s_cfg_read.type = AUDIO_STREAM_READER;
//	i2s_cfg_read.task_stack = -1;
	i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);
	audio_element_set_output_timeout(i2s_stream_reader, portMAX_DELAY);
	audio_pipeline_register(recorder, i2s_stream_reader, "i2s_read");

//	algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
//	algo_config.sample_rate = CONFIG_PCM_SAMPLE_RATE;
//	algo_config.out_rb_size = ESP_RING_BUFFER_SIZE;
//	algo_config.debug_input = true;
//	algo_config.swap_ch = true;
//	element_algo = algo_stream_init(&algo_config);
//	audio_element_set_music_info(element_algo, CONFIG_PCM_SAMPLE_RATE, 1, 16);
//	audio_element_set_read_cb(element_algo, i2s_read_cb, NULL);
//	audio_element_set_input_timeout(element_algo, portMAX_DELAY);
//	audio_pipeline_register(recorder, element_algo, "algo");

	opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();
	opus_cfg.complexity = 0;
	encoder = encoder_opus_init(&opus_cfg);
	audio_pipeline_register(recorder, encoder, "encode");	
	audio_element_set_output_timeout(encoder, portMAX_DELAY);
	
	const char *link_tag[3] = {"i2s_read", "encode", "raw_read"}; 
	audio_pipeline_link(recorder, &link_tag[0], 3);
	
	audio_element_info_t music_info = {0};
	music_info.bits = 16;
	music_info.bps = 64000;
	music_info.channels = 1;
	music_info.duration = 20;
	music_info.sample_rates = 16000;
	audio_element_setinfo(i2s_stream_reader, &music_info);
	audio_element_setinfo(encoder, &music_info);
	audio_element_setinfo(raw_read_el, &music_info);
	
//	audio_element_setinfo(element_algo, &music_info);
//	printf("music info sample-rate %d  channel %d bits %d bps %d \r\n", music_info.sample_rates, music_info.channels, music_info.bits, music_info.bps);
//	printf("music info bytes_pos %lld  total_bytes %lld duration %d codec %d \r\n", music_info.byte_pos, music_info.total_bytes, music_info.duration, music_info.codec_fmt);
		
//	audio_element_set_read_cb(encoder, i2s_read_cb, NULL);
//	audio_element_set_music_info(encoder, CONFIG_PCM_SAMPLE_RATE, 1, 16);
//	audio_element_set_input_timeout(encoder, portMAX_DELAY);

	printf( "audio recorder has been created\r\n");
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
	if(userjoin == 1 && call_flag == 1){
		int ret = raw_stream_write(raw_write_el, (char *)data, len);
		if(ret < 0){
			printf("__on_audio_data send data error ret = %d\r\n", ret);
		}
		struct timeval tp;
		gettimeofday(&tp, NULL);
		audiolog("ret = %d len = %d data_type %d  time = %ld\r\n", ret, len, info_ptr->data_type, tp.tv_usec / 1000);
	}
}	
#endif //#ifdef CONFIG_ENABLE_AUDIO_MIXING

static int send_rtc_audio_frame(uint8_t *data, uint32_t len)
{
  // API: send audio data
  	audio_frame_info_t info = { 0 };
	info.data_type = AUDIO_DATA_TYPE_OPUSFB;
//  	info.data_type = AUDIO_DATA_TYPE_PCM;
	struct timeval tp;
	gettimeofday(&tp, NULL);
	
  int rval = agora_rtc_send_audio_data(g_conn_id, data, len, &info);
  if (rval < 0) {
    printf("Failed to send audio data, reason: %s\n", agora_rtc_err_2_str(rval));
    return -1;
  }
  else{printf("agora_rtc_send_audio_data success len = %ld time = %ld type %d\r\n", len, tp.tv_usec / 1000, info.data_type);}

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
		vTaskDelay(20 / portTICK_PERIOD_MS);
	}
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

	recorder_pipeline_open();
  	play_pipeline_open();
	peripherals_key_open();
	
	audio_pipeline_run(player);
	audio_pipeline_run(recorder);
	printf("audio_send_thread start\r\n");
    while (g_app.b_call_session_started) {

		if(userjoin == 1 && call_flag == 1){
			ret = raw_stream_read(raw_read_el, (char *)raw_buf, read_len);
			if (ret != read_len) {
				printf( "raw read error, expect %d, but only %d\n", read_len, ret);
			}
			i2s_data_divided((int16_t *)raw_buf, read_len, (int16_t *)pcm_buf);

			send_rtc_audio_frame((uint8_t *)pcm_buf, CONFIG_PCM_DATA_LEN);
		}
		
	  	vTaskDelay(20 / portTICK_PERIOD_MS);
    }
	printf("_pipeline_close close\r\n");
    //deinit
    _pipeline_close(recorder); 
	_pipeline_close(player);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

	free(raw_buf);
	free(pcm_buf);

	rb_destroy(recv_ringbuf);

	vTaskDelete(NULL);
}
static void app_init_event_handler(agora_rtc_event_handler_t *event_handler)
{
  event_handler->on_join_channel_success = __on_join_channel_success;
  event_handler->on_connection_lost = __on_connection_lost;
  event_handler->on_rejoin_channel_success = __on_rejoin_channel_success;
  event_handler->on_user_joined = __on_user_joined;
  event_handler->on_user_offline = __on_user_offline;
  event_handler->on_user_mute_audio = __on_user_mute_audio;
  event_handler->on_error = __on_error;
  event_handler->on_audio_data = __on_audio_data;
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
  channel_options.audio_codec_opt.audio_codec_type = AUDIO_CODEC_DISABLED; 
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
    sleep(10);
  }
  
  agora_rtc_leave_channel(g_conn_id);

  return 0;
}
