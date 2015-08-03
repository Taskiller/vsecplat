#include <stdio.h>
#include <string.h>
#include "tea_crypt.h"
union c{
	int a;
	unsigned char b[4];
};
int main(void)
{
	unsigned char buf[16];
	union c c;
	c.a = 0x5a5adead;
	printf("byte[0]=%x, byte[1]=%x, byte[2]=%x, byte[3]=%x\n",
			c.b[0], c.b[1], c.b[2], c.b[3]);

	memset(buf, 0, 16*sizeof(char));
	strcpy(buf, "AAAAAAAA");

	printf("orig buf: %s\n", buf);
	nm_encrypt((unsigned int *)buf, 8);
	printf("encrypt buf: %x %x %x %x %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

	nm_decrypt((unsigned int *)buf, 8);
	printf("decrypt buf: %c %c %c %c %c %c %c %c\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

	return 0;
}
