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
}

int nm_encrypt(unsigned int *val, int len)
{
	int i=0;
	len=(len&7)?(len+8-(len&7)):len;
	for(i=0;i<len;i+=2){
		tea_encrypt(val+i, tea_key);
	}
	return len;
}

int nm_decrypt(unsigned int *val, int len)
{
	int i=0;
	for(i=0;i<len;i+=2){
		tea_decrypt(val+i, tea_key);
	}

	return len;
}
