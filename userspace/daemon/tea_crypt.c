
#if defined(NM_TEA_CRYPT)
static unsigned int tea_key[4] = {0x5a5adead, 0x5a5adead, 0x55aadead, 0x55aadead};
/*
 * 参数：
 * 	val--8字节的明文输入，加密后替换为8字节密文
 * 	key--16字节的密钥
 **/
static void tea_encrypt(unsigned int*val, unsigned int *key)
{
	unsigned int left=val[0], right=val[1];
	unsigned int sum=0, i;
	unsigned int delta=0x9e3779b9; /* a key schedule constant */
	unsigned int a=key[0], b=key[1], c=key[2], d=key[3];

	for(i=0;i<32;i++){
		sum += delta;
		left += ((right<<4)+a)^(right+sum)^((right>>5)+b);
		right += ((left<<4)+c)^(left+sum)^((left>>5)+d);
	}

	val[0]=left;
	val[1]=right;

	return;
}

/*
 * 参数：
 * 	val--8字节密文输入，解密后替换为8字节明文
 * 	key--16字节密钥
 **/
static void tea_decrypt(unsigned int *val, unsigned int *key)
{
	unsigned int y=val[0], z=val[1], sum=0xC6EF3720, i;
	unsigned int delta=0x9e3779b9;
	unsigned int a=key[0], b=key[1], c=key[2], d=key[3];

	for(i=0;i<32;i++){
		z -= ((y<<4)+c)^(y+sum)^((y>>5)+d);
		y -= ((z<<4)+a)^(z+sum)^((z>>5)+b);
		sum -= delta;
	}
	val[0]=y;
	val[1]=z;

	return;
}
#endif

/*
 * val 为待加密的数据
 * len 为数据长度，单位Byte
 * */
int nm_encrypt(unsigned int *val, int len)
{
	int i=0;
#if defined(NM_TEA_CRYPT)
	int crypt_len = ((len&7)?(len+8-(len&7)):len)/sizeof(unsigned int);
	for(i=0;i<crypt_len;i+=2){
		tea_encrypt(val+i, tea_key);
	}
	return crypt_len*sizeof(unsigned int);
#else
	unsigned char *ptr=(unsigned char *)val;
	for(i=0;i<len;i++){
		if(*(ptr+i) & 0x80){
			*(ptr+i) &= 0x7f;
		}else{
			*(ptr+i) |= 0x80;
		}
	}
	return len;
#endif
}

/*
 * val 为待解密的数据
 * len 为数据长度，单位Byte
 * */

int nm_decrypt(unsigned int *val, int len)
{
	int i=0;
#if defined(NM_TEA_CRYPT)
	int crypt_len = len/sizeof(unsigned int);
	for(i=0;i<crypt_len;i+=2){
		tea_decrypt(val+i, tea_key);
	}

	return crypt_len*sizeof(unsigned int);
#else
	unsigned char *ptr=(unsigned char *)val;
	for(i=0;i<len;i++){
		if(*(ptr+i) & 0x80){
			*(ptr+i) &= 0x7f;
		}else{
			*(ptr+i) |= 0x80;
		}
	}
	return len;
#endif
}
