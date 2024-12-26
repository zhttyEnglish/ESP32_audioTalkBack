#include <stdio.h>
#include <sys/types.h>        
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#include "sha256.h"
#include "json_parse.h"

//#define TCPLOG_DEBUG
#ifdef TCPLOG_DEBUG
#define tcplog(format, ...) \
  			printf("[%s:%d] "format, __func__, __LINE__,  ##__VA_ARGS__)
#else
#define	tcplog(format, ...)
#endif


#define FRAME_HEAD  0
#define DATA_LEN_H  1
#define DATA_LEN_L	2
#define FRAME_NUM	3
#define EVENT_TYPE	4
#define VERSION		5
#define DATA_START	6

typedef enum{
	timing = 1, 
	auth = 2, 
	heart_beat = 3,
}event_type_t;

extern char mac[16];
char  key[32] = {0};
long long timestamp = 0;
char  sign[65] = {0};
int version = 1;

extern tcp_json_t tcp_json;

long long get_timestamp()
{
	long long tmp;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    tmp = tv.tv_sec;
//    tmp = tmp * 1000;
//    tmp = tmp + (tv.tv_usec / 1000);
    return tmp;
}

long long get_timestamp_ms()
{
	long long tmp;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    tmp = tv.tv_sec;
    tmp = tmp * 1000;
    tmp = tmp + (tv.tv_usec / 1000);
    return tmp;
}

void reverse_str(char * buf, int len, char * result)
{
	int top = 0, end = len - 1, i, j;
	while(top < end){			//一直交换开头和末尾
		char temp = buf[top];
		buf[top] = buf[end];
		buf[end] = temp;
		top++;
		end--;
	}
	for(i = 0, j = 0; i < len; i ++){
        sprintf(&result[j], "%02X", buf[i]);
        j+=2; // 每个16进制占2个长度
	}
	result[j] = '\0'; // 添加字符串结束符
}

void str2hex(char * dest, char * src, int len)
{
	int i, j;
	for(i = 0, j = 0; i < len; i ++){
        sprintf(&dest[j], "%02x", src[i]);
        j+=2; // 每个16进制占2个长度
	}
	dest[j] = '\0';
}

int create_data_body(char * buf)
{
	char  tmp[200] = {0};
	char  sign_tmp[32] = {0};
	char  ts[12] = {0};
	int pos = 0;
 
//	sprintf(mac, "%s", "BA2401000001");

	timestamp = get_timestamp();
	sprintf(ts,"%lld", timestamp);
	tcplog("create_data_body ts %s\r\n",ts);
	
	sprintf(tmp,"%s", mac);
	reverse_str(tmp, 12, key);
	tcplog("create_data_body mac %s\r\n", mac);
	tcplog("create_data_body key %s\r\n", key);

	sprintf(tmp,"%s%s%s%s%s%s","key", key, "mac", mac, "ts",ts);

	tcplog("sign input  %s\r\n", tmp);

	sha256((const unsigned char *)tmp, strlen(tmp), (unsigned char *)sign_tmp);
	str2hex(sign, sign_tmp, 32);
	tcplog("\r\nsign sha256 %s \r\n", sign);
	
	pos += sprintf(&buf[pos], "{");
	pos += sprintf(&buf[pos], "\"sign\":\"%s\",", sign);
	pos += sprintf(&buf[pos], "\"mac\":\"%s\",", mac);
	pos += sprintf(&buf[pos], "\"key\":\"%s\",", key);
	pos += sprintf(&buf[pos], "\"ts\":%s", ts);
	pos += sprintf(&buf[pos], "}");

	tcplog("create_data_body buf %s \r\n", buf);
	return pos;
}

void InvertUint16(unsigned short *dBuf,unsigned short *srcBuf)
{
	int i;
	unsigned short tmp[4]={0};
 
	for(i=0;i< 16;i++)
	{
		if(srcBuf[0]& (1 << i))
		tmp[0]|=1<<(15 - i);
	}
	dBuf[0] = tmp[0];
}

unsigned short CRC16_CCITT(unsigned char *data, unsigned int datalen)
{
	unsigned short wCRCin = 0x0000;
	unsigned short wCPoly = 0x1021;
	InvertUint16(&wCPoly,&wCPoly);
	while (datalen--) 	
	{
		wCRCin ^= *(data++);
		for(int i = 0;i < 8;i++)
		{
			if(wCRCin & 0x01)
				wCRCin = (wCRCin >> 1) ^ wCPoly;
			else
				wCRCin = wCRCin >> 1;
		}
	}
	return (wCRCin);
}

uint16_t CRC_Check(uint8_t *CRC_Ptr, uint8_t LEN)
{
    uint16_t CRC_Value = 0xffff;
    uint8_t  i         = 0;
    uint8_t  j         = 0;

    for(i = 0; i < LEN; i++)  //LEN为数组长度
    {
        CRC_Value ^= *(CRC_Ptr + i);
        for(j = 0; j < 8; j++)
        {
            if(CRC_Value & 0x00001)
                CRC_Value = (CRC_Value >> 1) ^ 0xA001;
            else
                CRC_Value = (CRC_Value >> 1);
        }
    }
    CRC_Value = ((CRC_Value >> 8) +  (CRC_Value << 8)); //交换高低字节

    return CRC_Value;
}

unsigned short CRC16_MODBUS(unsigned char *ptr, int len)
{
    unsigned int i;
    unsigned short crc = 0xFFFF;
    
    while(len--)
    {
        crc ^= *ptr++;
        for (i = 0; i < 8; ++i)
        {
            if (crc & 1){
                crc = (crc >> 1) ^ 0xA001;
            }
            else{
                crc = (crc >> 1);
            }
        }
    }
	crc = ((crc >> 8) +  (crc << 8)); //交换高低字节
    return crc;
}

int create_package(int type, int frame_num, char * buf)
{
	char data_body[200] = {0};
	short crc;
	
	int len = create_data_body(data_body);
	
	buf[FRAME_HEAD] = 0x5A;
	buf[DATA_LEN_H] = (len + 3) / 256;
	buf[DATA_LEN_L] = (len + 3) % 256;
	buf[FRAME_NUM]  = frame_num;
	buf[VERSION] = version;
	
	switch(type){
		case timing:		buf[EVENT_TYPE] = 0x01; 	break;
		case auth:			buf[EVENT_TYPE] = 0x02; 	break;
		case heart_beat:	buf[EVENT_TYPE] = 0x03; 	break;
		default : 			buf[EVENT_TYPE] = 0xff;		break;
	}
	
	memcpy(buf + DATA_START, data_body, len);
	crc = CRC16_MODBUS((unsigned char *)buf + 3, len + 3);
	tcplog("\r\n%04X   ", crc);
	
	buf[len + DATA_START] = (crc >> 8) & 0xFF;
	buf[len + DATA_START + 1] = crc & 0xFF;
	
	return len + DATA_START + 2;
}

int tcp_connect_server(int type, int frame_num)
{   
	char buf[300] = {0};
	
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1){
		tcplog("socket error\r\n");
		return -1;
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	
//	test platform
//	server_addr.sin_addr.s_addr = inet_addr("120.196.100.12");
//	server_addr.sin_port = htons(12113);

	server_addr.sin_addr.s_addr = inet_addr("49.234.245.231");
	server_addr.sin_port = htons(8301);

	int ret = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(ret < 0){
		tcplog("connect error\r\n");
		return -1;
	}
	
	int len = create_package(type, frame_num, buf);
	int i = 0, j;
#if 0	
	for(i = 0; i < len; i ++){
		printf("%02X ", buf[i]);
		if(i % 16 == 15){
			printf("\r\n");
		}
	}
#endif
	ret = send(sockfd, buf, len, 0);
	if(ret < 0){
		tcplog("send error\r\n");
		return -1;
	}

	tcplog("send ------ recv \r\n");
	memset(buf, 0, 300);
	
    ret = recv(sockfd, buf, 300, 0);
	if(ret <= 0){
		tcplog("recv error\r\n");
		return -1;
	}
#if 0
 	for(i = 0; i < ret; i ++){
		printf("%02X ", buf[i]);
		if(i % 16 == 15){
			printf("\r\n");
		}
	}
#endif
	char tmp[300] = {0};
	for(i = 0, j = 6; j < ret ; i ++, j++){
		tmp[i] = buf[j];
	}
	printf("\r\nrecv %s\r\n", tmp);
	
	tcp_json_parse(tmp);
	
	tcplog("code %d \r\nmessage %s \r\ndata {\r\ncom_code %d\r\n", tcp_json.code,tcp_json.message,tcp_json.data.comm_server_code);
	tcplog("auth %s \r\nbroker %s \r\nauth_key %s\r\ncom_key %s\r\n}", tcp_json.data.auth,tcp_json.data.broker,tcp_json.data.auth_sec_key, tcp_json.data.comm_sec_key);

	close(sockfd);
	
    return 0;
}
  
  



