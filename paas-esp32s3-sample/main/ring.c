#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "sdkconfig.h"
#include "audio_sys.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "i2s_stream.h"
#include "spiffs_stream.h"
#include "mp3_decoder.h"
#include "opus_decoder.h"
#include "opus_encoder.h"
#include "esp_peripherals.h"
#include "periph_spiffs.h"
#include "board.h"
#include "tcp.h"
static const char *TAG = "SPIFFS_MP3_EXAMPLE";

#define MP3_PATH "/spiffs/callring.mp3"

#define OPUS_PATH "/spiffs/1.opus"

int ring_stop_flag = 0;
void ring_stop()
{
	ring_stop_flag = 1;
}

#define SAMPLE_RATE         16000
#define CHANNEL             1
#define BIT_RATE            64000
#define COMPLEXITY          10

void record_and_save()
{
	audio_pipeline_handle_t pipeline;
	audio_element_handle_t spiffs_stream_reader, i2s_stream_reader, audio_encoder;
	int channel_format = I2S_CHANNEL_TYPE_RIGHT_LEFT;
	int sample_rate = 16000;

	ESP_LOGI(TAG, "[3.0] Create audio pipeline for recording");
	audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	pipeline = audio_pipeline_init(&pipeline_cfg);
	mem_assert(pipeline);

	ESP_LOGI(TAG, "[3.1] Create spiffs stream to write data ");
	spiffs_stream_cfg_t flash_cfg = SPIFFS_STREAM_CFG_DEFAULT();
	flash_cfg.type = AUDIO_STREAM_WRITER;
	spiffs_stream_reader = spiffs_stream_init(&flash_cfg);

	ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
	i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
	i2s_cfg.type = AUDIO_STREAM_READER;
	sample_rate = SAMPLE_RATE;
	channel_format = I2S_CHANNEL_TYPE_ONLY_LEFT;
	i2s_stream_set_channel_type(&i2s_cfg, channel_format);
	i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = sample_rate;
	i2s_stream_reader = i2s_stream_init(&i2s_cfg);

	ESP_LOGI(TAG, "[3.3] Create audio encoder to handle data");
	opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();
	opus_cfg.sample_rate		= SAMPLE_RATE;
	opus_cfg.channel			= CHANNEL;
	opus_cfg.bitrate			= BIT_RATE;
	opus_cfg.complexity 		= COMPLEXITY;
	audio_encoder = encoder_opus_init(&opus_cfg);

	ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
	audio_pipeline_register(pipeline, spiffs_stream_reader, "spiffs");
	audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
	audio_pipeline_register(pipeline, audio_encoder, "opus");

	ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->i2s_stream-->audio_encoder-->spiffs_stream-->[spiffs]");
	const char *link_tag[3] = {"i2s", "opus", "spiffs"};
	audio_pipeline_link(pipeline, &link_tag[0], 3);

	ESP_LOGI(TAG, "[3.6] Set music info to fatfs");
	audio_element_info_t music_info = {0};
	audio_element_getinfo(i2s_stream_reader, &music_info);
	ESP_LOGI(TAG, "[ * ] Save the recording info to the fatfs stream writer, sample_rates=%d, bits=%d, ch=%d",
				music_info.sample_rates, music_info.bits, music_info.channels);
	audio_element_setinfo(spiffs_stream_reader, &music_info);

	ESP_LOGI(TAG, "[3.7] Set up  uri");
	audio_element_set_uri(spiffs_stream_reader, OPUS_PATH);

	ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
	audio_pipeline_run(pipeline);

	int time1 = get_timestamp();
	int time2 = 0;
	while (1) {
		time2 = get_timestamp();
		if(time2 - time1 == 5){
			break;
		}
		vTaskDelay(100/ portTICK_PERIOD_MS);
	}
	ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events, record for %d Seconds", time2 - time1);
	ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
	audio_pipeline_stop(pipeline);
	audio_pipeline_wait_for_stop(pipeline);
	audio_pipeline_terminate(pipeline);

	audio_pipeline_unregister(pipeline, audio_encoder);
	audio_pipeline_unregister(pipeline, i2s_stream_reader);
	audio_pipeline_unregister(pipeline, spiffs_stream_reader);

	/* Terminal the pipeline before removing the listener */
	audio_pipeline_remove_listener(pipeline);

	/* Release all resources */
	audio_pipeline_deinit(pipeline);
	audio_element_deinit(spiffs_stream_reader);
	audio_element_deinit(i2s_stream_reader);
	audio_element_deinit(audio_encoder);

}

void ring_play(void)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t spiffs_stream_reader, ring_i2s_stream_writer, mp3_decoder, decoder;

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    AUDIO_NULL_CHECK(TAG, pipeline, return);

    ESP_LOGI(TAG, "[3.1] Create spiffs stream to read data from sdcard");
    spiffs_stream_cfg_t flash_cfg = SPIFFS_STREAM_CFG_DEFAULT();
    flash_cfg.type = AUDIO_STREAM_READER;
    spiffs_stream_reader = spiffs_stream_init(&flash_cfg);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_TYLE_AND_CH(I2S_NUM_0, 16000, 16, AUDIO_STREAM_WRITER, 1);
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    ring_i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.3] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);
//	opus_decoder_cfg_t opus_cfg = DEFAULT_OPUS_DECODER_CONFIG();
//    decoder = decoder_opus_init(&opus_cfg);
	
    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, spiffs_stream_reader, "spiffs");
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
//	audio_pipeline_register(pipeline, decoder, "opus");
    audio_pipeline_register(pipeline, ring_i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[3.5] Link it together [flash]-->spiffs-->mp3_decoder-->i2s_stream-->[codec_chip]");
	const char *link_tag[3] = {"spiffs", "mp3", "i2s"};
//    const char *link_tag[3] = {"spiffs", "opus", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    ESP_LOGI(TAG, "[3.6] Set up  uri (file as spiffs, mp3 as mp3 decoder, and default output is i2s)");
	audio_element_set_uri(spiffs_stream_reader, MP3_PATH);
//    audio_element_set_uri(spiffs_stream_reader, OPUS_PATH);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

	while(1){
		audio_element_state_t el_state = audio_element_get_state(ring_i2s_stream_writer);
        if (el_state == AEL_STATE_FINISHED) {
			audio_element_set_uri(spiffs_stream_reader, MP3_PATH);
            audio_pipeline_reset_ringbuffer(pipeline);
            audio_pipeline_reset_elements(pipeline);
			audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
			audio_pipeline_run(pipeline);
		}
		if(ring_stop_flag == 1){
			//audio_pipeline_change_state(pipeline, AEL_STATE_FINISHED);
			break;
		}
		vTaskDelay(100/ portTICK_PERIOD_MS);
	}

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);

	audio_pipeline_unregister(pipeline, spiffs_stream_reader);
	audio_pipeline_unregister(pipeline, ring_i2s_stream_writer);
	audio_pipeline_unregister(pipeline, mp3_decoder);

	audio_element_deinit(spiffs_stream_reader);
	audio_element_deinit(ring_i2s_stream_writer);
	audio_element_deinit(mp3_decoder);

    audio_pipeline_deinit(pipeline);
	vTaskDelay(100/ portTICK_PERIOD_MS);
	ring_stop_flag = 0;
}
