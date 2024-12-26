#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_spiffs.h"
#include "audio_sys.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "audio_hal.h"
#include "board.h"

#include "mqtt.h"
#include "video_doorbell.h"
#include "key.h"
#include "blufi_example.h"
#include "file_server.h"
#include "ring.h"

#define DEFAULT_LISTEN_INTERVAL CONFIG_EXAMPLE_WIFI_LISTEN_INTERVAL

static const char *TAG = "example";

char passward[32] = {0};
char wifissid[32] = {0};
int wifi_connected = 0;
char  mac[16] = {0};

static void read_config_txt(void)
{
    printf("Reading config.txt\r\n");

    // Open for reading hello.txt
    FILE* f = fopen("/spiffs/config.txt", "r");
    if (f == NULL) {
        printf("Failed to open config.txt");
        return;
    }

    char buf[64];
    memset(buf, 0, sizeof(buf));
    fread(buf, 1, sizeof(buf), f);
    fclose(f);

    // Display the read contents from the file
    printf("Read from config.txt: %s\r\n", buf);

	sscanf(buf, "%s %s", wifissid, passward);
//	printf("%s\t%s\r\n", wifissid, passward);
}

static void read_mac_txt(void)
{
    printf("Reading mac.txt\r\n");

    // Open for reading hello.txt
    FILE* f = fopen("/spiffs/sub/mac.txt", "r");
    if (f == NULL) {
        printf("Failed to open mac.txt");
        return;
    }

    char buf[20];
    memset(buf, 0, sizeof(buf));
    fread(buf, 1, sizeof(buf), f);
    fclose(f);

    // Display the read contents from the file
    printf("Read from mac.txt: %s\r\n", buf);

	sscanf(buf, "%s", mac);
//	printf("%s\t%s\r\n", wifissid, passward);
}

static void setup_audio(void)
{
	audio_board_handle_t board_handle;
	int volume = 50;

   board_handle = audio_board_init();
   //audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);
   audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
   audio_hal_enable_pa(board_handle->audio_hal, true);
   //printf("after pa enable ret = %d\r\n", ret);
   audio_hal_set_volume(board_handle->audio_hal, volume);
}

int app_main()
{
	printf("Initializing SPIFFS\r\n");

	const char * basePath = "/spiffs";
    esp_vfs_spiffs_conf_t conf = {
      .base_path = basePath,
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = false
    };
	esp_vfs_spiffs_register(&conf);

    size_t total = 0, used = 0;
	esp_spiffs_info(NULL, &total, &used);
    printf("Partition size: total: %d, used: %d\r\n", total, used);

    read_config_txt();
	read_mac_txt();

	ESP_ERROR_CHECK(nvs_flash_init());

	start_blufi();
//	setup_wifi();
	
	while(wifi_connected == 0) vTaskDelay(10/ portTICK_PERIOD_MS);

	setup_audio();
	
	input_key_init();

	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "ntp.aliyun.com");
	setenv("TZ", "CST-8", 1);
	sntp_init();
	
	while(sntp_get_sync_status() !=  SNTP_SYNC_STATUS_COMPLETED) {
		printf("wait for sntp_get_sync_status\r\n");
		vTaskDelay(1000/ portTICK_PERIOD_MS);
	}
	
	mqtt_init();
	
	start_file_server(basePath);
	
	video_doorbell_thread_init();

//	record_and_save();
	
	while(1){
		//audio_sys_get_real_time_stats();
		vTaskDelay(10000/ portTICK_PERIOD_MS);
	}
	return 0;
}

