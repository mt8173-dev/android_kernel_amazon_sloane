/* use accdet + EINT solution */
#define ACCDET_EINT
/* support multi_key feature */
#define ACCDET_MULTI_KEY_FEATURE
/* after 5s disable accdet */
#define ACCDET_LOW_POWER

/* #define ACCDET_PIN_RECOGNIZATION */
#define ACCDET_28V_MODE

#define ACCDET_SHORT_PLUGOUT_DEBOUNCE
#define ACCDET_SHORT_PLUGOUT_DEBOUNCE_CN 20
#define ACCDET_MULTI_KEY_ONLY_FOR_VOLUME
struct headset_mode_settings {
	int pwm_width;		/* pwm frequence */
	int pwm_thresh;		/* pwm duty */
	int fall_delay;		/* falling stable time */
	int rise_delay;		/* rising stable time */
	int debounce0;		/* hook switch or double check debounce */
	int debounce1;		/* mic bias debounce */
	int debounce3;		/* plug out debounce */
};

/* key press customization: long press time */
struct headset_key_custom {
	int headset_long_press_time;
};
struct headset_mode_data {
	unsigned int cust_eint_accdet_num;
	unsigned int cust_eint_accdet_debounce_cn;
	unsigned int cust_eint_accdet_type;
};
