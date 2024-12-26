/* upgrade app and data partition Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "audio_common.h"

#include "esp_log.h"
#include "esp_peripherals.h"
#include "esp_system.h"

#include "audio_mem.h"
#include "ota_service.h"
#include "ota_proc_default.h"

static const char *TAG = "HTTPS_OTA_EXAMPLE";
static EventGroupHandle_t events = NULL;

#define OTA_FINISH (BIT0)

static esp_err_t ota_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    if (evt->type == OTA_SERV_EVENT_TYPE_RESULT) {
        ota_result_t *result_data = evt->data;
        if (result_data->result != ESP_OK) {
            ESP_LOGE(TAG, "List id: %d, OTA failed", result_data->id);
        } else {
            ESP_LOGI(TAG, "List id: %d, OTA success", result_data->id);
        }
    } else if (evt->type == OTA_SERV_EVENT_TYPE_FINISH) {
        xEventGroupSetBits(events, OTA_FINISH);
    }

    return ESP_OK;
}

void start_ota()
{
	ESP_LOGI(TAG, "[2.0] Create OTA service");
    ota_service_config_t ota_service_cfg = OTA_SERVICE_DEFAULT_CONFIG();
    ota_service_cfg.task_stack = 8 * 1024;
    ota_service_cfg.evt_cb = ota_service_cb;
    ota_service_cfg.cb_ctx = NULL;
    periph_service_handle_t ota_service = ota_service_create(&ota_service_cfg);
    events = xEventGroupCreate();

	char * uri = "http://192.168.61.160:8080/tcp/build/mqtt_tcp.bin";
	char * partition_lebal = "ota";
    ESP_LOGI(TAG, "[2.1] Set upgrade list");
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_APP,
                NULL,
                uri,
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            true,
            false
        }
    };

    ota_app_get_default_proc(&upgrade_list[0]);

    ota_service_set_upgrade_param(ota_service, upgrade_list, 1);

    ESP_LOGI(TAG, "[2.2] Start OTA service");
    AUDIO_MEM_SHOW(TAG);
    periph_service_start(ota_service);

    EventBits_t bits = xEventGroupWaitBits(events, OTA_FINISH, true, false, portMAX_DELAY);
    if (bits & OTA_FINISH) {
        ESP_LOGI(TAG, "[2.3] Finish OTA service");
    }
    ESP_LOGI(TAG, "[2.4] Clear OTA service");
    periph_service_destroy(ota_service);
    vEventGroupDelete(events);

}

