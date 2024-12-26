#include <stdio.h>

// �ͻ��������
typedef struct request_data{
	int 	force_property_sip; 	//������ҵ��ʶ
	int 	force_sip; 				//sosǿ��sipͨ��
	int 	call_duration;			//ͨ��ʱ��
	int 	accout_type;			//�˺����� Ĭ�� 1
	char  	channel_name[32];			//Ƶ������ ������ʱ��ȡ
	char    version[16];				//�汾
}request_data_t;

typedef struct request_json{
	char  cmd[32];				//ָ������
	char  version[16];			//Ӧ�ð汾��
	char  device[16];			//�豸mac��
	char  request_id[128];		//�豸?�ɵ������Ϣid������������responseʱ����Я������Ϣ
	long long ts;				//���뼶ʱ���
	request_data_t data; 	
}request_json_t;

/*============================================================================================================*/

// Ӧ��� / �·�ָ��
typedef struct alarm_data{
	int 	id;				//Ψһid
	int 	mode;			// 0 ���� 1 ÿ��
	char  	date[5];			// ʱ��   "12:20"
	char  	ring_tone_url[128]; 	//������Դ
}alarm_data_t;

typedef struct agora_token_data{
	char  token[256];
	char  channel_name[33];
	char  app_id[33];
	char  license_value[33]; 
}agora_token_data_t;

typedef struct response_data{
	int type;				//���ͣ�0:��ֹ��1:�ƶԽ���2:����ͨ�� ������1����1
	int channel_type;		//���ͣ�1:Sip��2:rtc ������1����1
	int mobile;				//�ֻ���
	int count;				//Ƶ������ 
	int is_open;			//log�ϴ� 1 �� 0 �ر�
	int interval;			//���ʱ��
	char  upload_url[128];		//�ϴ���ַ
	char  oss_key[128];			//��Կ
	char  content[128];		//�������� ���tts�����⣬����ͨ������Զ��mp3��?ʽ
	char  rtc_type[5];		//��Ƶ��video��������voice������1��ֻ������
	char  file_name[16];	//�汾����
	char  source_url[128];		//�汾��Դ
	agora_token_data_t agora_rtc_token_dto;  //����sdk��Ϣ
	alarm_data_t alarm;
}response_data_t;

typedef struct response_json{
	int code;				//0Ϊ������Ӧ������Ϊҵ�����
	long long ts;				//�豸�ϱ�ʱ�䣨���뼶ʱ�����
	char  cmd[32];				//ָ������
	char  version[16];			//�汾��
	char  device[16];			//�豸mac��
	char  request_id[128];		//�豸���ɵ������Ϣid������������responseʱ����Я������Ϣ
	char  message[32];			//code��Ϊ0ʱ�Ĵ�������
	response_data_t data;
}response_json_t;

/*=============================================================================================================*/
typedef struct tcp_data{
	char auth[128];
	char broker[128];
	char auth_sec_key[32];
	char comm_sec_key[32];
	int comm_server_code;
} tcp_data_t;

typedef struct tcp_json{
	int code;
	char message[32];
	tcp_data_t data;
}tcp_json_t;

