#define NM_TEA_KEY 0x55aadeadaa55dead

/*
 * 参数：
 * 	val--8字节的明文输入，加密后替换为8字节密文
 * 	key--16字节的密钥
 **/
static void tea_encrypt(unsigned long *val, unsigned long *key)
{
	unsigned long left=val[0], right=val[1];
   	unsigned long sum=0, i;
	unsigned long delta=0x9e3779b9; /* a key schedule constant */
	unsigned long a=key[0], b=key[1], c=key[2], d=key[3];

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
static void tea_decrypt(unsigned long *val, unsigned long *key)
{
	unsigned long y=val[0], z=val[1], sum=0xC6EF3720, i;
	unsigned long delta=0x9e3779b9;
	unsigned long a=key[0], b=key[1], c=key[2], d=key[3];

	for(i=0;i<32;i++){
		z -= ((y<<4)+c)^(y+sum)^((y>>5)+d);
		y -= ((z<<4)+a)^(z+sum)^((z>>5)+b);
		sum -= delta;
	}
	val[0]=y;
	val[1]=z;
}

int nm_encrypt(unsigned long *val, int len)
{
	return len;
}

int nm_decrypt(unsigned long *val, int len)
{
	return len;
}
