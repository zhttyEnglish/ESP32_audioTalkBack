idf_component_register(
	SRCS 
		main.c 
		video_doorbell.c
		mqtt.c 
		json_parse.c 
		tcp.c 
		sha256.c 
		sm4.c
		key.c
		handle_cmd.c
		blufi_example.c
		blufi_security.c
		blufi_init.c
		file_server.c
		ring.c
		
	INCLUDE_DIRS "."
	
	REQUIRES 
			 esp_wifi 
			 nvs_flash 
			 agora_iot_sdk 
			 lwip audio_pipeline 
			 audio_stream 
			 audio_sal 
			 audio_board 
			 esp_peripherals 
			 audio_hal freertos 
			 esp-adf-libs 
			 display_service 
			 esp_dispatcher 
			 esp_actions 
			 wifi_service 
			 dueros_service 
			 clouds esp-sr 
			 tone_partition
			 mqtt
			 lwip
			 input_key_service
			 spiffs
			 bt
			 mbedtls
			 esp_http_server


	EMBED_FILES "favicon.ico" 
				"upload_script.html"
)
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
					