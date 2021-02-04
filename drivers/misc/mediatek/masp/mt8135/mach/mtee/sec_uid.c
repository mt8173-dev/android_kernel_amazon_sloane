#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "tz_cross/ta_hacc.h"
#include "../hacc_mach.h"
#include "tz_cross/trustzone.h"
#include "kree/system.h"

#define MOD                         "TEE"

int masp_hal_get_uuid(unsigned int *uuid)
{
    TZ_RESULT ret;
	KREE_SESSION_HANDLE cryptoSession;
	MTEEC_PARAM param[4];
	uint32_t paramTypes;
	/* uint32_t result; */
	unsigned int *outBuf = 0;
	unsigned int size;

	pr_info("__FILE__: %s\n", __FILE__);
	pr_info("__func__: %s\n", __func__);
	pr_info("__LINE__: %d\n", __LINE__);

	/* Bind Crypto test section with TZ_CRYPTO_TA_UUID */
	ret = KREE_CreateSession(TZ_CRYPTO_TA_UUID, &cryptoSession);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("CreateSession error 0x%x\n", ret);
		goto _err;
	}

	paramTypes = TZ_ParamTypes1(TZPT_MEM_OUTPUT);

	/* Out buffer */
	size = sizeof(unsigned int) * 4;
	outBuf = (unsigned int *) kmalloc(size, GFP_KERNEL);
	if (!outBuf) {
		pr_info("kmalloc outBuf error\n");
		goto _err;
	}

    param[0].mem.buffer = outBuf;
    param[0].mem.size = size;

	pr_info("Start to do service call - TZCMD_GET_UUID\n");
	ret = KREE_TeeServiceCall(cryptoSession, TZCMD_GET_UUID, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("ServiceCall error %d\n", ret);
		goto _err;
	}
	memcpy(uuid, param[0].mem.buffer, size);

	pr_info("kfree outBuf\n");
	if (outBuf) {
		kfree(outBuf);
	}

	pr_info("Start to close session\n");
	/* Close session of TA_Crypto_TEST */
	ret = KREE_CloseSession(cryptoSession);

	return 0;

	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("CloseSession error %d\n", ret);
		goto _err;
	}

 _err:
	printk("[%s] HACC Fail (0x%x)\n", MOD, ret);
	if (outBuf) {
		kfree(outBuf);
	}
	ASSERT(0);

    return -1;
}