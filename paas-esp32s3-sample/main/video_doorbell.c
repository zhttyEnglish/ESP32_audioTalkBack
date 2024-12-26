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
//#include "app_config.h"
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
#include "opus_decoder.h"
#include "opus_encoder.h"
#include "board.h"
#include "board_pins_config.h"

#include "json_parse.h"
#include "mqtt.h"
#include "tcp.h"
#include "spiffs_stream.h"

//#include "gpio.h"

// #include "esp_pm.h"
// #include "input_key_service.h"
// #include "amf_ipcam.h"
// #include "license_activator_v2.h"


#define AUDIOLOG_DEBUG
#ifdef AUDIOLOG_DEBUG
#define audiolog(format, ...) \
  			printf("[%s:%d] "format, __func__, __LINE__,  ##__VA_ARGS__)
#else
#define	audiolog(format, ...)
#endif


#define CONFIG_SEND_PCM_DATA
//#define CONFIG_USE_G722_CODEC
//#define USE_G722

//#define CONFIG_USE_G711A_CODEC
//#define USE_G711

#define CONFIG_USE_OPUS_CODEC
#define USE_OPUS

#define REAL_HARDWARE
#define CONFIG_AUDIO_ONLY

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
#define AUDIO_CODEC_TYPE AUDIO_CODEC_TYPE_G722
#define CONFIG_PCM_SAMPLE_RATE (16000)
#define CONFIG_PCM_DATA_LEN  640

#elif defined(CONFIG_USE_G711A_CODEC)
#define AUDIO_CODEC_TYPE AUDIO_CODEC_TYPE_G711A
#define CONFIG_PCM_SAMPLE_RATE (8000)
#define CONFIG_PCM_DATA_LEN  320

#elif defined(CONFIG_USE_OPUS_CODEC)
#define AUDIO_CODEC_TYPE AUDIO_CODEC_DISABLED
#define CONFIG_PCM_SAMPLE_RATE (16000)
#define CONFIG_PCM_DATA_LEN  640

#endif

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
static audio_pipeline_handle_t player;

static audio_element_handle_t i2s_stream_writer, i2s_stream_reader, raw_read_el, element_algo;
#ifdef USE_OPUS	
static audio_element_handle_t encoder, decoder, raw_write_el;
#endif

static SemaphoreHandle_t g_audio_capture_sem  = NULL;

#ifdef USE_OPUS
ringbuf_handle_t recv_ringbuf = NULL; 
#endif

static connection_id_t g_conn_id;
static connection_info_t g_conn_info;
static audio_thread_t *g_audio_send_thread;

extern response_json_t response_json;
extern long long hang_up_ts;
extern long long answer_ts;
extern char publish_topic[48];
static int call_flag = 0;
static int start_flag = 0;
static int userjoined = 0;
int talkback_run = 0;

void hang_up_answer(int flag)
{
	if(flag == 0){  //hang up
		g_app.b_call_session_started = false;
		call_flag = 2;
		userjoined = 0;
		start_flag = 0;
		hang_up_ts = get_timestamp_ms();
	}
	else if(flag == 1){  //answer
		call_flag = 1;
		answer_ts = get_timestamp_ms();
	}
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
	
#if (defined USE_G722) || (defined USE_G711)
	i2s_stream_cfg_t i2s_cfg_write = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(I2S_NUM_0, CONFIG_PCM_SAMPLE_RATE, 
															16, AUDIO_STREAM_WRITER, I2S_SLOT_MODE_MONO);
    i2s_cfg_write.type = AUDIO_STREAM_WRITER;
	i2s_cfg_write.task_stack = -1;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg_write);

	audio_pipeline_register(player, i2s_stream_writer, "i2s_write");

	const char *link_tag[1] = {"i2s_write"};
	audio_pipeline_link(player, &link_tag[0], 1);
#endif

#ifdef USE_OPUS
#if 0
	opus_decoder_cfg_t opus_cfg = DEFAULT_OPUS_DECODER_CONFIG();
    decoder = decoder_opus_init(&opus_cfg);
	audio_pipeline_register(player, decoder, "opus_decode");
	
	const char *link_tag[1] = {"opus_decode"};
	audio_pipeline_link(player, &link_tag[0], 1);
	
//	recv_ringbuf = rb_create(2048, 1);
//	audio_element_set_input_ringbuf(decoder, recv_ringbuf);
	audio_element_set_write_cb(decoder, i2s_write_cb, NULL);
	audio_element_set_output_timeout(decoder, portMAX_DELAY);
	audio_element_set_music_info(decoder, CONFIG_PCM_SAMPLE_RATE, 1, 16);
#endif
	i2s_stream_cfg_t i2s_cfg_write = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(I2S_NUM_0, 16000, 
															16, AUDIO_STREAM_WRITER, I2S_SLOT_MODE_MONO);
//	i2s_stream_cfg_t i2s_cfg_write = I2S_STREAM_CFG_DEFAULT();
	i2s_cfg_write.type = AUDIO_STREAM_WRITER;
//	i2s_cfg_write.task_stack = -1;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg_write);
	audio_pipeline_register(player, i2s_stream_writer, "i2s_write");

	raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
	raw_cfg.type        = AUDIO_STREAM_WRITER;
//	raw_cfg.out_rb_size = -1;
	raw_write_el = raw_stream_init(&raw_cfg);
	audio_pipeline_register(player, raw_write_el, "raw_write");

	opus_decoder_cfg_t opus_cfg = DEFAULT_OPUS_DECODER_CONFIG();
    decoder = decoder_opus_init(&opus_cfg);
	audio_pipeline_register(player, decoder, "opus_decode");
	
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

#endif
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
	
#if (defined USE_G722) || (defined USE_G711)	
	// create raw element handle
	raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
	raw_cfg.type        = AUDIO_STREAM_READER;
	raw_read_el = raw_stream_init(&raw_cfg);
	audio_element_set_output_timeout(raw_read_el, portMAX_DELAY);
	audio_pipeline_register(recorder, raw_read_el, "raw");
	
	// create i2s element handle
	i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, CONFIG_PCM_SAMPLE_RATE, 16, AUDIO_STREAM_READER);
	i2s_cfg_read.type = AUDIO_STREAM_READER;
	i2s_cfg_read.task_stack = -1;
	i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);

	// create algorithm element handle
	algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
	algo_config.sample_rate = CONFIG_PCM_SAMPLE_RATE;
	algo_config.out_rb_size = ESP_RING_BUFFER_SIZE;
	algo_config.debug_input = true;
	algo_config.swap_ch = true;
	element_algo = algo_stream_init(&algo_config);
	audio_element_set_music_info(element_algo, CONFIG_PCM_SAMPLE_RATE, 1, ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT);
	audio_element_set_read_cb(element_algo, i2s_read_cb, NULL);
	audio_element_set_input_timeout(element_algo, portMAX_DELAY);
	audio_pipeline_register(recorder, element_algo, "algo");

	const char *link_tag[2] = {"algo", "raw"}; 
	audio_pipeline_link(recorder, &link_tag[0], 2);
#endif

#ifdef USE_OPUS
#if 0
	i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(I2S_NUM_0, CONFIG_PCM_SAMPLE_RATE, 
															16, AUDIO_STREAM_READER, I2S_SLOT_MODE_MONO);
	i2s_cfg_read.type = AUDIO_STREAM_READER;
	i2s_cfg_read.task_stack = -1;
	i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);
//	audio_pipeline_register(recorder, i2s_stream_reader, "i2s_read");

	algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
	algo_config.sample_rate = CONFIG_PCM_SAMPLE_RATE;
	algo_config.out_rb_size = ESP_RING_BUFFER_SIZE;
	algo_config.debug_input = true;
	algo_config.swap_ch = true;
	element_algo = algo_stream_init(&algo_config);
	audio_element_set_music_info(element_algo, CONFIG_PCM_SAMPLE_RATE, 1, 16);
	audio_element_set_read_cb(element_algo, i2s_read_cb, NULL);
	audio_element_set_input_timeout(element_algo, portMAX_DELAY);
	audio_pipeline_register(recorder, element_algo, "algo");

//	spiffs_stream_cfg_t flash_cfg = SPIFFS_STREAM_CFG_DEFAULT();
//	flash_cfg.type = AUDIO_STREAM_WRITER;
//	audio_element_handle_t spiffs_stream_reader = spiffs_stream_init(&flash_cfg);
//	audio_pipeline_register(recorder, spiffs_stream_reader, "spiffs");	
//	audio_element_info_t music_info = {0};
//	audio_element_getinfo(i2s_stream_reader, &music_info);
//	audio_element_setinfo(spiffs_stream_reader, &music_info);
//	audio_element_set_uri(spiffs_stream_reader, "/spiffs/record.opus");

	opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();
	encoder = encoder_opus_init(&opus_cfg);
	audio_pipeline_register(recorder, encoder, "encode");	
	
	const char *link_tag[2] = { "algo", "encode"}; 
	audio_pipeline_link(recorder, &link_tag[0], 2);
	
//	audio_element_set_read_cb(encoder, i2s_read_cb, NULL);
	audio_element_set_music_info(encoder, CONFIG_PCM_SAMPLE_RATE, 1, 16);
//	audio_element_set_input_timeout(encoder, portMAX_DELAY);
#endif
	// create raw element handle
	raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
	raw_cfg.type        = AUDIO_STREAM_READER;
	raw_read_el = raw_stream_init(&raw_cfg);
	audio_pipeline_register(recorder, raw_read_el, "raw_read");

	i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, CONFIG_PCM_SAMPLE_RATE, 16, AUDIO_STREAM_READER);
	i2s_cfg_read.type = AUDIO_STREAM_READER;
//	i2s_cfg_read.task_stack = -1;
	i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);
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
#endif

	printf( "audio recorder has been created\r\n");
	return ESP_OK;
}

static esp_err_t _pipeline_close(audio_pipeline_handle_t handle)
{
  audio_pipeline_stop(handle);
  audio_pipeline_wait_for_stop(handle);
//  audio_pipeline_terminate(handle);

  return ESP_OK;
}

static void __on_join_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed)
{
  g_app.b_call_session_started = true;
  agora_rtc_get_connection_info(conn_id, &g_conn_info);
  audiolog("[conn-%lu] Join the channel %s successfully, uid %lu elapsed %d ms\n", conn_id, g_conn_info.channel_name, uid, elapsed);
}

static void __on_connection_lost(connection_id_t conn_id)
{
  g_app.b_call_session_started = false;
  printf("[conn-%lu] Lost connection from the channel\n", conn_id);
}

static void __on_reconnecting(connection_id_t conn_id)
{
  printf("[conn-%lu] Reconnecting \n", conn_id);
}

static void __on_rdt_state(connection_id_t conn_id, uint32_t uid, rdt_state_e state)
{
  audiolog("[conn-%lu] __on_rdt_state uid %lu  status %d \n", conn_id, uid, state);
}
static void __on_rdt_msg(connection_id_t conn_id, uint32_t uid, rdt_stream_type_e type, const void *msg, size_t len)
{
  audiolog("[conn-%lu] __on_rdt_msg uid %lu type %d len %d msg %s\n", conn_id, uid, type, len, (char *)msg);
}
static void __on_rejoin_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
  g_app.b_call_session_started = true;
  printf("[conn-%lu] Rejoin the channel successfully, uid %lu elapsed %d ms\n", conn_id, uid, elapsed_ms);
}

static void __on_user_joined(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
	userjoined = 1;
  printf("[conn-%lu] Remote user \"%lu\" has joined the channel, elapsed %d ms\n", uid, conn_id, elapsed_ms);
}

static void __on_user_offline(connection_id_t conn_id, uint32_t uid, int reason)
{
	userjoined = 0;
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
	if(call_flag == 1 && userjoined == 1 && g_app.b_call_session_started == true && start_flag == 1)
	{
		if(len > 0 && data != NULL)
		{	
			int ret = 0;
#if (defined USE_G722) || (defined USE_G711)
			ret = audio_element_output(i2s_stream_writer, (char *)data, len);
#endif
#ifdef USE_OPUS
			//ret = rb_write(recv_ringbuf, (char *)data, len, portMAX_DELAY);
			//ret = audio_element_output(decoder, (char *)data, len);
			ret = raw_stream_write(raw_write_el, (char *)data, len);
#endif
			struct timeval tp;
			gettimeofday(&tp, NULL);
			audiolog("ret = %d len = %d data_type %d  time = %ld\r\n", ret, len, info_ptr->data_type, tp.tv_usec / 1000);
		}
	}
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
#ifdef USE_OPUS
  	info.data_type = AUDIO_DATA_TYPE_OPUS;
#endif

#if (defined USE_G722) || (defined USE_G711)
	info.data_type = AUDIO_DATA_TYPE_PCM;
#endif
	struct timeval tp;
	gettimeofday(&tp, NULL);
	
	int rval = agora_rtc_send_audio_data(g_conn_id, data, len, &info);
	if (rval < 0) {
	printf("Failed to send audio data, reason: %d %s\n", rval, agora_rtc_err_2_str(rval));
		return -1;
	}
	else{printf("agora_rtc_send_audio_data success len = %ld time = %ld datatype %d\r\n", len, tp.tv_usec / 1000, info.data_type);}

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

void audio_talkback()
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

	recorder_pipeline_open();
  	play_pipeline_open();
	
	audio_pipeline_run(player);
	audio_pipeline_run(recorder);

	printf("call_flag  %d \r\n", call_flag);

	start_flag = 1;

	while(1){	
		if(call_flag == 1 && userjoined == 1 && g_app.b_call_session_started == true)
		{
			ret = raw_stream_read(raw_read_el, (char *)raw_buf, read_len);
			if (ret != read_len) {
				printf( "raw read error, expect %d, but only %d\n", read_len, ret);
			}
			i2s_data_divided((int16_t *)raw_buf, read_len, (int16_t *)pcm_buf);
		
			send_rtc_audio_frame((uint8_t *)pcm_buf, CONFIG_PCM_DATA_LEN);
		}
		if(call_flag == 2){
			break;
		}
	  	vTaskDelay(15/ portTICK_PERIOD_MS);
    }
	printf("_pipeline_close close\r\n");
    //deinit
    
    _pipeline_close(recorder); 
	_pipeline_close(player);
	
	audio_pipeline_unregister(recorder, element_algo);
	audio_pipeline_unregister(recorder, raw_read_el);
	audio_pipeline_unregister(recorder, i2s_stream_reader);
	audio_pipeline_unregister(player, i2s_stream_writer);

	audio_element_deinit(i2s_stream_reader);
	audio_element_deinit(element_algo);
	audio_element_deinit(raw_read_el);
	audio_element_deinit(i2s_stream_writer);
	
	audio_pipeline_deinit(recorder);
	audio_pipeline_deinit(player);
	printf("audio_pipeline_deinit done\r\n");
	
  	free(raw_buf);
  	free(pcm_buf);
	raw_buf = NULL;
	pcm_buf = NULL;

#ifdef USE_OPUS
	rb_destroy(recv_ringbuf);
#endif

}
#if 0
static void audio_send_thread(void *arg)
{
	audio_talkback();
}
#endif

static void app_init_event_handler(agora_rtc_event_handler_t *event_handler)
{
  event_handler->on_join_channel_success = __on_join_channel_success;
  event_handler->on_connection_lost = __on_connection_lost;
  event_handler->on_reconnecting = __on_reconnecting;
  event_handler->on_rdt_state = __on_rdt_state;
  event_handler->on_rdt_msg = __on_rdt_msg;
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


int video_doorbell(void)
{
	printf("start video_doorbell\r\n");
	int rval;

	char * app_id = response_json.data.agora_rtc_token_dto.app_id;
	char * channel_name = response_json.data.agora_rtc_token_dto.channel_name;
	char * token = response_json.data.agora_rtc_token_dto.token;
	int user_id = 0;
	char * license_value = response_json.data.agora_rtc_token_dto.license_value;

	printf("app_id = %s\r\n", app_id);
	printf("channel_name = %s\r\n", channel_name);
	printf("token = %s\r\n", token);
	printf("license = %s\r\n", license_value);

	g_audio_capture_sem = xSemaphoreCreateBinary();
	if (NULL == g_audio_capture_sem) {
		printf( "Unable to create audio capture semaphore!\n");
		return -1;
	}

	// 2. API: init agora rtc sdk
	agora_rtc_event_handler_t event_handler = { 0 };
	app_init_event_handler(&event_handler);

	rtc_service_option_t service_opt = { 0 };
	service_opt.area_code            = AREA_CODE_GLOB;
	service_opt.log_cfg.log_disable  = true;
	service_opt.log_cfg.log_level    = RTC_LOG_NOTICE;
	service_opt.log_cfg.log_path     = DEFAULT_SDK_LOG_PATH;
	service_opt.domain_limit         = false;
	
	sprintf(service_opt.license_value, "%s", license_value);
	
	rval = agora_rtc_init(app_id, &event_handler, &service_opt);
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
	channel_options.auto_connect_rdt = true;
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

	rval = agora_rtc_join_channel(g_conn_id, channel_name, user_id, token, &channel_options);
	if (rval < 0) {
		printf("Failed to join channel \"%s\", reason: %s\n", channel_name, agora_rtc_err_2_str(rval));
		return -1;
	}

	while(!g_app.b_call_session_started) {
		usleep(200 * 1000);
	}
	printf("~~~~~agora_rtc_join_channel success~~~~\r\n");

	audio_talkback();
//	audio_sys_get_real_time_stats();
	while (call_flag == 1) {
		if(call_flag == 2){
			break;
		}
		vTaskDelay(100/ portTICK_PERIOD_MS);
	}

	agora_rtc_leave_channel(g_conn_id);
	agora_rtc_fini();

//	mqtt_publish_pre(UPDATE_RECORD_STATUS_TO_ANSWER);
	call_flag = 0;

	return 0;
}

static void audio_send_thread(void *arg)
{
	printf("audio_send_thread start\r\n");
	while(1)
	{
//		printf("audio_send_thread wait talkback_run\r\n");
		if(talkback_run == 1)
		{
			printf("talkback_run == 1 enter\r\n");
			talkback_run = 0;
			video_doorbell();
			printf("audio_send_thread video_doorbell done\r\n");
//			mqtt_publish_pre(UPDATE_RECORD_STATUS_TO_ANSWER);
		}
		vTaskDelay(200/ portTICK_PERIOD_MS);
	}
}

int video_doorbell_thread_init(void)
{
	int rval = audio_thread_create(g_audio_send_thread, "send_task", audio_send_thread, NULL, 8 * 1024, PRIO_TASK_FETCH, true, 0);
	if (rval != ESP_OK) {
		printf( "Unable to create audio capture thread!\n");
		return -1;
	}
	return 0;
}


