/*
**  CRC.H - header file for SNIPPETS CRC and checksum functions
*/

#ifndef CRC__H
#define CRC__H

#include <stdlib.h>           /* For size_t                 */

typedef enum {Error_ = -1, Success_, False_ = 0, True_} Boolean_T;
typedef unsigned char BYTE;
typedef unsigned long DWORD;
typedef unsigned short WORD;

#define UPDC32(octet,crc) (crc_32_tab[((crc)\
     ^ ((BYTE)octet)) & 0xff] ^ ((crc) >> 8))

DWORD updateCRC32(unsigned char ch, DWORD crc);
DWORD crc32(char *buf, size_t len);

#endif /* CRC__H */
