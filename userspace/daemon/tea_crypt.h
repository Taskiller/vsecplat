#ifndef __TEA_CRYPT_H__
#define __TEA_CRYPT_H__

void tea_encrypt(unsigned long *val, unsigned long *key);
void tea_decrypt(unsigned long *val, unsigned long *key);

#endif
