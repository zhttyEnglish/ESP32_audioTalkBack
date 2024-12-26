#include <stdio.h>

// 客户端请求包
typedef struct request_data{
	int 	force_property_sip; 	//请求物业标识
	int 	force_sip; 				//sos强制sip通话
	int 	call_duration;			//通话时长
	int 	accout_type;			//账号类型 默认 1
	char  	channel_name[32];			//频道名称 被呼入时获取
	char    version[16];				//版本
}request_data_t;

typedef struct request_json{
	char  cmd[32];				//指令名称
	char  version[16];			//应用版本号
	char  device[16];			//设备mac号
	char  request_id[128];		//设备?成的随机消息id，服务器存在response时，将携带此信息
	long long ts;				//毫秒级时间戳
	request_data_t data; 	
}request_json_t;

/*============================================================================================================*/

// 应答包 / 下发指令
typedef struct alarm_data{
	int 	id;				//唯一id
	int 	mode;			// 0 单次 1 每天
	char  	date[5];			// 时间   "12:20"
	char  	ring_tone_url[128]; 	//铃声资源
}alarm_data_t;

typedef struct agora_token_data{
	char  token[256];
	char  channel_name[33];
	char  app_id[33];
	char  license_value[33]; 
}agora_token_data_t;

typedef struct response_data{
	int type;				//类型，0:禁止，1:云对讲，2:网络通话 ，银玲1号是1
	int channel_type;		//类型，1:Sip，2:rtc ，银玲1号是1
	int mobile;				//手机号
	int count;				//频道人数 
	int is_open;			//log上传 1 打开 0 关闭
	int interval;			//间隔时间
	char  upload_url[128];		//上传地址
	char  oss_key[128];			//密钥
	char  content[128];		//播放内容 如果tts有问题，可以通过播放远端mp3的?式
	char  rtc_type[5];		//视频：video，语音：voice，银玲1号只有语音
	char  file_name[16];	//版本名称
	char  source_url[128];		//版本资源
	agora_token_data_t agora_rtc_token_dto;  //声网sdk信息
	alarm_data_t alarm;
}response_data_t;

typedef struct response_json{
	int code;				//0为正常响应，其他为业务错误
	long long ts;				//设备上报时间（毫秒级时间戳）
	char  cmd[32];				//指令名称
	char  version[16];			//版本号
	char  device[16];			//设备mac号
	char  request_id[128];		//设备生成的随机消息id，服务器存在response时，将携带此信息
	char  message[32];			//code不为0时的错误描述
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

