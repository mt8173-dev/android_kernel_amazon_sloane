#ifndef _KD_FLASHLIGHTLIST_H_
#define _KD_FLASHLIGHTLIST_H_

/* export funtions */
MUINT32 defaultFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 dummyFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 peakFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 torchFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);

int strobe_getPartId(int sensorDev);
MUINT32 subStrobeInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc);
/* MUINT32 subStrobeInit_2ndPart_2(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc); */
/* MUINT32 mainStrobeInit_2ndPart_2(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc); */

#endif
