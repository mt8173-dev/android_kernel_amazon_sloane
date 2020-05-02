
#ifndef _MT_BATTERY_THROTTLE_
#define _MT_BATTERY_THROTTLE_

extern bool mtk_get_gpu_power_loading(unsigned int *pLoading);
extern bool upmu_is_chr_det(void);

#define BATTERY_MAX_BUDGET  (-1)
#define BATTERY_MIN_BUDGET  430
#define BATTERY_MAX_BUDGET_FACTOR  10
#define BATTERY_MIN_BUDGET_FACTOR  0
#define BATTERY_BUDGET_MIN_VOLTAGE 3450
#define BATTERY_BUDGET_TOLERANCE_VOLTAGE 50
#define BATTERY_LIMITOR 1
#define VBAT_LOWER_BOUND 3100
#define WORK_INTERVAL 10000

enum throttle_mode {
	TH_NORMAL = 0,
	TH_BUDGET = 1,
	TH_DISABLE = 2,
};

#endif
