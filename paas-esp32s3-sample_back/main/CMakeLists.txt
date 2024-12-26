idf_component_register(SRCS video_doorbell.c
					REQUIRES esp_wifi nvs_flash agora_iot_sdk lwip audio_pipeline audio_stream audio_sal audio_board esp_peripherals audio_hal freertos esp-adf-libs display_service esp_dispatcher esp_actions wifi_service dueros_service clouds esp-sr 
					tone_partition)
					