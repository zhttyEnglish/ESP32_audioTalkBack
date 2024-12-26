/**
 * \file sm4.h
 */
#ifndef XYSSL_SM4_H
#define XYSSL_SM4_H
 
#ifdef __cplusplus
extern "C" {
#endif
 
#define SM4_ENCRYPT     1
#define SM4_DECRYPT     0
 
/**
 * \brief          SM4上下文结构
 */
typedef struct
{
    int mode;                   /*!<  encrypt/decrypt  加密解密  */
    unsigned long sk[32];       /*!<  SM4 subkeys     SM4关键字  */
}sm4_context;
 
 
/**
 * \brief          SM4 key schedule (128-bit, encryption) SM4密钥调度(128位，加密)
 *
 * \param ctx      SM4 context to be initialized        SM4上下文
 * \param key      16-byte secret key       16字节密钥
 */
void sm4_setkey_enc( sm4_context *ctx, unsigned char key[16] );
/**
 * \brief          SM4 key schedule (128-bit, decryption) SM4密钥调度(128位，解密)
 *
 * \param ctx      SM4 context to be initialized        SM4上下文
 * \param key      16-byte secret key       16字节密钥
 */
void sm4_setkey_dec( sm4_context *ctx, unsigned char key[16] );
/**
 * \brief          SM4-ECB block encryption/decryption   SM4-ECB 块加密/解密
 * \param ctx      SM4 context to be used               SM4上下文
 * \param mode     SM4_ENCRYPT or SM4_DECRYPT        
 * \param length   length of the input data              输入数据的长度
 * \param input    input block      输入块
 * \param output   output block     输出块
 */
int sm4_crypt_ecb( sm4_context *ctx,
				     int mode,
					 int length,
                     unsigned char *input,
                     unsigned char *output);

void sm4_crypt_cbc( sm4_context *ctx,
					 int mode,
					 int length,
					 unsigned char iv[16],
					 unsigned char *input,
					 unsigned char *output );

#ifdef __cplusplus
}
#endif
 
#endif /* sm4.h */
 
