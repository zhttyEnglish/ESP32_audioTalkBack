/**
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_audio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Audio decoder input frame
 *
 * @note  User need process the `consumed` like following:
 *
 * @code{c}
 *           esp_audio_dec_in_raw_t raw = {.buffer = buffer, .len = len};
 *           uint8_t *frame_data = (uint8_t*) malloc(4096);
 *           esp_audio_dec_out_frame_t out_frame = {.buffer = frame_data, .len = 4096};
 *           while (raw.len) {
 *               ret = esp_audio_dec_process(decoder, &raw, &out_frame);
 *               if (ret != ESP_AUDIO_ERR_OK) {
 *                   break;
 *               }
 *               raw.buffer += raw.consumed;
 *               raw.len -= raw.consumed;
 *           }
 *           free(out_frame.buffer);
 *@endcode
 *
 */
typedef struct {
    uint8_t *buffer;   /*!< Input encoded data buffer */
    uint32_t len;      /*!< Input encoded data size */
    uint32_t consumed; /*!< Input data consumed size */
} esp_audio_dec_in_raw_t;

/**
 * @brief  Audio decoder output frame
 *
 * @note  User can handle `needed_size` using following code:
 *
 * @code{c}
 *           esp_audio_dec_in_raw_t raw = {.buffer = buffer, .len = len};
 *           uint8_t *frame_data = (uint8_t*) malloc(4096);
 *           esp_audio_dec_out_frame_t out_frame = {.buffer = frame_data, .len = 4096};
 *           while (raw.len) {
 *               ret = esp_audio_dec_process(decoder, &raw, &out_frame);
 *               if (ret != ESP_AUDIO_ERR_OK && ret != ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
 *                   break;
 *               }
 *               raw.buffer += raw.consumed;
 *               raw.len -= raw.consumed;
 *               if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
 *                    uint8_t *new_frame_data = (uint8_t*) realloc(out_frame.buffer, out_frame.needed_size);
 *                    if (new_frame_data == NULL) {
 *                        break;
 *                    }
 *                    out_frame.buffer = new_frame_data;
 *                    out_frame.len = out_frame.needed_size;
 *               }
 *           }
 *           free(out_frame.buffer);
 *@endcode
 *
 */
typedef struct {
    uint8_t *buffer;       /*!< Output buffer to hold decoded PCM data */
    uint32_t len;          /*!< Output buffer size */
    uint32_t needed_size;  /*!< Set when output buffer size not enough (output) */
    uint32_t decoded_size; /*!< Decoded PCM data size (output) */
} esp_audio_dec_out_frame_t;

/**
 * @brief  Audio decoder configuration.
 */
typedef struct {
    esp_audio_type_t type;   /*!< Audio decoder type */
    void            *cfg;    /*!< Audio decoder specified configuration (required by special decoder or special use case, refer to corresponding decoder header file for details)
                                  For example, to decode AAC with no ADTS header, need set it to pointer to 'esp_aac_dec_config_t' */
    uint32_t         cfg_sz; /*!< Size of "cfg". For example, to decode AAC with no ADTS header set to sizeof 'esp_aac_dec_config_t' */
} esp_audio_dec_cfg_t;

/**
 * @brief  Audio decoder information
 */
typedef struct {
    uint32_t sample_rate;     /*!< Audio sample rate */
    uint8_t  bits_per_sample; /*!< Bits per sample */
    uint8_t  channel;         /*!< Audio channel*/
    uint32_t bitrate;         /*!< Audio bitrate (bits per second) */
    uint32_t frame_size;      /*!< Audio frame size */
} esp_audio_dec_info_t;

/**
 * @brief  Handle of audio decoder
 */
typedef void *esp_audio_dec_handle_t;

/**
 * @brief  Open audio decoder
 *
 * @param[in]   config   Audio decoder configuration
 * @param[out]  decoder  Audio decoder handle
 *
 * @return
 *       - ESP_AUDIO_ERR_OK                 On success
 *       - ESP_AUDIO_ERR_NOT_SUPPORT        Decoder not register yet
 *       - ESP_AUDIO_ERR_INVALID_PARAMETER  Invalid parameter
 *       - ESP_AUDIO_ERR_MEM_LACK           No enough memory
 */
esp_audio_err_t esp_audio_dec_open(esp_audio_dec_cfg_t *config, esp_audio_dec_handle_t *decoder);

/**
 * @brief  Decode encoded audio data to get output PCM data
 *
 * @param[in]      decoder  Audio  decoder handle
 * @param[in,out]  raw      Input encoded data to be decoded
 * @param[in,out]  frame    Output frame settings
 *
 * @return
 *       - ESP_AUDIO_ERR_OK                 On success
 *       - ESP_AUDIO_ERR_INVALID_PARAMETER  Invalid parameter
 *       - ESP_AUDIO_ERR_FAIL               Decode error
 *       - ESP_AUDIO_ERR_MEM_LACK           No enough memory
 *       - ESP_AUDIO_ERR_NOT_SUPPORT        Not supported by audio decoder (caller can stop following decode operations)
 *       - ESP_AUDIO_ERR_BUFF_NOT_ENOUGH    Output memory not enough for output frame
 *                                          Need re-allocated output memory according reported `needed_size` and retry
 */
esp_audio_err_t esp_audio_dec_process(esp_audio_dec_handle_t *decoder, esp_audio_dec_in_raw_t *raw,
                                      esp_audio_dec_out_frame_t *frame);

/**
 * @brief  Get audio decoder information
 *
 * @note  User can get decoder information after call `esp_audio_dec_process` and succeed to get decoded data.
 *        This API just fetches saved information with no extra cost.
 *        Basic information (for playback) needs only fetch once like `sample_rate`, `channel` etc.
 *        Other information in a frame like `frame_size` and `bitrate` will be updated each time after call API
 *        `esp_audio_dec_process`, if user want to estimate file duration according average `bitrate`,
 *        needs call this API to update it
 *
 * @param[in]   decoder  Audio decoder handle
 * @param[out]  info     Audio decoder information
 *
 * @return
 *       - ESP_AUDIO_ERR_OK                 On success
 *       - ESP_AUDIO_ERR_INVALID_PARAMETER  Invalid input parameter
 *       - ESP_AUDIO_ERR_NOT_FOUND          Decoder information not ready yet
 */
esp_audio_err_t esp_audio_dec_get_info(esp_audio_dec_handle_t decoder, esp_audio_dec_info_t *info);

/**
 * @brief  Close audio decoder
 *
 * @param[in]  decoder  Audio decoder handle
 */
void esp_audio_dec_close(esp_audio_dec_handle_t decoder);

#ifdef __cplusplus
}
#endif
