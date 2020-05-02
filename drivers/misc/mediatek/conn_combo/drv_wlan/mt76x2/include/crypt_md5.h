/****************************************************************************
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 ***************************************************************************/

/****************************************************************************
    Module Name:
    MD5

    Abstract:
    RFC1321: The MD5 Message-Digest Algorithm

    Revision History:
    Who         When            What
    --------    ----------      ------------------------------------------
    Eddy        2008/11/24      Create md5
***************************************************************************/

#ifndef __CRYPT_MD5_H__
#define __CRYPT_MD5_H__


/* Algorithm options */
#define MD5_SUPPORT

#ifdef MD5_SUPPORT
#define MD5_BLOCK_SIZE    64	/* 512 bits = 64 bytes */
#define MD5_DIGEST_SIZE   16	/* 128 bits = 16 bytes */
typedef struct {
	UINT32 HashValue[4];
	UINT64 MessageLen;
	UINT8 Block[MD5_BLOCK_SIZE];
	UINT BlockLen;
} MD5_CTX_STRUC, *PMD5_CTX_STRUC;

VOID RT_MD5_Init(IN MD5_CTX_STRUC * pMD5_CTX);
VOID RT_MD5_Hash(IN MD5_CTX_STRUC * pMD5_CTX);
VOID RT_MD5_Append(IN MD5_CTX_STRUC * pMD5_CTX, IN const UINT8 Message[], IN UINT MessageLen);
VOID RT_MD5_End(IN MD5_CTX_STRUC * pMD5_CTX, OUT UINT8 DigestMessage[]);
VOID RT_MD5(IN const UINT8 Message[], IN UINT MessageLen, OUT UINT8 DigestMessage[]);
#endif /* MD5_SUPPORT */


#endif /* __CRYPT_MD5_H__ */
