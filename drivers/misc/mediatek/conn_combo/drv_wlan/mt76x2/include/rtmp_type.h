/*
 ***************************************************************************
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
 ***************************************************************************

    Module Name:
    rtmp_type.h

    Abstract:

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
    Name        Date            Modification logs
    Paul Lin    1-2-2004
*/

#ifndef __RTMP_TYPE_H__
#define __RTMP_TYPE_H__



#ifndef GNU_PACKED
#define GNU_PACKED  __attribute__ ((packed))
#endif /* GNU_PACKED */


#ifdef LINUX
/* Put platform dependent declaration here */
/* For example, linux type definition */
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;
typedef short INT16;
typedef int INT32;
typedef long long INT64;

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int UINT;
typedef unsigned long ULONG;
#endif /* LINUX */

typedef unsigned char *PUINT8;
typedef unsigned short *PUINT16;
typedef unsigned int *PUINT32;
typedef unsigned long long *PUINT64;
typedef int *PINT32;
typedef long long *PINT64;

/* modified for fixing compile warning on Sigma 8634 platform */
typedef char STRING;

typedef signed char CHAR;

typedef signed short SHORT;
typedef signed int INT;
typedef signed long LONG;
typedef signed long long LONGLONG;

typedef unsigned long long ULONGLONG;

typedef unsigned char BOOLEAN;
#ifdef LINUX
typedef void VOID;
#endif /* LINUX */

typedef char *PSTRING;
typedef VOID * PVOID;
typedef CHAR * PCHAR;
typedef UCHAR * PUCHAR;
typedef USHORT * PUSHORT;
typedef LONG * PLONG;
typedef ULONG * PULONG;
typedef UINT * PUINT;

typedef unsigned int NDIS_MEDIA_STATE;

typedef union _LARGE_INTEGER {
	struct {
#ifdef RT_BIG_ENDIAN
		INT32 HighPart;
		UINT LowPart;
#else
		UINT LowPart;
		INT32 HighPart;
#endif				/* endif */
	} u;
	INT64 QuadPart;
} LARGE_INTEGER;

/* Register set pair for initialzation register set definition */
typedef struct _RTMP_REG_PAIR {
	UINT32 Register;
	UINT32 Value;
} RTMP_REG_PAIR, *PRTMP_REG_PAIR;

typedef struct _REG_PAIR {
	UCHAR Register;
	UCHAR Value;
} REG_PAIR, *PREG_PAIR;

typedef struct _REG_PAIR_CHANNEL {
	UCHAR Register;
	UCHAR FirstChannel;
	UCHAR LastChannel;
	UCHAR Value;
} REG_PAIR_CHANNEL, *PREG_PAIR_CHANNEL;

typedef struct _REG_PAIR_BW {
	UCHAR Register;
	UCHAR BW;
	UCHAR Value;
} REG_PAIR_BW, *PREG_PAIR_BW;

typedef struct _REG_PAIR_PHY {
	UCHAR reg;
	UCHAR s_ch;
	UCHAR e_ch;
	UCHAR phy;		/* RF_MODE_XXX */
	UCHAR bw;		/* RF_BW_XX */
	UCHAR val;
} REG_PAIR_PHY;

/* Register set pair for initialzation register set definition */
typedef struct _RTMP_RF_REGS {
	UCHAR Channel;
	UINT32 R1;
	UINT32 R2;
	UINT32 R3;
	UINT32 R4;
} RTMP_RF_REGS, *PRTMP_RF_REGS;

typedef struct _FREQUENCY_ITEM {
	UCHAR Channel;
	UCHAR N;
	UCHAR R;
	UCHAR K;
} FREQUENCY_ITEM, *PFREQUENCY_ITEM;

typedef int NTSTATUS;

#define STATUS_SUCCESS			0x00
#define STATUS_UNSUCCESSFUL		0x01

typedef struct _QUEUE_ENTRY {
	struct _QUEUE_ENTRY *Next;
} QUEUE_ENTRY, *PQUEUE_ENTRY;

/* Queue structure */
typedef struct _QUEUE_HEADER {
	PQUEUE_ENTRY Head;
	PQUEUE_ENTRY Tail;
	UINT Number;
} QUEUE_HEADER, *PQUEUE_HEADER;

typedef struct _CR_REG {
	UINT32 flags;
	UINT32 offset;
	UINT32 value;
} CR_REG, *PCR_REG;

typedef struct _BANK_RF_CR_REG {
	UINT32 flags;
	UCHAR bank;
	UCHAR offset;
	UCHAR value;
} BANK_RF_CR_REG, *PBANK_RF_CR_REG;

struct mt_dev_priv {
	void *sys_handle;
	void *wifi_dev;
	unsigned long priv_flags;
	UCHAR sniffer_mode;
};

#endif /* __RTMP_TYPE_H__ */
