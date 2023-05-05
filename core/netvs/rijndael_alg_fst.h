#ifndef __RIJNDAEL_ALG_FST_H
#define __RIJNDAEL_ALG_FST_H

#define MAXKC (256/32)
#define MAXKB (256/8)
#define MAXNR 14
typedef unsigned char u8; 
typedef unsigned short u16; 
typedef unsigned int u32;
int rijndaelKeySetupEnc(u32 rk[], const u8 cipherKey[], int keyBits);
int rijndaelKeySetupDec(u32 rk[], const u8 cipherKey[], int keyBits);
void rijndaelEncrypt(const u32 rk[], int Nr, const u8 pt[16], u8 ct[16]);
void rijndaelDecrypt(const u32 rk[], int Nr, const u8 ct[16], u8 pt[16]);
#ifdef INTERMEDIATE_VALUE_KAT
void rijndaelEncryptRound(const u32 rk[], int Nr, u8 block[16], int rounds);
void rijndaelDecryptRound(const u32 rk[], int Nr, u8 block[16], int rounds);
#endif

#endif
