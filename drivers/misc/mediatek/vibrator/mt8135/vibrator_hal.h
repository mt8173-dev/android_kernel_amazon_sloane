/*********************************************
HAL API
**********************************************/

void vibr_Enable_HW(void);
void vibr_Disable_HW(void);
void vibr_power_set(void);
struct vibrator_hw *mt_get_cust_vibrator_hw(void);

extern S32 pwrap_read(U32 adr, U32 *rdata);
extern S32 pwrap_write(U32 adr, U32 wdata);
extern void dct_pmic_VIBR_enable(kal_bool dctEnable);
