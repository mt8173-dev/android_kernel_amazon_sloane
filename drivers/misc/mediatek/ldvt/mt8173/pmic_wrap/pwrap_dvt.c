#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <generated/autoconf.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/smp.h>

#include <mach/mt_typedefs.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/random.h>
#include <asm/system.h>
#include <linux/clk.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif


#define RAND_MAX 20000000

#include <drivers/misc/mediatek/pmic_wrap/mt8173/pwrap_hal.h>
#include <mach/mt_pmic_wrap.h>
#include "register_rw_tbl.h"
#include "pwrap_dvt.h"

static struct mt_pmic_wrap_driver *mt_wrp;
struct mt_pwrap_dvt_t {
	U32 irq_mode;
	spinlock_t wacs0_lock;
	spinlock_t wacs1_lock;
	void (*complete) (void *context);
	void *context;
};
enum pwrap_irq_mode {
	NORMAL_TEST,
	WDT_TEST,
	INT_TEST,
};
enum pwrap_test_case {
	PWRAP_DVT_UNSUPPORTED = -1,
	INIT,
	ACCESS,
	STATUS_UPDATE,
	DUAL_IO,
	REG_RW,
	MUX_SWITCH,
	SOFT_RESET,
	HIGH_PRI,
	ENCRYPTION,
	WDT,
	INTERRUPT,
	CONCURRENCE,
	CLK_MANAGER,
	SINGLE_IO,
	RESET_PATTERN,
	WDT_1,
	WDT_2,
	WDT_3,
	WDT_4,
	CLK_GATING,
	INFRASYS_PMICSPI,
	INFRASYS_PWRAP,
	PWRAP_DVT_MAX,
};
static struct mt_pwrap_dvt_t mt_wrp_dvt_obj = {
	.irq_mode = NORMAL_TEST,
	.wacs0_lock = __SPIN_LOCK_UNLOCKED(lock),
	.wacs1_lock = __SPIN_LOCK_UNLOCKED(lock),
};

static struct mt_pwrap_dvt_t *mt_wrp_dvt = &mt_wrp_dvt_obj;
/*-----start-- global variable-------------------------------------------------*/
DECLARE_COMPLETION(pwrap_done);


#define WRAP_ACCESS_TEST_REG DEW_WRITE_TEST
#define ldvt_follow_up
#define DEBUG_LDVT

/*-----macro for dewrapper defaule value----------------------------------------*/
#define DEFAULT_VALUE_READ_TEST      0x5aa5
#define WRITE_TEST_VALUE             0xa55a

/*-----macro for manual commnd -------------------------------------------------*/
/* SLAVE Select, 0: 6323/6397 */
#define OP_PMIC_SEL                 (0x0)
/* SLAVE Select, 1: new PMIC */
#define OP_PMIC_SEL_NEW             (0x1)

#define IO_PHYS (0xF0000000)
#define APMCU_GPTIMER_BASE  (IO_PHYS + 0x00008000)

#define MAX_USER_RT_PRIO	100
#define MAX_RT_PRIO		MAX_USER_RT_PRIO

U32 eint_in_cpu0 = 0;
U32 eint_in_cpu1 = 2;

/*-pwrap debug--------------------------------------------------------------------------*/
static inline void pwrap_dump_ap_register(void)
{
	U32 i = 0;

	PWRAPREG("dump pwrap register, base=0x%x\n", PMIC_WRAP_BASE);
	PWRAPREG("address     :   3 2 1 0    7 6 5 4    B A 9 8    F E D C\n");
	for (i = 0; i <= 0x150; i += 16) {
		PWRAPREG("offset 0x%.3x:0x%.8x 0x%.8x 0x%.8x 0x%.8x\n", i,
			 WRAP_RD32(PMIC_WRAP_BASE + i + 0),
			 WRAP_RD32(PMIC_WRAP_BASE + i + 4),
			 WRAP_RD32(PMIC_WRAP_BASE + i + 8), WRAP_RD32(PMIC_WRAP_BASE + i + 12));
	}
	return;
}

static inline void pwrap_dump_pmic_register(void)
{
#if 0
	U32 i = 0;
	U32 reg_addr = 0;
	U32 reg_value = 0;

	PWRAPREG("dump dewrap register\n");
	for (i = 0; i <= 14; i++) {
		reg_addr = (DEW_BASE + i * 4);
		reg_value = pwrap_read_nochk(reg_addr, &reg_value);
		PWRAPREG("0x%x=0x%x\n", reg_addr, reg_value);
	}
#endif
	return;
}

static inline void pwrap_dump_all_register(void)
{
	pwrap_dump_ap_register();
	pwrap_dump_pmic_register();
	return;
}

static void __pwrap_soft_reset(void)
{
	PWRAPLOG("start reset wrapper\n");
	PWRAP_SOFT_RESET;
	PWRAPLOG("the reset register =%x\n", WRAP_RD32(INFRA_GLOBALCON_RST0));
	PWRAPLOG("PMIC_WRAP_STAUPD_GRPEN =0x%x,it should be equal to 0x1\n",
		 WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN));
	PWRAP_CLEAR_SOFT_RESET_BIT;
	return;
}

/******************************************************************************
  wrapper timeout
 ******************************************************************************/
#define PWRAP_TIMEOUT
#ifdef PWRAP_TIMEOUT
#include <mach/mt_gpt.h>
static U64 _pwrap_get_current_time(void)
{
	return sched_clock();
}

static BOOL _pwrap_timeout_ns(U64 start_time_ns, U64 timeout_time_ns)
{
	U64 cur_time = 0;
	U64 elapse_time = 0;

	/* get current tick */
	cur_time = _pwrap_get_current_time();
	/* avoid timer over flow exiting in FPGA env */
	if (cur_time < start_time_ns) {
		PWRAPERR("@@@@Timer overflow! start%lld cur timer%lld\n", start_time_ns, cur_time);
		start_time_ns = cur_time;
		timeout_time_ns = 255 * 1000;
		PWRAPERR("@@@@reset timer! start%lld setting%lld\n", start_time_ns,
			 timeout_time_ns);
	}

	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time) {
		PWRAPERR("@@@@Timeout: elapse time%lld,start%lld setting timer%lld\n",
			 elapse_time, start_time_ns, timeout_time_ns);
		return TRUE;
	}
	return FALSE;
}

static U64 _pwrap_time2ns(U64 time_us)
{
	return time_us * 1000;
}

#else
static U64 _pwrap_get_current_time(void)
{
	return 0;
}

static BOOL _pwrap_timeout_ns(U64 start_time_ns, U64 elapse_time)
{
	return FALSE;
}

static U64 _pwrap_time2ns(U64 time_us)
{
	return 0;
}

#endif
/*#####################################################################
//define macro and inline function (for do while loop)
//#####################################################################*/

/*  define a function pointer   */
typedef U32(*loop_condition_fp) (U32);

static inline U32 wait_for_fsm_idle(U32 x)
{
	return GET_WACS0_FSM(x) != WACS_FSM_IDLE;
}

static inline U32 wait_for_fsm_vldclr(U32 x)
{
	return GET_WACS0_FSM(x) != WACS_FSM_WFVLDCLR;
}

static inline U32 wait_for_sync(U32 x)
{
	return GET_SYNC_IDLE0(x) != WACS_SYNC_IDLE;
}

static inline U32 wait_for_idle_and_sync(U32 x)
{
	return (GET_WACS2_FSM(x) != WACS_FSM_IDLE) || (GET_SYNC_IDLE2(x) != WACS_SYNC_IDLE);
}

static inline U32 wait_for_wrap_idle(U32 x)
{
	return (GET_WRAP_FSM(x) != 0x0) || (GET_WRAP_CH_DLE_RESTCNT(x) != 0x0);
}

static inline U32 wait_for_wrap_state_idle(U32 x)
{
	return GET_WRAP_AG_DLE_RESTCNT(x) != 0;
}

static inline U32 wait_for_man_idle_and_noreq(U32 x)
{
	return (GET_MAN_REQ(x) != MAN_FSM_NO_REQ) || (GET_MAN_FSM(x) != MAN_FSM_IDLE);
}

static inline U32 wait_for_man_vldclr(U32 x)
{
	return GET_MAN_FSM(x) != MAN_FSM_WFVLDCLR;
}

static inline U32 wait_for_cipher_ready(U32 x)
{
	return x != 3;
}

static inline U32 wait_for_stdupd_idle(U32 x)
{
	return GET_STAUPD_FSM(x) != 0x0;
}

static inline U32 wait_for_state_ready_init(loop_condition_fp fp, U32 timeout_us, U64 wacs_register,
					    U64 *read_reg)
{

	U64 start_time_ns = 0, timeout_ns = 0;
	U32 reg_rdata = 0x0;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait_for_state_ready_init timeout when waiting for idle\n");
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

static inline U32 wait_for_state_idle_init(loop_condition_fp fp, U32 timeout_us, U64 wacs_register,
					   U64 wacs_vldclr_register, U64 *read_reg)
{

	U64 start_time_ns = 0, timeout_ns = 0;
	U32 reg_rdata;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait_for_state_idle_init timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		/* if last read command timeout,clear vldclr bit */
		/* read command state machine:FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle */
		switch (GET_WACS0_FSM(reg_rdata)) {
		case WACS_FSM_WFVLDCLR:
			WRAP_WR32(wacs_vldclr_register, 1);
			PWRAPERR("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
			break;
		case WACS_FSM_WFDLE:
			PWRAPERR("WACS_FSM = WACS_FSM_WFDLE\n");
			break;
		case WACS_FSM_REQ:
			PWRAPERR("WACS_FSM = WACS_FSM_REQ\n");
			break;
		default:
			break;
		}
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

static inline U32 wait_for_state_idle(loop_condition_fp fp, U32 timeout_us, U64 wacs_register,
				      U64 wacs_vldclr_register, U64 *read_reg)
{

	U64 start_time_ns = 0, timeout_ns = 0;
	U32 reg_rdata;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait_for_state_idle timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		if (GET_INIT_DONE0(reg_rdata) != WACS_INIT_DONE) {
			PWRAPERR("initialization isn't finished\n");
			return E_PWR_NOT_INIT_DONE;
		}
		/*if last read command timeout,clear vldclr bit */
		/*read command state machine:FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle */
		switch (GET_WACS0_FSM(reg_rdata)) {
		case WACS_FSM_WFVLDCLR:
			WRAP_WR32(wacs_vldclr_register, 1);
			PWRAPERR("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
			break;
		case WACS_FSM_WFDLE:
			PWRAPERR("WACS_FSM = WACS_FSM_WFDLE\n");
			break;
		case WACS_FSM_REQ:
			PWRAPERR("WACS_FSM = WACS_FSM_REQ\n");
			break;
		default:
			break;
		}
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

static inline U32 wait_for_state_ready(loop_condition_fp fp, U32 timeout_us, U64 wacs_register,
				       U64 *read_reg)
{
	U64 start_time_ns = 0, timeout_ns = 0;
	U32 reg_rdata;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);

		if (GET_INIT_DONE0(reg_rdata) != WACS_INIT_DONE) {
			PWRAPERR("initialization isn't finished\n");
			return E_PWR_NOT_INIT_DONE;
		}
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

static void pwrap_delay_us(U32 us)
{
	mdelay(us / 1000);
}

static inline void pwrap_complete(void *arg)
{
	complete(arg);
}

/*--------------------------------------------------------
//    Function : pwrap_wacs0()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
S32 pwrap_wacs0(U32 write, U32 adr, U32 wdata, U32 *rdata)
{
	U32 reg_rdata = 0;
	U32 wacs_write = 0;
	U32 wacs_adr = 0;
	U32 wacs_cmd = 0;
	U32 return_value = 0;
	unsigned long flags = 0;

	/* Check argument validation */
	if ((write & ~(0x1)) != 0)
		return E_PWR_INVALID_RW;
	if ((adr & ~(0xffff)) != 0)
		return E_PWR_INVALID_ADDR;
	if ((wdata & ~(0xffff)) != 0)
		return E_PWR_INVALID_WDAT;

	spin_lock_irqsave(&mt_wrp_dvt->wacs0_lock, flags);
	/* Check IDLE & INIT_DONE in advance */
	return_value =
	    wait_for_state_idle(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS0_RDATA,
				PMIC_WRAP_WACS0_VLDCLR, 0);
	if (return_value != 0) {
		PWRAPERR("wait_for_fsm_idle fail,return_value=%d\n", return_value);
		goto FAIL;
	}

	wacs_write = write << 31;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;

	WRAP_WR32(PMIC_WRAP_WACS0_CMD, wacs_cmd);
	if (write == 0) {
		if (NULL == rdata) {
			PWRAPERR("rdata is a NULL pointer\n");
			return_value = E_PWR_INVALID_ARG;
			goto FAIL;
		}
		return_value =
		    wait_for_state_ready(wait_for_fsm_vldclr, TIMEOUT_READ, PMIC_WRAP_WACS0_RDATA,
					 &reg_rdata);
		if (return_value != 0) {
			PWRAPERR("wait_for_fsm_vldclr fail,return_value=%d\n", return_value);
			return_value += 1;	/* E_PWR_NOT_INIT_DONE_READ or E_PWR_WAIT_IDLE_TIMEOUT_READ */
			goto FAIL;
		}
		*rdata = GET_WACS0_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS0_VLDCLR, 1);
	}
FAIL:
	spin_unlock_irqrestore(&mt_wrp_dvt->wacs0_lock, flags);
	return return_value;
}

/*--------------------------------------------------------
//    Function : pwrap_wacs1()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
S32 pwrap_wacs1(U32 write, U32 adr, U32 wdata, U32 *rdata)
{
	U32 reg_rdata = 0;
	U32 wacs_write = 0;
	U32 wacs_adr = 0;
	U32 wacs_cmd = 0;
	U32 return_value = 0;
	unsigned long flags = 0;

	/* Check argument validation */
	if ((write & ~(0x1)) != 0)
		return E_PWR_INVALID_RW;
	if ((adr & ~(0xffff)) != 0)
		return E_PWR_INVALID_ADDR;
	if ((wdata & ~(0xffff)) != 0)
		return E_PWR_INVALID_WDAT;

	spin_lock_irqsave(&mt_wrp_dvt->wacs1_lock, flags);
	/* Check IDLE & INIT_DONE in advance */
	return_value =
	    wait_for_state_idle(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS1_RDATA,
				PMIC_WRAP_WACS1_VLDCLR, 0);
	if (return_value != 0) {
		PWRAPERR("wait_for_fsm_idle fail,return_value=%d\n", return_value);
		goto FAIL;
	}
	/* Argument process */
	wacs_write = write << 31;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;
	/*send command */
	WRAP_WR32(PMIC_WRAP_WACS1_CMD, wacs_cmd);
	if (write == 0) {
		if (NULL == rdata) {
			PWRAPERR("rdata is a NULL pointer\n");
			return_value = E_PWR_INVALID_ARG;
			goto FAIL;
		}
		return_value =
		    wait_for_state_ready(wait_for_fsm_vldclr, TIMEOUT_READ, PMIC_WRAP_WACS1_RDATA,
					 &reg_rdata);
		if (return_value != 0) {
			PWRAPERR("wait_for_fsm_vldclr fail,return_value=%d\n", return_value);
			return_value += 1;	/* E_PWR_NOT_INIT_DONE_READ or E_PWR_WAIT_IDLE_TIMEOUT_READ */
			goto FAIL;
		}
		*rdata = GET_WACS0_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS1_VLDCLR, 1);
	}
FAIL:
	spin_unlock_irqrestore(&mt_wrp_dvt->wacs1_lock, flags);
	return return_value;

}

/*    Function : _pwrap_switch_dio()
// Description :call it after pwrap_init, check init done
//   Parameter :
//      Return :
//--------------------------------------------------------*/
S32 _pwrap_switch_dio(U32 dio_en)
{

	U32 arb_en_backup = 0;
	U32 rdata = 0;
	U32 sub_return = 0;
	U32 return_value = 0;

	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* only WACS0 */
	sub_return = pwrap_write(DEW_DIO_EN, dio_en);

	if (sub_return != 0) {
		PWRAPERR("[_pwrap_switch_dio] enable DEW_DIO fail,return=%x", sub_return);
		return E_PWR_SWITCH_DIO;
	}
	/* Wait WACS0_FSM==IDLE */
	return_value =
	    wait_for_state_ready(wait_for_idle_and_sync, TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA,
				 0);
	if (return_value != 0)
		return return_value;


	WRAP_WR32(PMIC_WRAP_DIO_EN, dio_en);
	/* Read Test */
	pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR
		    ("[_pwrap_switch_dio][Read Test] fail,dio_en = %x, READ_TEST rdata=%x, exp=0x5aa5\n",
		     dio_en, rdata);
		return E_PWR_READ_TEST_FAIL;
	}

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	return 0;
}

/*--------------------------------------------------------
//    Function : DrvPWRAP_SwitchMux()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
S32 _pwrap_switch_mux(U32 mux_sel_new)
{
	U32 mux_sel_old = 0;
	U32 rdata = 0;
	U32 return_value = 0;
	/* return if no change is necessary */
	mux_sel_old = WRAP_RD32(PMIC_WRAP_MUX_SEL);
	if (mux_sel_new == mux_sel_old)
		return;

	/* disable OLD, wait OLD finish */
	/* switch MUX, then enable NEW */
	if (mux_sel_new == 1) {
		WRAP_WR32(PMIC_WRAP_WRAP_EN, 0);
		/* Wait for WRAP to be in idle state, // and no remaining rdata to be received */
		return_value =
		    wait_for_state_ready_init(wait_for_wrap_idle, TIMEOUT_WAIT_IDLE,
					      PMIC_WRAP_WRAP_STA, 0);
		if (return_value != 0)
			return return_value;
		WRAP_WR32(PMIC_WRAP_MUX_SEL, 1);
		WRAP_WR32(PMIC_WRAP_MAN_EN, 1);
	} else {
		WRAP_WR32(PMIC_WRAP_MAN_EN, 0);
		/* Wait for WRAP to be in idle state, // and no remaining rdata to be received */
		return_value =
		    wait_for_state_ready_init(wait_for_man_idle_and_noreq, TIMEOUT_WAIT_IDLE,
					      PMIC_WRAP_MAN_RDATA, 0);
		if (return_value != 0)
			return return_value;

		WRAP_WR32(PMIC_WRAP_MUX_SEL, 0);
		WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);
	}

	return 0;
}



/*--------------------------------------------------------
//    Function : _pwrap_enable_cipher()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
S32 _pwrap_enable_cipher(void)
{
	U32 arb_en_backup = 0;
	U32 rdata = 0;
	U32 cipher_ready = 0;
	U32 return_value = 0;
	U64 start_time_ns = 0, timeout_ns = 5000000;
	PWRAPFUC();
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* only WACS2 */

	/*Make sure CIPHER engine is idle */
	pwrap_write(DEW_CIPHER_START, 0x0);
	WRAP_WR32(PMIC_WRAP_CIPHER_EN, 0);

	WRAP_WR32(PMIC_WRAP_CIPHER_MODE, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_KEY_SEL, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_IV_SEL, 2);
	WRAP_WR32(PMIC_WRAP_CIPHER_EN, 1);

	/*Config CIPHER @ PMIC */
	pwrap_write(DEW_CIPHER_SWRST, 0x1);
	pwrap_write(DEW_CIPHER_SWRST, 0x0);
	pwrap_write(DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write(DEW_CIPHER_IV_SEL, 0x2);

	pwrap_write(DEW_CIPHER_LOAD, 0x1);
	pwrap_write(DEW_CIPHER_START, 0x1);



	/*wait for cipher ready */
	return_value =
	    wait_for_state_ready_init(wait_for_cipher_ready, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_CIPHER_RDY, 0);
	if (return_value != 0)
		return return_value;

	start_time_ns = _pwrap_get_current_time();
	do {
		pwrap_read(DEW_CIPHER_RDY, &rdata);
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("timeout %dms when waiting for ready\n", timeout_ns / 1000000);
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
	} while (rdata != 0x1);	/* cipher_ready */

	pwrap_write(DEW_CIPHER_MODE, 0x1);
	/*wait for wacs2 ready */
	return_value =
	    wait_for_state_ready_init(wait_for_idle_and_sync, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_WACS2_RDATA, 0);
	if (return_value != 0)
		return return_value;

	WRAP_WR32(PMIC_WRAP_CIPHER_MODE, 1);

	/* Read Test */
	pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("Enable Encryption [Read Test] fail, READ_TEST rdata=%x, exp=0x5aa5",
			 rdata);
		return E_PWR_READ_TEST_FAIL;
	}

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	return 0;
}



/*--------------------------------------------------------
//    Function : _pwrap_disable_cipher()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
S32 _pwrap_disable_cipher(void)
{
	U32 arb_en_backup = 0;
	U32 rdata = 0;
	U32 return_value = 0;
	PWRAPFUC();
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* only WACS2 */

	/*[7:6]key_sel, [5:4]iv_sel, [3]swrst [2]load, [1]start, [0]mode */
	pwrap_write(DEW_CIPHER_MODE, 0x0);

	/*wait for wacs2 ready */
	return_value =
	    wait_for_state_ready_init(wait_for_idle_and_sync, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_WACS2_RDATA, 0);
	if (return_value != 0)
		return return_value;

	WRAP_WR32(PMIC_WRAP_CIPHER_MODE, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_EN, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 0);

	/* Read Test */
	pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("disable Encryption [Read Test] fail, READ_TEST rdata=%x, exp=0x5aa5",
			 rdata);
		return E_PWR_READ_TEST_FAIL;
	}

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	return 0;
}

/*--------------------------------------------------------
//    Function : _pwrap_manual_mode()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
S32 _pwrap_manual_mode(U32 write, U32 op, U32 wdata, U32 *rdata)
{
	U32 reg_rdata = 0;
	U32 man_write = 0;
	U32 man_op = 0;
	U32 man_cmd = 0;
	U32 return_value = 0;
	U32 pmic_sel = 0;	/* SLAVE Select, 0: 6323/6397 */
	reg_rdata = WRAP_RD32(PMIC_WRAP_MAN_RDATA);
	if (GET_MAN_FSM(reg_rdata) != 0)	/* IDLE State */
		return E_PWR_NOT_IDLE_STATE;

	/* check argument validation */
	if ((write & ~(0x1)) != 0)
		return E_PWR_INVALID_RW;
	if ((op & ~(0x1f)) != 0)
		return E_PWR_INVALID_OP_MANUAL;
	if ((wdata & ~(0xff)) != 0)
		return E_PWR_INVALID_WDAT;

	man_write = write << 14;
	pmic_sel = OP_PMIC_SEL << 13;
	man_op = op << 8;
	man_cmd = man_write | pmic_sel | man_op | wdata;
	WRAP_WR32(PMIC_WRAP_MAN_CMD, man_cmd);
	if (write == 0) {
		/*wait for wacs2 ready */
		return_value =
		    wait_for_state_ready_init(wait_for_man_vldclr, TIMEOUT_WAIT_IDLE,
					      PMIC_WRAP_MAN_RDATA, &reg_rdata);
		if (return_value != 0)
			return return_value;

		*rdata = GET_MAN_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_MAN_VLDCLR, 1);
	}
	return 0;
}

/*--------------------------------------------------------
//    Function : _pwrap_manual_modeAccess()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
S32 _pwrap_manual_modeAccess(U32 write, U32 adr, U32 wdata, U32 *rdata)
{
	U32 man_wdata = 0;
	U32 man_rdata = 0;

	/* check argument validation */
	if ((write & ~(0x1)) != 0)
		return E_PWR_INVALID_RW;
	if ((adr & ~(0xffff)) != 0)
		return E_PWR_INVALID_ADDR;
	if ((wdata & ~(0xffff)) != 0)
		return E_PWR_INVALID_WDAT;

	_pwrap_switch_mux(1);
	_pwrap_manual_mode(OP_WR, OP_CSH, 0, &man_rdata);
	_pwrap_manual_mode(OP_WR, OP_CSL, 0, &man_rdata);


	man_wdata = adr >> 1;
	_pwrap_manual_mode(OP_WR, OP_OUTD, (man_wdata & 0xff), &man_rdata);
	man_wdata = (adr >> 9) | (write << 7);
	man_wdata = man_wdata >> 8 | (write << 7);
	_pwrap_manual_mode(OP_WR, OP_OUTD, (man_wdata & 0xff), &man_rdata);


	_pwrap_manual_mode(OP_WR, OP_CSH, 0, &man_rdata);

	_pwrap_manual_mode(OP_WR, OP_CSL, 0, &man_rdata);



	if (write == 1) {
		man_wdata = wdata;
		_pwrap_manual_mode(1, OP_OUTD, (man_wdata & 0xff), &man_rdata);
		man_wdata = wdata >> 8;
		_pwrap_manual_mode(1, OP_OUTD, (man_wdata & 0xff), &man_rdata);
	} else {
		_pwrap_manual_mode(0, OP_IND, 0, &man_rdata);
		*rdata = GET_MAN_RDATA(man_rdata);
		_pwrap_manual_mode(0, OP_IND, 0, &man_rdata);
		*rdata |= (GET_MAN_RDATA(man_rdata) << 8);
	}
	return 0;
}

/*--------------------------------------------------------
//    Function : _pwrap_StaUpdTrig()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
static S32 _pwrap_StaUpdTrig(S32 mode)
{
	U32 man_rdata = 0;
	U32 reg_data = 0;
	U32 return_value = 0;

	/*Wait for FSM to be IDLE */
	return_value =
	    wait_for_state_ready_init(wait_for_stdupd_idle, TIMEOUT_WAIT_IDLE, PMIC_WRAP_STAUPD_STA,
				      0);
	if (return_value != 0)
		return return_value;

	/*Trigger FSM */
	WRAP_WR32(PMIC_WRAP_STAUPD_MAN_TRIG, 0x1);
	reg_data = WRAP_RD32(PMIC_WRAP_STAUPD_STA);
	/*Check if FSM is in REQ */
	if (GET_STAUPD_FSM(reg_data) != 0x2)
		return E_PWR_NOT_IDLE_STATE;

	/* if mode==1, only return after new status is updated. */
	if (mode == 1) {
		/* IDLE State */
		while (GET_STAUPD_FSM(reg_data) != 0x0)
			;
	}

	return 0;
}

/*--------------------------------------------------------
//    Function : _pwrap_AlignCRC()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
void _pwrap_AlignCRC(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 arb_en_backup = 0;
	U32 staupd_prd_backup = 0;
	U32 return_value = 0;
	/*Backup Configuration & Set New Ones */
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* only WACS2 */
	staupd_prd_backup = WRAP_RD32(PMIC_WRAP_STAUPD_PRD);
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0);	/* disable STAUPD */

	/* reset CRC */
	WRAP_WR32(PMIC_WRAP_CRC_EN, 0);

	/* Wait for FSM to be IDLE */
	return_value =
	    wait_for_state_ready_init(wait_for_wrap_state_idle, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_WRAP_STA, 0);
	if (return_value != 0)
		return;

	/* Enable CRC */
	WRAP_WR32(PMIC_WRAP_CRC_EN, 1);

	/* restore Configuration */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, staupd_prd_backup);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
}

/*--------------wrap test API------------------------------------*/

/*--------------------------------------------------------
//    Function : _pwrap_status_update_test()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
S32 _pwrap_status_update_test(void)
{
	U32 i, j;
	U32 rdata;
	U32 int_en_orig;
	U32 reg_int_raw_flg = 0;
	int result;
	PWRAPFUC();
	/*disable signature interrupt */
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x0);
	result = pwrap_write(DEW_WRITE_TEST, WRITE_TEST_VALUE);


	WRAP_WR32(PMIC_WRAP_SIG_ADR, DEW_WRITE_TEST);
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, 0xAA55);
	WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x1);

	rdata = WRAP_RD32(PMIC_WRAP_SIG_ERRVAL);

	/* SIG_ERRVAL usage:  [31:16] SIG_ERRVAL1; [15:0] SIG_ERRVAL; */
	if ((rdata & 0x0000FFFF) != WRITE_TEST_VALUE)
		PWRAPERR("_pwrap_status_update_test error,error code=%x, rdata=%x\n", 1, rdata);
	/* check sig_error interrupt flag bit */
	reg_int_raw_flg = WRAP_RD32(PMIC_WRAP_INT_FLG_RAW);
	PWRAPLOG("PMIC_WRAP_INT_RAW_FLG=0x%x.\n", reg_int_raw_flg);
	if (reg_int_raw_flg & 0x2) {
		PWRAPLOG("_int_test_bit1 pass.\n");
		/*clear sig_error interrupt flag bit */
		WRAP_WR32(PMIC_WRAP_INT_CLR, 1 << 1);
	} else {
		PWRAPLOG("_int_test_bit1 fail.\n");
	}

	WRAP_WR32(PMIC_WRAP_SIG_VALUE, WRITE_TEST_VALUE);	/* tha same as write test */
	/*clear sig_error interrupt flag bit */
	WRAP_WR32(PMIC_WRAP_INT_CLR, 1 << 1);

	/*enable signature interrupt */
	WRAP_WR32(PMIC_WRAP_INT_EN, int_en_orig);
	WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x0);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, DEW_CRC_VAL);

	pwrap_delay_us(1000);	/* delay 5 seconds */
	return 0;
}

/*--------------------------------------------------------
//    Function : _pwrap_wrap_access_test()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
static S32 _pwrap_wrap_access_test(void)
{
	U32 rdata = 0;
	U32 res = 0;
	U32 reg_value_backup = 0;
	U32 return_value = 0;
	PWRAPFUC();
	/*###############################
	   // Read/Write test using WACS0
	   //############################### */
	/*clear sig_error interrupt test */

	reg_value_backup = WRAP_RD32(PMIC_WRAP_INT_EN);
	WRAP_CLR_BIT(1 << 1, PMIC_WRAP_INT_EN);
	PWRAPLOG("start test WACS0 DEW_READ_TEST = 0x%x\n", DEW_READ_TEST);

	return_value = pwrap_wacs0(0, DEW_READ_TEST, 0, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS0),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;
	PWRAPLOG("start test WRAP_ACCESS_TEST_REG = 0x%x\n", WRAP_ACCESS_TEST_REG);
	pwrap_wacs0(1, WRAP_ACCESS_TEST_REG, 0x1234, &rdata);
	return_value = pwrap_wacs0(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
	if (rdata != 0x1234) {
		PWRAPERR("write test error(using WACS0),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	/*################################
	   // Read/Write test using WACS1
	   //############################### */
	PWRAPLOG("start test WACS1\n");
	return_value = pwrap_wacs1(0, DEW_READ_TEST, 0, &rdata);
	if (rdata != 0x5aa5) {
		PWRAPERR("read test error(using WACS1),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;
	pwrap_wacs1(1, WRAP_ACCESS_TEST_REG, 0x1234, &rdata);
	return_value = pwrap_wacs1(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
	if (rdata != 0x1234) {
		PWRAPERR("write test error(using WACS1),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;

	/*###############################
	   // Read/Write test using WACS2
	   //############################### */
	PWRAPLOG("start test WACS2\n");
	return_value = pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS2),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;
	pwrap_write(WRAP_ACCESS_TEST_REG, 0x1234);
	return_value = pwrap_wacs2(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
	if (rdata != 0x1234) {
		PWRAPERR("write test error(using WACS2),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;

	WRAP_WR32(PMIC_WRAP_INT_EN, reg_value_backup);
	return res;
}

static U32 _pwrap_WRITE_TEST_test(void)
{
	U32 rdata = 0;
	U32 res = 0;
	U32 reg_value_backup = 0;
	U32 return_value = 0;
	PWRAPFUC();

	return_value = pwrap_wacs2(0, DEW_WRITE_TEST, 0, &rdata);
	if (return_value != 0) {
		PWRAPERR("DEW_WRITE_TEST read test error(using WACS2),return_value=%x, rdata=%x\n",
			 return_value, rdata);
		res += 1;
	}

	if (rdata != 0) {
		PWRAPLOG("tc_reset_pattern_test WRITE_TEST reset default value fail, rdata=%x\n",
			 rdata);
	}

	return rdata;
}

/*--------------------------------------------------------
//    Function : _pwrap_man_access_test()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
static S32 _pwrap_man_access_test(void)
{
	U32 rdata = 0;
	U32 res = 0;
	U32 return_value = 0;
	U32 reg_value_backup;
	U32 reg_value_backup_prd;
	PWRAPFUC();
	/*###############################
	   // Read/Write test using manual mode
	   //############################### */
	reg_value_backup = WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN);
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, reg_value_backup & (~(0x1 << 6)));
	reg_value_backup_prd = WRAP_RD32(PMIC_WRAP_STAUPD_PRD);
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 1);

	return_value = _pwrap_manual_modeAccess(0, DEW_READ_TEST, 0, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		/* TERR="Error: [ReadTest] fail, rdata=%x, exp=0x5aa5", rdata */
		PWRAPERR("read test error(using manual mode),return_value=%x, rdata=%x",
			 return_value, rdata);
		res += 1;
	}

	rdata = 0;
	_pwrap_manual_modeAccess(1, WRAP_ACCESS_TEST_REG, 0x1234, &rdata);
	return_value = _pwrap_manual_modeAccess(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
	if (rdata != 0x1234) {
		/* TERR="Error: [WriteTest] fail, rdata=%x, exp=0x1234", rdata */
		PWRAPERR("write test error(using manual mode),return_value=%x, rdata=%x",
			 return_value, rdata);
		res += 1;
	}
	_pwrap_switch_mux(0);	/* wrap mode */

	rdata = 0;
	 /*MAN*/ _pwrap_AlignCRC();
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, reg_value_backup);
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, reg_value_backup_prd);

	return res;
}


static S32 tc_wrap_init_test(void)
{
	UINT32 ret = 0;
	UINT32 res = 0;
	U32 regValue = 0;
	mt_wrp_dvt->complete = pwrap_complete;
	mt_wrp_dvt->context = &pwrap_done;

	ret = pwrap_init();
	if (ret == 0) {
		PWRAPLOG("wrap_init test pass.\n");
		ret = _pwrap_status_update_test();
		if (ret == 0) {
			PWRAPLOG("_pwrap_status_update_test pass.\n");
		} else {
			PWRAPLOG("error:_pwrap_status_update_test fail.\n");
			res += 1;
		}
	} else {
		PWRAPLOG("error:wrap_init test fail.return_value=%d.\n", ret);
		res += 1;
	}
#ifdef DEBUG_LDVT
	regValue = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG =%x\n", regValue);

	regValue = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG =%x\n", regValue);
#endif

#ifdef DEBUG_CTP
	regValue = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG =%x\n", regValue);

	regValue = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG =%x\n", regValue);
#endif
	if (res != 0)
		PWRAPLOG("error:tc_wrap_init_test fail.\n");
	else
		PWRAPLOG("tc_wrap_init_test pass.\n");

	return res;
}

static S32 tc_wrap_access_test(void)
{
	int res = 0;
	res = _pwrap_wrap_access_test();
	if (res == 0)
		PWRAPLOG("WRAP_UVVF_WACS_TEST pass.\n");
	else
		PWRAPLOG("WRAP_UVVF_WACS_TEST fail.res=%d\n", res);
	return res;
}


static S32 tc_status_update_test(void)
{
	int res = 0;

	res = 0;
	res = _pwrap_status_update_test();
	if (res == 0)
		PWRAPLOG("_pwrap_status_update_test pass.\n");
	else
		PWRAPLOG("_pwrap_status_update_test fail.res=%d\n", res);
	return res;
}

static S32 tc_single_io_test(void)
{
	int res = 0;
	U32 rdata = 0;

	res = 0;

	/*disable dual io mode */
	_pwrap_switch_dio(0);
	PWRAPLOG("disable dual io mode.\n");
	res = _pwrap_wrap_access_test();
	if (res == 0)
		PWRAPLOG("_pwrap_wrap_access_test pass.\n");
	else
		PWRAPLOG("_pwrap_wrap_access_test fail.res=%d\n", res);

	if (res == 0)
		PWRAPLOG("tc_single_io_test pass.\n");
	else
		PWRAPLOG("tc_single_io_test fail.res=%d\n", res);
	return res;
}

static S32 tc_dual_io_test(void)
{
	int res = 0;
	U32 rdata = 0;

	res = 0;
	PWRAPLOG("enable dual io mode.\n");
	/*enable dual io mode */
	_pwrap_switch_dio(1);
	res = _pwrap_wrap_access_test();
	if (res == 0)
		PWRAPLOG("_pwrap_wrap_access_test pass.\n");
	else
		PWRAPLOG("_pwrap_wrap_access_test fail.res=%d\n", res);

	/*disable dual io mode */
	_pwrap_switch_dio(0);
	PWRAPLOG("disable dual io mode.\n");
	res = _pwrap_wrap_access_test();
	if (res == 0)
		PWRAPLOG("_pwrap_wrap_access_test pass.\n");
	else
		PWRAPLOG("_pwrap_wrap_access_test fail.res=%d\n", res);

	if (res == 0)
		PWRAPLOG("tc_dual_io_test pass.\n");
	else
		PWRAPLOG("tc_dual_io_test fail.res=%d\n", res);

	return res;
}

U32 RegWriteValue[4] = { 0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA };

static S32 tc_reg_rw_test(void)
{
	int res = 0;
	U32 i, j;
	U32 pmic_wrap_reg_size = 0;
	U32 PERI_PWRAP_BRIDGE_reg_size = 0;
	U32 regValue = 0;
	U32 reg_data = 0;

	U32 test_result = 0;
	PWRAPFUC();
	__pwrap_soft_reset();
	pwrap_dump_all_register();
	pmic_wrap_reg_size = sizeof(pmic_wrap_reg_tbl) / sizeof(pmic_wrap_reg_tbl[0]);

	PWRAPLOG("pmic_wrap_reg_size=%d\n", pmic_wrap_reg_size);

	PWRAPLOG("start test pmic_wrap_reg_tbl:default value test\n");
	for (i = 0; i < pmic_wrap_reg_size; i++) {
		/*Only R/W or RO should do default value test */
		if (pmic_wrap_reg_tbl[i][3] != WO) {
			PWRAPLOG("Reg offset %.3x: Default %.8x,i=%d\n", pmic_wrap_reg_tbl[i][0],
				 pmic_wrap_reg_tbl[i][1], i);
			if (i != 70) {
				if ((*
				     ((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0] +
								PMIC_WRAP_BASE)) !=
				     pmic_wrap_reg_tbl[i][1])) {
					PWRAPLOG
					    ("Reg offset %.3x Default %.8x,infact %.8x, Test failed!!\r\n",
					     pmic_wrap_reg_tbl[i][0], pmic_wrap_reg_tbl[i][1],
					     (*
					      ((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0] +
									 PMIC_WRAP_BASE))));
					test_result++;
				}
			}
			/* special for register 0x128, must clear the 0x128[4:2],
			   because throse bits are not for pwrap */
			else {
				if (((*
				      ((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0] +
								 PMIC_WRAP_BASE)) & (~0x1c)) !=
				     pmic_wrap_reg_tbl[i][1])) {
					PWRAPLOG
					    ("Reg offset %.3x Default %.8x,infact %.8x, Test failed!!\r\n",
					     pmic_wrap_reg_tbl[i][0], pmic_wrap_reg_tbl[i][1],
					     (*
					      ((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0] +
									 PMIC_WRAP_BASE))));
					test_result++;
				}
			}
		}
	}
	PWRAPLOG("start test pmic_wrap_reg_tbl:R/W test\n");
	for (i = 0; i < pmic_wrap_reg_size; i++) {
		if (pmic_wrap_reg_tbl[i][3] == RW) {
			for (j = 0; j < 4; j++) {
				*((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0] +
							    PMIC_WRAP_BASE)) =
				    (RegWriteValue[j] & pmic_wrap_reg_tbl[i][2]);
				if (((*
				      ((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0] +
								 PMIC_WRAP_BASE)) &
				      pmic_wrap_reg_tbl[i][2]) !=
				     (RegWriteValue[j] & pmic_wrap_reg_tbl[i][2]))) {
					PWRAPLOG
					    ("Reg offset %.3x R/W test fail. write %.8x, read %.8x \r\n",
					     (pmic_wrap_reg_tbl[i][0] + PMIC_WRAP_BASE),
					     (RegWriteValue[j] & pmic_wrap_reg_tbl[i][2]),
					     (*
					      ((volatile unsigned int *)(pmic_wrap_reg_tbl[i][0] +
									 PMIC_WRAP_BASE))) &
					     pmic_wrap_reg_tbl[i][2]);
					test_result++;
				}
			}
		}
	}
	__pwrap_soft_reset();
	if (test_result == 0)
		PWRAPLOG("tc_reg_rw_test pass.\n");
	else
		PWRAPLOG("tc_reg_rw_test fail.res=%d\n", test_result);
	return test_result;
}

static S32 tc_mux_switch_test(void)
{
	UINT32 res = 0;

	res = 0;
	res = _pwrap_man_access_test();
	if (res == 0)
		PWRAPLOG("tc_mux_switch_test pass.\n");
	else
		PWRAPLOG("tc_mux_switch_test fail.res=%d\n", res);

	return res;
}

static S32 tc_reset_pattern_test(void)
{
	UINT32 res = 0;
	UINT32 rdata = 0;
	UINT32 return_value = 0;
#if 0
	res = pwrap_init();
	res = _pwrap_wrap_access_test();
	res = pwrap_init();
	res = _pwrap_wrap_access_test();
	if (res == 0)
		PWRAPLOG("tc_reset_pattern_test pass.\n");
	else
		PWRAPLOG("tc_reset_pattern_test fail.res=%d\n", res);
#else

	WRAP_CLR_BIT(1 << 1, PMIC_WRAP_INT_EN);

	return_value = pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS2),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;

	pwrap_write(WRAP_ACCESS_TEST_REG, 0x1234);

	pwrap_read(WRAP_ACCESS_TEST_REG, &rdata);

	if (rdata != 0x1234) {
		PWRAPERR("write test error(using WACS2),return_value=%x, rdata=%x\n", res, rdata);
		res += 1;
	}

	res = _pwrap_reset_spislv();
	if (res != 0)
		PWRAPERR("error,_pwrap_reset_spislv fail,sub_return=%x\n", res);

	rdata = 0;

	return_value = pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS2),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;

	pwrap_read(WRAP_ACCESS_TEST_REG, &rdata);

	if (rdata != 0x1234) {
		PWRAPERR("write test error(using WACS2),return_value=%x, rdata=%x\n", res, rdata);
		res += 1;
	}
#endif
	return res;
}


static S32 tc_soft_reset_test(void)
{
	UINT32 res = 0;
	UINT32 regValue = 0;

	res = 0;
	/*---do wrap init and wrap access test-----------------------------------*/
#if 0
	res = pwrap_init();
	res = _pwrap_wrap_access_test();
#endif
	/*---reset wrap-------------------------------------------------------------*/
	PWRAPLOG("start reset wrapper\n");

	WRAP_WR32(PMIC_WRAP_DIO_EN, 1);
	PWRAP_SOFT_RESET;
	regValue = WRAP_RD32(INFRA_GLOBALCON_RST0);
	PWRAPLOG("the reset register =%x\n", regValue);
	regValue = WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN);
	PWRAPLOG("PMIC_WRAP_STAUPD_GRPEN =%x,it should be equal to 0x1\n", regValue);

	/*read DIO_EN, it should be disable */
	regValue = WRAP_RD32(PMIC_WRAP_DIO_EN);
	PWRAPLOG("PMIC_WRAP_DIO_EN =%x,it should be equal to 0x0\n", regValue);
	if ((regValue & 0x01) != 0x00) {
		PWRAPLOG("_pwrap_wrap_access_test fail.regValue =%d\n", regValue);
		return 0;
	}
	/*clear reset bit */
	PWRAP_CLEAR_SOFT_RESET_BIT;
#if 0
	PWRAPLOG("the wrap access test should be fail after reset,before init\n");
	res = _pwrap_wrap_access_test();
	if (res == 0)
		PWRAPLOG("_pwrap_wrap_access_test pass.\n");
	else
		PWRAPLOG("_pwrap_wrap_access_test fail.res=%d\n", res);
	PWRAPLOG("the wrap access test should be pass after reset and wrap init\n");

	res = pwrap_init();
	res = _pwrap_wrap_access_test();
	if (res == 0)
		PWRAPLOG("_pwrap_wrap_access_test pass.\n");
	else
		PWRAPLOG("_pwrap_wrap_access_test fail.res=%d\n", res);

	if (res == 0)
		PWRAPLOG("tc_soft_reset_test pass.\n");
	else
		PWRAPLOG("tc_soft_reset_test fail.res=%d\n", res);

	return res;
#endif
}

static S32 tc_high_pri_test(void)
{
	U32 res = 0;
	U32 rdata = 0;
	U64 pre_time = 0;
	U64 post_timer = 0;
	U64 enable_staupd_time = 0;
	U64 disable_staupd_time = 0;
	U64 GPT2_COUNT_value = 0;

	res = 0;
	/*----enable status updata and do wacs0-------------------------------------*/
	PWRAPLOG("enable status updata and do wacs0,record the cycle\n");
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x1);	/*0x1:20us,for concurrence test,MP:0x5;  //100us */
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0xff);
	/*###############################
	   // Read/Write test using WACS0
	   //############################### */
	GPT2_COUNT_value = WRAP_RD32(APMCU_GPTIMER_BASE + 0x0028);
	pre_time = sched_clock();
	PWRAPLOG("GPT2_COUNT_value=%lld pre_time=%lld\n", GPT2_COUNT_value, pre_time);
	pwrap_wacs0(0, DEW_READ_TEST, 0, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS0),error code=%x, rdata=%x\n", 1, rdata);
		res += 1;
	}
	post_timer = sched_clock();
	enable_staupd_time = post_timer - pre_time;
	PWRAPLOG("pre_time=%lld post_timer=%lld\n", pre_time, post_timer);
	PWRAPLOG("pwrap_wacs0 enable_staupd_time=%lld\n", enable_staupd_time);

	/*----disable status updata and do wacs0-------------------------------------*/
	PWRAPLOG("disable status updata and do wacs0,record the cycle\n");
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0xF);	/*0x1:20us,for concurrence test,MP:0x5;  //100us */
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0x00);
	/*###############################
	   // Read/Write test using WACS0
	   //############################### */
	pre_time = sched_clock();
	pwrap_wacs0(0, DEW_READ_TEST, 0, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS0),error code=%x, rdata=%x\n", 1, rdata);
		res += 1;
	}

	post_timer = sched_clock();
	disable_staupd_time = post_timer - pre_time;
	PWRAPLOG("pre_time=%lld post_timer=%lld\n", pre_time, post_timer);
	PWRAPLOG("pwrap_wacs0 disable_staupd_time=%lld\n", disable_staupd_time);
	if (disable_staupd_time <= enable_staupd_time)
		PWRAPLOG("tc_high_pri_test pass.\n");
	else
		PWRAPLOG("tc_high_pri_test fail.res=%d\n", res);
	return res;
}


static S32 tc_spi_encryption_test(void)
{
	int res = 0;
	U32 reg_value_backup = 0;
	int return_value;
	U32 rdata;
	/* disable status update,to check the waveform on oscilloscope */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x0);	/*0x0:disable */
	/*disable wdt int bit */
	reg_value_backup = WRAP_RD32(PMIC_WRAP_INT_EN);
	WRAP_CLR_BIT(1 << 0, PMIC_WRAP_INT_EN);
	/*disable dio mode,single io wave */
	_pwrap_switch_dio(0);
	/*###############################
	   // disable Encryption
	   //############################### */
	res = _pwrap_disable_cipher();	/*FPGA:set breakpoint here */
	if (res != 0) {
		PWRAPERR("disable Encryption error,error code=%x, rdata=%x", 0x21, res);
		return -EINVAL;
	}

	PWRAPLOG("start read PMIC1 using WACS2, please record the waveform\n");
	return_value = pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS2),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	/*###############################
	   // enable Encryption
	   //############################### */
	res = _pwrap_enable_cipher();
	if (res != 0) {
		PWRAPERR("Enable Encryption error,error code=%x, res=%x", 0x21, res);
		return -EINVAL;
	}

	PWRAPLOG("start read PMIC1 using WACS2, please record the waveform\n");
	return_value = pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS2),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	if (res == 0)
		PWRAPLOG("tc_spi_encryption_test pass.\n");
	else
		PWRAPLOG("tc_spi_encryption_test fail.res=%d\n", res);
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, reg_value_backup);	/*0x0:disable */
	return res;
}

/*for CLK manager DVT test case */
static S32 tc_clk_mag_test(void)
{
	int res = 0;

	res = tc_wrap_access_test();
	if (res == 0)
		PWRAPLOG("tc_clk_mag_test pass.\n");
	else
		PWRAPLOG("tc_clk_mag_test fail.res=%d\n", res);

	return res;
}

/*-------------------irq init start-------------------------------------*/
/*CHOOSE_LISR=0:normal test;CHOOSE_LISR=1:watch dog test;*/
/*CHOOSE_LISR=2:interrupt test*/
#define CHOOSE_LISR     1
#define NORMAL_TEST     1
#define WDT_TEST        2
#define INT_TEST        4

U32 wrapper_lisr_count_cpu0 = 0;
U32 wrapper_lisr_count_cpu1 = 0;

static U32 int_test_bit;
static U32 wait_int_flag;
static U32 int_test_fail_count;

/*global value for watch dog*/
static U32 wdt_test_bit;
static U32 wait_for_wdt_flag;
static U32 wdt_test_fail_count;

/*global value for peri watch dog*/


static S32 pwrap_lisr_normal_test(void)
{
	U32 reg_int_flg = 0;
	U32 reg_wdt_flg = 0;
	PWRAPFUC();

	if (raw_smp_processor_id() == 0)
		wrapper_lisr_count_cpu0++;
	else if (raw_smp_processor_id() == 1)
		wrapper_lisr_count_cpu1++;

	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x.\n", reg_int_flg);
	reg_wdt_flg = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x.\n", reg_wdt_flg);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);	/*clear watch dog */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffff);
	WRAP_WR32(PMIC_WRAP_INT_CLR, reg_int_flg);
}

static S32 pwrap_lisr_for_wdt_test(void)
{
	U32 reg_int_flg = 0;
	U32 reg_wdt_flg = 0;
	PWRAPFUC();

	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x.\n", reg_int_flg);
	reg_wdt_flg = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x.\n", reg_wdt_flg);

	if ((reg_int_flg & 0x1) != 0) {
		if ((reg_wdt_flg & (1 << wdt_test_bit)) != 0) {
			PWRAPLOG("watch dog test:recieve the right wdt.\n");
			wait_for_wdt_flag = 1;
			/*clear watch dog and interrupt */
			WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
		} else {
			PWRAPLOG("fail watch dog test:recieve the wrong wdt.\n");
			wdt_test_fail_count++;
			/*clear the unexpected watch dog and interrupt */
			WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
			WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 1 << wdt_test_bit);
		}
	}

	WRAP_WR32(PMIC_WRAP_INT_CLR, reg_int_flg);

	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x.\n", reg_int_flg);
	reg_wdt_flg = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x.\n", reg_wdt_flg);
}

static S32 pwrap_lisr_for_int_test(void)
{
	U32 reg_int_flg = 0;
	U32 reg_wdt_flg = 0;
	PWRAPFUC();
	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x.\n", reg_int_flg);
	reg_wdt_flg = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x.\n", reg_wdt_flg);

	/*-------------------------interrupt test---------------*/
	PWRAPLOG("int_test_bit=0x%x.\n", int_test_bit);
	if ((reg_int_flg & (1 << int_test_bit)) != 0) {
		PWRAPLOG(" int test:recieve the right pwrap interrupt.\n");
		wait_int_flag = 1;
	} else {
		PWRAPLOG(" int test fail:recieve the wrong pwrap interrupt.\n");
		int_test_fail_count++;
	}
	WRAP_WR32(PMIC_WRAP_INT_CLR, reg_int_flg);
	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("PMIC_WRAP_INT_FLG=0x%x.\n", reg_int_flg);
	reg_wdt_flg = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=0x%x.\n", reg_wdt_flg);

	/*for int test bit[1] */
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0);

}

static irqreturn_t mt_pwrap_dvt_irq(int irqno, void *dev_id)
{
	unsigned long flags = 0;
	PWRAPFUC();
	PWRAPLOG("Peng debug for mt_pwrap_dvt_irq.\n");
	pwrap_dump_ap_register();
	/*-----------------------------------------------------------------------*/
	switch (mt_wrp_dvt->irq_mode) {
	case NORMAL_TEST:
		pwrap_lisr_normal_test();
		break;
	case WDT_TEST:
		pwrap_lisr_for_wdt_test();
		mt_wrp_dvt->complete(mt_wrp_dvt->context);
		break;
	case INT_TEST:
		PWRAPLOG("Peng debug for mt_pwrap_dvt_irq INT_TEST.\n");
		pwrap_lisr_for_int_test();
		mt_wrp_dvt->complete(mt_wrp_dvt->context);
		break;
	}
	return IRQ_HANDLED;
}


/*-------------------irq init end-------------------------------------------*/

/*-------------------watch dog test start------------------------------------*/
U32 watch_dog_test_reg = DEW_WRITE_TEST;

static S32 _wdt_test_disable_other_int(void)
{
	/*disable watch dog */
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x1);
	return 0;
}

/*[1]: HARB_WACS0_ALE: HARB to WACS0 ALE timeout monitor*/
/*disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS0 write command*/
static S32 _wdt_test_bit1(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wait_for_wdt_flag = 0;
	wdt_test_bit = 1;
	/*disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x1ff);
	WRAP_CLR_BIT(WACS0, PMIC_WRAP_HIPRIO_ARB_EN);
	pwrap_wacs0(1, watch_dog_test_reg, 0x1234, &rdata);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit1 pass.\n");
	return 0;

}

/*[2]: HARB_WACS1_ALE: HARB to WACS1 ALE timeout monitor*/
/*disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS1 write command*/
static S32 _wdt_test_bit2(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 2;
	wait_for_wdt_flag = 0;
	/*disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x1ff);

	WRAP_CLR_BIT(WACS1, PMIC_WRAP_HIPRIO_ARB_EN);
	pwrap_wacs1(1, watch_dog_test_reg, 0x1234, &rdata);

	mdelay(20);
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit2 pass.\n");
	return 0;

	return res;
}

/*[3]: HARB_WACS2_ALE: HARB to WACS2 ALE timeout monitor*/
/*disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS2 write command*/
static S32 _wdt_test_bit3(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 3;
	wait_for_wdt_flag = 0;
	/*disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x1ff);
	WRAP_CLR_BIT(WACS2, PMIC_WRAP_HIPRIO_ARB_EN);
	pwrap_write(watch_dog_test_reg, 0x1234);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit3 pass.\n");
	return 0;
}

/*[5]: HARB_ERC_ALE: HARB to ERC ALE timeout monitor*/
/*disable the corresponding bit in HIPRIO_ARB_EN,do event test*/
static S32 _wdt_test_bit5(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 6;
	wait_for_wdt_flag = 0;
	/*disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x1ff);
	WRAP_CLR_BIT(1 << wdt_test_bit, PMIC_WRAP_HIPRIO_ARB_EN);
	/*similar to event  test case */

	mdelay(20);
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit5 pass.\n");
	return 0;
}

/*[6]: HARB_STAUPD_ALE: HARB to STAUPD ALE timeout monitor
//disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS0 write command*/
static S32 _wdt_test_bit6(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 6;
	wait_for_wdt_flag = 0;
	/*disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x1ff);
	WRAP_CLR_BIT(STAUPD, PMIC_WRAP_HIPRIO_ARB_EN);
	/*similar to status updata test case */
	pwrap_wacs0(1, DEW_WRITE_TEST, 0x55AA, &rdata);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, DEW_WRITE_TEST);
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, 0xAA55);
	WRAP_WR32(PMIC_WRAP_STAUPD_MAN_TRIG, 0x1);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit6 pass.\n");
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, 0x55AA);	/*tha same as write test */
	return 0;
}

/*[7]: PWRAP_PERI_ALE: HARB to PWRAP_PERI_BRIDGE ALE timeout monitor
//disable the corresponding bit in HIPRIO_ARB_EN,and send a WACS3 write command*/
static S32 _wdt_test_bit7(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 7;
	wait_for_wdt_flag = 0;
	/*disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x1ff);
	WRAP_CLR_BIT(1 << wdt_test_bit, PMIC_WRAP_HIPRIO_ARB_EN);

	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit7 pass.\n");
	return 0;
}

/*[8]: HARB_EINTBUF_ALE: HARB to EINTBUF ALE timeout monitor
//disable the corresponding bit in HIPRIO_ARB_EN,and send a eint interrupt*/
static S32 _wdt_test_bit8(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 8;
	wait_for_wdt_flag = 0;
	/*disable corresponding bit in PMIC_WRAP_HIPRIO_ARB_EN */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x1ff);
	WRAP_CLR_BIT(1 << wdt_test_bit, PMIC_WRAP_HIPRIO_ARB_EN);
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit8 pass.\n");
	return 0;
}

/*[9]: WRAP_HARB_ALE: WRAP to HARB ALE timeout monitor
//disable RRARB_EN[0],and do eint test*/
static S32 _wdt_test_bit9(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0, i = 0;
	PWRAPFUC();
	wdt_test_bit = 9;
	wait_for_wdt_flag = 0;

#ifdef ENABLE_EINT_ON_LDVT
#if 0
	eint_init();
	_concurrence_eint_test_code(eint_in_cpu0);
	eint_unmask(eint_in_cpu0);
	Delay(500);
#endif
#endif
	/*disable wrap_en */
	for (i = 0; i < (300 * 20); i++)
		;
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit9 pass.\n");
	return 0;
}

/*[10]: PWRAP_AG_ALE#1: PWRAP to AG#1 ALE timeout monitor
//disable RRARB_EN[1],and do keypad test*/
static S32 _wdt_test_bit10(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 10;
	wait_for_wdt_flag = 0;
	/*disable wrap_en */
	wait_for_completion(&pwrap_done);
	/*push keypad key */
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit10 pass.\n");
	return 0;
}

/*[11]: PWRAP_AG_ALE#2: PWRAP to AG#2 ALE timeout monitor
//disable RRARB_EN[0],and do eint test*/
static S32 _wdt_test_bit11(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 11;
	wait_for_wdt_flag = 0;
	/*kepadcommand */
#ifdef ENABLE_EINT_ON_LDVT
	/*
	   eint_init();
	   _concurrence_eint_test_code(eint_in_cpu0);
	   eint_unmask(eint_in_cpu0); */
#endif

	/*disable wrap_en */
	wait_for_completion(&pwrap_done);
	/*push keypad key */
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit11 pass.\n");
	return 0;
}

/*[12]: wrap_HARB_ALE: WRAP to harb ALE timeout monitor
//  ,disable wrap_en and set a WACS0 read command*/
static S32 _wdt_test_bit12(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 12;
	wait_for_wdt_flag = 0;

	_pwrap_switch_mux(1);	/*manual mode */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 0);	/*disble wrap */

	pwrap_wacs0(1, watch_dog_test_reg, 0x1234, &rdata);
	mdelay(20);
	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	_pwrap_switch_mux(0);

	PWRAPLOG("_wdt_test_bit12 pass.\n");
	return 0;

}

/*[13]: MUX_WRAP_ALE: MUX to WRAP ALE timeout monitor
// set MUX to manual mode ,enable wrap_en and set a WACS0 read command*/
static S32 _wdt_test_bit13(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 13;
	wait_for_wdt_flag = 0;

	_pwrap_switch_mux(1);	/*manual mode */

	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);	/*enable wrap */
	pwrap_wacs0(1, watch_dog_test_reg, 0x1234, &rdata);
	mdelay(20);
	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	_pwrap_switch_mux(0);	/* recover */

	PWRAPLOG("_wdt_test_bit13 pass.\n");
	return 0;
}

/*[14]: MUX_MAN_ALE: MUX to MAN ALE timeout monitor
//MUX to MAN ALE:set MUX to wrap mode and set manual command*/
static S32 _wdt_test_bit14(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 14;
	wait_for_wdt_flag = 0;
	_pwrap_switch_mux(0);	/*wrap mode */
	WRAP_WR32(PMIC_WRAP_MAN_EN, 1);	/*enable manual */

	_pwrap_manual_mode(OP_WR, OP_CSH, 0, &rdata);
	mdelay(20);
	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit14 pass.\n");
	_pwrap_switch_mux(1);
	return 0;
}

/*[16]: HARB_WACS0_DLE: HARB to WACS0 DLE timeout monitor
//HARB to WACS0 DLE:disable MUX,and send a read commnad with WACS0*/
static S32 _wdt_test_bit16(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 16;
	wait_for_wdt_flag = 0;

	reg_rdata = WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
	PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=%x.\n", reg_rdata);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS0);	/*enable wrap */
	reg_rdata = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	PWRAPLOG("PMIC_WRAP_HIPRIO_ARB_EN=%x.\n", reg_rdata);
	/*set status update period to the max value,or disable status update */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0xF);

	_pwrap_switch_mux(1);	/*manual mode */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);	/*enable wrap */
	/*read command */
	pwrap_wacs0(0, watch_dog_test_reg, 0, &rdata);
	mdelay(20);
	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit16 pass.\n");
	_pwrap_switch_mux(0);	/*recover */
	return 0;
}


/*[17]: HARB_WACS1_DLE: HARB to WACS1 DLE timeout monitor
//HARB to WACS1 DLE:disable MUX,and send a read commnad with WACS1*/
static S32 _wdt_test_bit17(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 17;
	wait_for_wdt_flag = 0;

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS1);	/*enable wrap */
	reg_rdata = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	PWRAPLOG("PMIC_WRAP_HIPRIO_ARB_EN=%x.\n", reg_rdata);
	/*set status update period to the max value,or disable status update */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x0);

	_pwrap_switch_mux(1);	/*manual mode */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);	/*enable wrap */
	/*read command */
	pwrap_wacs1(0, watch_dog_test_reg, 0, &rdata);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit17 pass.\n");
	_pwrap_switch_mux(0);	/*recover */
	return 0;
}

/*[18]: HARB_WACS2_DLE: HARB to WACS1 DLE timeout monitor
//HARB to WACS2 DLE:disable MUX,and send a read commnad with WACS2*/
static S32 _wdt_test_bit18(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 18;
	wait_for_wdt_flag = 0;

	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 1 << wdt_test_bit);
	reg_rdata = WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
	PWRAPLOG("PMIC_WRAP_HIPRIO_ARB_EN=%x.\n", reg_rdata);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);
	reg_rdata = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	PWRAPLOG("PMIC_WRAP_HIPRIO_ARB_EN=%x.\n", reg_rdata);
	/*set status update period to the max value,or disable status update */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0xF);

	reg_rdata = WRAP_RD32(PMIC_WRAP_STAUPD_PRD);
	PWRAPLOG("PMIC_WRAP_STAUPD_PRD=%x.\n", reg_rdata);

	_pwrap_switch_mux(1);	/*manual mode */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);	/*enable wrap */
	/*clear INT */
	WRAP_WR32(PMIC_WRAP_INT_CLR, 0xFFFFFFFF);

	/*read command */
	pwrap_read(watch_dog_test_reg, &rdata);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit18 pass.\n");
	_pwrap_switch_mux(0);	/*recover */
	return 0;
}

/*[19]: HARB_ERC_DLE: HARB to ERC DLE timeout monitor
//HARB to staupda DLE:disable event,write de_wrap event test,
then swith mux to manual mode ,enable wrap_en enable event*/
/*similar to bit5*/
static S32 _wdt_test_bit19(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 19;
	wait_for_wdt_flag = 0;
	/*disable event */

	/*do event test */
	/*disable mux */
	_pwrap_switch_mux(1);	/*manual mode */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);	/*enable wrap */
	/*enable event */
	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit19 pass.\n");
	_pwrap_switch_mux(0);	/*recover */
	return 0;
}

/*[20]: HARB_STAUPD_DLE: HARB to STAUPD DLE timeout monitor
//  HARB to staupda DLE:disable MUX,then send a read commnad ,and do status update test
//similar to bit6*/
static S32 _wdt_test_bit20(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 20;
	wait_for_wdt_flag = 0;
	_pwrap_switch_mux(1);	/*manual mode */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);	/*enable wrap */
	/*similar to status updata test case */
	pwrap_wacs0(1, DEW_WRITE_TEST, 0x55AA, &rdata);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, DEW_WRITE_TEST);
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, 0xAA55);
	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit20 pass.\n");
	_pwrap_switch_mux(0);	/*recover */
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, 0x55AA);	/*tha same as write test */

	return 0;
}

/*[21]: HARB_RRARB_DLE: HARB to RRARB DLE timeout monitor HARB to RRARB DLE
//:disable MUX,do keypad test*/
static S32 _wdt_test_bit21(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	U32 reg_backup;
	PWRAPFUC();
	wdt_test_bit = 21;
	wait_for_wdt_flag = 0;

	reg_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x80);	/*only enable keypad */
	_pwrap_switch_mux(1);	/*manual mode */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);	/*enable wrap */
	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit21 pass.\n");
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, reg_backup);

	_pwrap_switch_mux(0);	/*recover */
	return 0;
}


/*[22]: MUX_WRAP_DLE: MUX to WRAP DLE timeout monitor
//MUX to WRAP DLE:disable MUX,then send a read commnad ,and do WACS0*/
static S32 _wdt_test_bit22(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 22;
	wait_for_wdt_flag = 0;

	pwrap_wacs1(0, watch_dog_test_reg, 0, &rdata);
	_pwrap_switch_mux(1);	/*manual mode */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);	/*enable wrap */
	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit22 pass.\n");
	_pwrap_switch_mux(0);	/*recover */
	return 0;
}

/*[23]: MUX_MAN_DLE: MUX to MAN DLE timeout monitor
//MUX to MAN DLE:disable MUX,then send a read commnad in manual mode*/
static S32 _wdt_test_bit23(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	U32 return_value = 0;
	PWRAPFUC();
	wdt_test_bit = 23;
	wait_for_wdt_flag = 0;

	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 1 << wdt_test_bit);
	reg_rdata = WRAP_RD32(PMIC_WRAP_WDT_SRC_EN);
	PWRAPLOG("PMIC_WRAP_WDT_SRC_EN=%x.\n", reg_rdata);

	return_value = _pwrap_switch_mux(1);	/*manual mode */
	PWRAPLOG("_pwrap_switch_mux return_value=%x.\n", return_value);

	WRAP_WR32(PMIC_WRAP_SI_CK_CON, 0x6);
	reg_rdata = WRAP_RD32(PMIC_WRAP_SI_CK_CON);
	PWRAPLOG("PMIC_WRAP_SI_CK_CON=%x.\n", reg_rdata);

	return_value = _pwrap_manual_mode(0, OP_IND, 0, &rdata);
	PWRAPLOG("_pwrap_manual_mode return_value=%x.\n", return_value);
	mdelay(20);
	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit23 pass.\n");
	_pwrap_switch_mux(0);	/*recover */
	return 0;
}

/*[24]: MSTCTL_SYNC_DLE: MSTCTL to SYNC DLE timeout monitor
//MSTCTL to SYNC  DLE:disable sync,then send a read commnad with wacs0Q*/
static S32 _wdt_test_bit24(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 24;
	wait_for_wdt_flag = 0;
	_pwrap_switch_mux(1);	/*manual mode */
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit24 pass.\n");
	_pwrap_switch_mux(0);	/*recover */
	return 0;
}

/*[25]: STAUPD_TRIG:
//set period=0*/
static S32 _wdt_test_bit25(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	wdt_test_bit = 25;
	wait_for_wdt_flag = 0;
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x0);	/*0x1:20us,for concurrence test,MP:0x5;  //100us */
	mdelay(20);
	wait_for_completion(&pwrap_done);
	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit25 pass.\n");
	return 0;
}

/*[26]: PREADY: APB PREADY timeout monitor
//disable wrap_en and write wacs0 6 times*/
static S32 _wdt_test_bit26(void)
{
	U32 rdata = 0;
	U32 wdata = 0;
	U32 reg_rdata = 0;
	U32 i = 0;
	U32 wacs_write = 0;
	U32 wacs_adr = 0;
	U32 wacs_cmd = 0;
	U32 return_value = 0;
	UINT32 regValue = 0;
	PWRAPFUC();
	wdt_test_bit = 26;
	wait_for_wdt_flag = 0;

	/*enable watch dog interrupt */
	WRAP_WR32(PMIC_WRAP_INT_EN, (1 << 0) | WRAP_RD32(PMIC_WRAP_INT_EN));

	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 1 << wdt_test_bit);

	WRAP_WR32(PMIC_WRAP_WACS0_EN, 1);	/*enable wacs0 */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 0);	/*disable wrap */

	for (i = 0; i < 5; i++) {
		/*
		wdata += 0x20;
		PWRAPLOG("before send %d write command .\n", i);
		pwrap_wacs0(1, watch_dog_test_reg, wdata, &rdata);
		wacs_write  = 1 << 31;
		wacs_adr    = (DEW_WRITE_TEST >> 1) << 16;
		wacs_cmd = wacs_write | wacs_adr | wdata;
		wacs_cmd = 0xde060040;
		*/
		WRAP_WR32(PMIC_WRAP_WACS0_CMD, 0xde060020);
		PWRAPLOG("send %d write command.\n", i);
	}

	for (i = 0; i < 2; i++) {
		/*
		PWRAPLOG("before send %d read command .\n", i);
		pwrap_wacs0(0, watch_dog_test_reg, 0, &rdata);
		wacs_write  = 0 << 31;
		wacs_adr    = (DEW_READ_TEST >> 1) << 16;
		wacs_cmd= 0x5e050000;
		*/
		WRAP_WR32(PMIC_WRAP_WACS0_CMD, 0x5e050000);
		PWRAPLOG("send %d read command .\n", i);
	}
	wait_for_completion(&pwrap_done);

	while (wait_for_wdt_flag != 1)
		;
	PWRAPLOG("_wdt_test_bit26 pass.\n");
	return 0;
}

#define test_fail
static S32 tc_wdt_test(void)
{
	UINT32 return_value = 0;
	UINT32 result = 0;
	UINT32 reg_data = 0;
	mt_wrp_dvt->complete = pwrap_complete;
	mt_wrp_dvt->context = &pwrap_done;

	mt_wrp_dvt->irq_mode = WDT_TEST;
	/*enable watch dog */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffff);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit1();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit2();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit3();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit6();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit12();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit13();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();

	reg_data = WRAP_RD32(PMIC_WRAP_INT_FLG);
	PWRAPLOG("wrap_int_flg=%x.\n", reg_data);
	reg_data = WRAP_RD32(PMIC_WRAP_WDT_FLG);
	PWRAPLOG("PMIC_WRAP_WDT_FLG=%x.\n", reg_data);

	return_value = _wdt_test_bit14();
	mdelay(40);
	return_value = pwrap_init();
	_wdt_test_disable_other_int();

	return_value = _wdt_test_bit16();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit17();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit18();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit23();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit25();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	mdelay(40);
	return_value = _wdt_test_bit26();
	return_value = pwrap_init();
	_wdt_test_disable_other_int();

	PWRAPLOG("wdt_test_fail_count=%d.\n", wdt_test_fail_count);
	if (result == 0)
		PWRAPLOG("tc_wdt_test pass.\n");
	else
		PWRAPLOG("tc_wdt_test fail.res=%d\n", result);
	mt_wrp_dvt->irq_mode = NORMAL_TEST;
	return result;
}

/*test bit 1, bit 2, bit 3, bit 6*/
static S32 tc_wdt_test_sub1(void)
{
	UINT32 return_value = 0;
	UINT32 result = 0;
	UINT32 reg_data = 0;
	mt_wrp_dvt->complete = pwrap_complete;
	mt_wrp_dvt->context = &pwrap_done;

	mt_wrp_dvt->irq_mode = WDT_TEST;
	/*enable watch dog */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffffff);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit1();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit2();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit3();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit6();
	mdelay(40);

	PWRAPLOG("wdt_test_fail_count=%d.\n", wdt_test_fail_count);
	if (result == 0)
		PWRAPLOG("tc_wdt_test_sub1 bit(1, 2, 3, 6) pass.\n");
	else
		PWRAPLOG("tc_wdt_test_sub1 fail.res=%d\n", result);
	mt_wrp_dvt->irq_mode = NORMAL_TEST;
	return result;
}

/*test bit 12, bit 13, bit 14*/
static S32 tc_wdt_test_sub2(void)
{
	UINT32 return_value = 0;
	UINT32 result = 0;
	UINT32 reg_data = 0;
	mt_wrp_dvt->complete = pwrap_complete;
	mt_wrp_dvt->context = &pwrap_done;

	mt_wrp_dvt->irq_mode = WDT_TEST;
	/*enable watch dog */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffffff);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit12();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit13();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit14();
	mdelay(40);

	PWRAPLOG("wdt_test_fail_count=%d.\n", wdt_test_fail_count);
	if (result == 0)
		PWRAPLOG("tc_wdt_test_sub2 bit(12, 13, 14) pass.\n");
	else
		PWRAPLOG("tc_wdt_test_sub2 fail.res=%d\n", result);
	mt_wrp_dvt->irq_mode = NORMAL_TEST;
	return result;
}

/* test bit 16, bit 17, bit 18, bit 23, bit 25*/
static S32 tc_wdt_test_sub3(void)
{
	UINT32 return_value = 0;
	UINT32 result = 0;
	UINT32 reg_data = 0;
	mt_wrp_dvt->complete = pwrap_complete;
	mt_wrp_dvt->context = &pwrap_done;

	mt_wrp_dvt->irq_mode = WDT_TEST;
	/*enable watch dog */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffffff);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit16();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit17();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit18();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit23();
	mdelay(40);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit25();
	mdelay(40);

	PWRAPLOG("wdt_test_fail_count=%d.\n", wdt_test_fail_count);
	if (result == 0)
		PWRAPLOG("tc_wdt_test_sub3 bit(16, 17, 18, 23, 25) pass.\n");
	else
		PWRAPLOG("tc_wdt_test_sub3 fail.res=%d\n", result);
	mt_wrp_dvt->irq_mode = NORMAL_TEST;
	return result;
}

/* test bit 26*/
static S32 tc_wdt_test_sub4(void)
{
	UINT32 return_value = 0;
	UINT32 result = 0;
	UINT32 reg_data = 0;
	mt_wrp_dvt->complete = pwrap_complete;
	mt_wrp_dvt->context = &pwrap_done;

	mt_wrp_dvt->irq_mode = WDT_TEST;
	/*enable watch dog */
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffffff);

	return_value = pwrap_init();
	_wdt_test_disable_other_int();
	return_value = _wdt_test_bit26();

	PWRAPLOG("wdt_test_fail_count=%d.\n", wdt_test_fail_count);
	if (result == 0)
		PWRAPLOG("tc_wdt_test_sub4 bit(26) pass.\n");
	else
		PWRAPLOG("tc_wdt_test_sub4 fail.res=%d\n", result);
	mt_wrp_dvt->irq_mode = NORMAL_TEST;
	return result;
}

/*-------------------watch dog test end-------------------------------------*/

/*start----------------interrupt test ------------------------------------*/
U32 interrupt_test_reg = DEW_WRITE_TEST;
/*[1]:  SIG_ERR: Signature Checking failed.     set bit[0]=1 in cmd*/
static S32 _int_test_bit1(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	U32 return_value = 0;
	U32 wacs_write = 0;
	U32 wacs_adr = 0;
	U32 wacs_cmd = 0;
	U32 addr = WRAP_ACCESS_TEST_REG;
	PWRAPFUC();
	int_test_bit = 1;
	wait_int_flag = 0;
	WRAP_WR32(PMIC_WRAP_INT_EN, (1 << int_test_bit) | WRAP_RD32(PMIC_WRAP_INT_EN));

	pwrap_write(DEW_WRITE_TEST, 0x55AA);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, DEW_WRITE_TEST);
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, 0xAA55);
	WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x1);

	pwrap_delay_us(1000);	/*delay 5 seconds */
	rdata = WRAP_RD32(PMIC_WRAP_SIG_ERRVAL);
	if (rdata != 0x55AA)
		PWRAPERR("_pwrap_status_update_test error,error code=%x, rdata=%x", 1, rdata);

	mdelay(20);
	wait_for_completion(&pwrap_done);
	WRAP_WR32(PMIC_WRAP_INT_CLR, 1 << 1);
	if (wait_int_flag == 1)
		PWRAPLOG("_int_test_bit1 pass.\n");
	else
		PWRAPLOG("_int_test_bit1 fail.\n");
	return 0;
}


/*[5]:  MAN_CMD_MISS: A MAN CMD is written while MAN is disabled.
//    disable man,send a manual command*/
static S32 _int_test_bit5(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	U32 return_value = 0;
	PWRAPFUC();
	int_test_bit = 5;
	wait_int_flag = 0;
	WRAP_WR32(PMIC_WRAP_MAN_EN, 0);	/* disable man */

	return_value = _pwrap_manual_mode(OP_WR, OP_CSH, 0, &rdata);
	PWRAPLOG("return_value of _pwrap_manual_mode=%x.\n", return_value);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("_int_test_bit5 pass.\n");
	else
		PWRAPLOG("_int_test_bit5 fail.\n");
	return 0;
}

/*[14]: WACS0_CMD_MISS: A WACS0 CMD is written while WACS0 is disabled.
//    disable man,send a wacs0 command*/
static S32 _int_test_bit14(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	int_test_bit = 14;
	wait_int_flag = 0;
	WRAP_WR32(PMIC_WRAP_WACS0_EN, 0);	/* disable man */

	pwrap_wacs0(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("_int_test_bit14 pass.\n");
	else
		PWRAPLOG("_int_test_bit14 fail.\n");
	return 0;
}

/*[17]: WACS1_CMD_MISS: A WACS1 CMD is written while WACS1 is disabled.
//    disable man,send a wacs0 command*/
static S32 _int_test_bit17(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	int_test_bit = 17;
	wait_int_flag = 0;
	WRAP_WR32(PMIC_WRAP_WACS1_EN, 0);	/* disable man */

	pwrap_wacs1(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("_int_test_bit17 pass.\n");
	else
		PWRAPLOG("_int_test_bit17 fail.\n");
	return 0;
}

/*[20]: WACS2_CMD_MISS: A WACS2 CMD is written while WACS2 is disabled.
//    disable man,send a wacs2 command*/
static S32 _int_test_bit20(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	int_test_bit = 20;
	wait_int_flag = 0;
	WRAP_WR32(PMIC_WRAP_WACS2_EN, 0);	/* disable man */

	pwrap_write(WRAP_ACCESS_TEST_REG, 0x55AA);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("_int_test_bit20 pass.\n");
	else
		PWRAPLOG("_int_test_bit20 fail.\n");
	return 0;
}

/*[4]:  MAN_UNEXP_VLDCLR: MAN unexpected VLDCLR
//    send a manual write command,and clear valid big*/
static S32 _int_test_bit3(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	U32 return_value;
	PWRAPFUC();
	int_test_bit = 3;
	wait_int_flag = 0;
	_pwrap_switch_mux(1);
	return_value = _pwrap_manual_mode(OP_WR, OP_CSH, 0, &rdata);
	PWRAPLOG("return_value of _pwrap_manual_mode=%x.\n", return_value);
	WRAP_WR32(PMIC_WRAP_MAN_VLDCLR, 1);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("_int_test_bit3 pass.\n");
	else
		PWRAPLOG("_int_test_bit3 fail.\n");
	return 0;
}

/*[12]: WACS0_UNEXP_VLDCLR: WACS0 unexpected VLDCLR
//    send a wacs0 write command,and clear valid big*/
static S32 _int_test_bit12(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	int_test_bit = 12;
	wait_int_flag = 0;
	pwrap_wacs0(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
	WRAP_WR32(PMIC_WRAP_WACS0_VLDCLR, 1);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("_int_test_bit12 pass.\n");
	else
		PWRAPLOG("_int_test_bit12 fail.\n");
	return 0;
}

/*[15]: WACS1_UNEXP_VLDCLR: WACS1 unexpected VLDCLR
//    send a wacs1 write command,and clear valid big*/
static S32 _int_test_bit15(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	int_test_bit = 15;
	wait_int_flag = 0;
	pwrap_wacs1(1, WRAP_ACCESS_TEST_REG, 0x55AA, &rdata);
	WRAP_WR32(PMIC_WRAP_WACS1_VLDCLR, 1);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("_int_test_bit15 pass.\n");
	else
		PWRAPLOG("_int_test_bit15 fail.\n");
	return 0;
}

/*[18]: WACS2_UNEXP_VLDCLR: WACS2 unexpected VLDCLR
//    send a wacs2 write command,and clear valid big*/
static S32 _int_test_bit18(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	int_test_bit = 18;
	wait_int_flag = 0;
	pwrap_write(WRAP_ACCESS_TEST_REG, 0x55AA);
	WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR, 1);
	mdelay(20);
	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("_int_test_bit18 pass.\n");
	else
		PWRAPLOG("_int_test_bit18 fail.\n");
	return 0;
}

/*[21]: PERI_WRAP_INT: PERI_PWRAP_BRIDGE interrupt is asserted.
//    send a wacs3 write command,and clear valid big*/
static S32 _int_test_bit21(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 res = 0;
	PWRAPFUC();
	int_test_bit = 21;

	wait_int_flag = 0;
	mdelay(20);
	wait_for_completion(&pwrap_done);
	if (wait_int_flag == 1)
		PWRAPLOG("_int_test_bit21 pass.\n");
	else
		PWRAPLOG("_int_test_bit21 fail.\n");
	return 0;
}

static S32 _int_test_disable_watch_dog(void)
{
	/*disable watch dog*/
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	return 0;
}


static S32 tc_int_test(void)
{
	UINT32 return_value = 0;
	UINT32 test_this_case = 0;
	mt_wrp_dvt->complete = pwrap_complete;
	mt_wrp_dvt->context = &pwrap_done;

	mt_wrp_dvt->irq_mode = INT_TEST;

	pwrap_init();
	_int_test_disable_watch_dog();
	return_value += _int_test_bit1();
	mdelay(20);

	pwrap_init();
	_int_test_disable_watch_dog();
	return_value += _int_test_bit5();
	mdelay(20);

	pwrap_init();
	_int_test_disable_watch_dog();
	return_value += _int_test_bit14();
	mdelay(20);

	pwrap_init();
	_int_test_disable_watch_dog();
	return_value += _int_test_bit17();
	mdelay(20);

	pwrap_init();
	_int_test_disable_watch_dog();
	return_value += _int_test_bit20();
	mdelay(20);

	pwrap_init();
	_int_test_disable_watch_dog();
	return_value += _int_test_bit3();
	mdelay(20);

	pwrap_init();
	_int_test_disable_watch_dog();
	return_value += _int_test_bit12();
	mdelay(20);

	pwrap_init();
	_int_test_disable_watch_dog();
	return_value += _int_test_bit15();
	mdelay(20);

	pwrap_init();
	_int_test_disable_watch_dog();
	return_value += _int_test_bit18();
	mdelay(20);


	pwrap_init();
	_int_test_disable_watch_dog();

	if (return_value == 0)
		PWRAPLOG("tc_int_test pass.\n");
	else
		PWRAPLOG("tc_int_test fail.res=%d\n", return_value);
	mt_wrp_dvt->irq_mode = NORMAL_TEST;
	return return_value;
}

#define CLK_CFG_4_SET 0xF0000000
static void pwrap_power_off(void)
{
	PMICSPI_PDN;
}

static S32 tc_clock_gating_test(void)
{
	UINT32 return_value = 0;
	UINT32 test_this_case = 0;
	PWRAPFUC();
	pwrap_power_off();
	return_value = _pwrap_wrap_access_test();
	if (return_value == 0)
		PWRAPLOG("tc_clock_gating_test pass.\n");
	return return_value;
}

static S32 tc_infrasys_pmicspi_pdn_test(void)
{
	U32 rdata = 0;
	U32 res = 0;
	U32 reg_value_backup = 0;
	U32 return_value = 0;

	reg_value_backup = WRAP_RD32(PMIC_WRAP_INT_EN);
	WRAP_CLR_BIT(1 << 1, PMIC_WRAP_INT_EN);
	PWRAPLOG("start test WACS0 DEW_READ_TEST = 0x%x\n", DEW_READ_TEST);

	/*###############################/
	// Read/Write test using WACS2
	//###############################*/
	PWRAPLOG("start test WACS2\n");
	return_value = pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS2),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;
	pwrap_write(WRAP_ACCESS_TEST_REG, 0x1234);
	return_value = pwrap_wacs2(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
	if (rdata != 0x1234) {
		PWRAPERR("write test error(using WACS2),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;

	PMICSPI_PDN;
	/*###############################//
	// Read/Write test using WACS2
	//###############################*/
	PWRAPLOG("start test WACS2\n");
	return_value = pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("read test error(using WACS2),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;
	pwrap_write(WRAP_ACCESS_TEST_REG, 0x1234);
	return_value = pwrap_wacs2(0, WRAP_ACCESS_TEST_REG, 0, &rdata);
	if (rdata != 0x1234) {
		PWRAPERR("write test error(using WACS2),return_value=%x, rdata=%x\n", return_value,
			 rdata);
		res += 1;
	}
	rdata = 0;

	PMICSPI_ON;

	WRAP_WR32(PMIC_WRAP_INT_EN, reg_value_backup);
	return return_value;

}

static S32 tc_infrasys_pwrap_pdn_test(void)
{
	UINT32 return_value = 0;
	UINT32 test_this_case = 0;
	PWRAPFUC();
	return_value = _pwrap_wrap_access_test();
	if (return_value != 0)
		PWRAPLOG("_pwrap_wrap_access_test fail.\n");
	PMIC_WRAP_PDN;
	return_value = _pwrap_wrap_access_test();
	if (return_value != 0)
		PWRAPLOG("tc_infrasys_pwrap_pdn_test pass.\n");
	PMIC_WRAP_ON;

	return return_value;

}


volatile U32 index_wacs0 = 0;
volatile U32 index_wacs1 = 0;
volatile U32 index_wacs2 = 0;
U64 start_time_wacs0 = 0;
U64 start_time_wacs1 = 0;
U64 start_time_wacs2 = 0;
U64 end_time_wacs0 = 0;
U64 end_time_wacs1 = 0;
U64 end_time_wacs2 = 0;
U32 WACS0_TEST_REG = DEW_WRITE_TEST;
U32 WACS1_TEST_REG = DEW_WRITE_TEST;
U32 WACS2_TEST_REG = DEW_WRITE_TEST;
U32 WACS3_TEST_REG = DEW_WRITE_TEST;
U32 WACS4_TEST_REG = DEW_WRITE_TEST;


static void _throughput_wacs0_test(void)
{
	U32 i = 0;
	U32 rdata = 0;
	PWRAPFUC();
	start_time_wacs0 = sched_clock();
	for (index_wacs0 = 0; index_wacs0 < 10000; index_wacs0++)
		pwrap_wacs0(0, WACS0_TEST_REG, 0, &rdata);

	end_time_wacs0 = sched_clock();
	PWRAPLOG("_throughput_wacs0_test send 10000 read command:average time(ns)=%llx.\n",
		 (end_time_wacs0 - start_time_wacs0));
	PWRAPLOG("index_wacs0=%d index_wacs1=%d index_wacs2=%d\n", index_wacs0, index_wacs1,
		 index_wacs2);
	PWRAPLOG("start_time_wacs0=%llx start_time_wacs1=%llx start_time_wacs2=%llx\n",
		 start_time_wacs0, start_time_wacs1, start_time_wacs2);
	PWRAPLOG("end_time_wacs0=%llx end_time_wacs1=%llx end_time_wacs2=%llx\n", end_time_wacs0,
		 end_time_wacs1, end_time_wacs2);
}

static void _throughput_wacs1_test(void)
{
	U32 rdata = 0;
	PWRAPFUC();
	start_time_wacs1 = sched_clock();
	for (index_wacs1 = 0; index_wacs1 < 10000; index_wacs1++)
		pwrap_wacs1(0, WACS1_TEST_REG, 0, &rdata);

	end_time_wacs1 = sched_clock();
	PWRAPLOG("_throughput_wacs1_test send 10000 read command:average time(ns)=%llx.\n",
		 (end_time_wacs1 - start_time_wacs1));
	PWRAPLOG("index_wacs0=%d index_wacs1=%d index_wacs2=%d\n", index_wacs0, index_wacs1,
		 index_wacs2);
	PWRAPLOG("start_time_wacs0=%llx start_time_wacs1=%llx start_time_wacs2=%llx\n",
		 start_time_wacs0, start_time_wacs1, start_time_wacs2);
	PWRAPLOG("end_time_wacs0=%llx end_time_wacs1=%llx end_time_wacs2=%llx\n", end_time_wacs0,
		 end_time_wacs1, end_time_wacs2);
}

static void _throughput_wacs2_test(void)
{
	U32 i = 0;
	U32 rdata = 0;
	U32 return_value = 0;
	PWRAPFUC();
	start_time_wacs2 = sched_clock();
	for (index_wacs2 = 0; index_wacs2 < 10000; index_wacs2++)
		return_value = pwrap_wacs2(0, WACS2_TEST_REG, 0, &rdata);

	end_time_wacs2 = sched_clock();
	PWRAPLOG("_throughput_wacs2_test send 10000 read command:average time(ns)=%llx.\n",
		 (end_time_wacs2 - start_time_wacs2));
	PWRAPLOG("index_wacs0=%d index_wacs1=%d index_wacs2=%d\n", index_wacs0, index_wacs1,
		 index_wacs2);
	PWRAPLOG("start_time_wacs0=%llx start_time_wacs1=%llx start_time_wacs2=%llx\n",
		 start_time_wacs0, start_time_wacs1, start_time_wacs2);
	PWRAPLOG("end_time_wacs0=%llx end_time_wacs1=%llx end_time_wacs2=%llx\n", end_time_wacs0,
		 end_time_wacs1, end_time_wacs2);
}

static S32 tc_throughput_test(void)
{
	U32 return_value = 0;
	U32 test_this_case = 0;
	U32 i = 0;
	U64 start_time = 0;
	U64 end_time = 0;

	U32 wacs0_throughput_task = 0;
	U32 wacs1_throughput_task = 0;
	U32 wacs2_throughput_task = 0;

	U32 wacs0_throughput_cpu_id = 0;
	U32 wacs1_throughput_cpu_id = 1;
	U32 wacs2_throughput_cpu_id = 2;

	PWRAPFUC();

	/*disable INT*/
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0);
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x7ffffffc);	/*except for [31] debug_int*/
#if 0
	/*--------------------------------------------------------------------------------*/
	PWRAPLOG("write throughput,start.\n");
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 8);	/*Only WACS2*/
	start_time = sched_clock();
	for (i = 0; i < 10000; i++)
		pwrap_write(WACS2_TEST_REG, 0x30);

	end_time = sched_clock();
	PWRAPLOG("send 100 write command:average time(ns)=%llx.\n", (end_time - start_time));	/*100000=100*1000*/
	PWRAPLOG("write throughput,end.\n");
	/*-----------------------------------------------------------------------------------*/
#endif
#if 0
	dsb();
	PWRAPLOG("1-core read throughput,start.\n");
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 1 << 1);	/*Only WACS0*/
	wacs0_throughput_task = kthread_create(_throughput_wacs0_test, 0, "wacs0_concurrence");
	kthread_bind(wacs0_throughput_task, wacs0_throughput_cpu_id);
	wake_up_process(wacs0_throughput_task);
	pwrap_delay_us(5000);
	/*kthread_stop(wacs0_throughput_task);*/
	PWRAPLOG("stop wacs0_throughput_task.\n");
	PWRAPLOG("1-core read throughput,end.\n");
	/*-----------------------------------------------------------------------------------*/
#endif
#if 1
	dsb();
	PWRAPLOG("2-core read throughput,start.\n");
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 6);	/*Only WACS0 and WACS1*/
	wacs0_throughput_task = kthread_create(_throughput_wacs0_test, 0, "wacs0_concurrence");
	kthread_bind(wacs0_throughput_task, wacs0_throughput_cpu_id);
	wake_up_process(wacs0_throughput_task);

	wacs1_throughput_task = kthread_create(_throughput_wacs1_test, 0, "wacs1_concurrence");
	kthread_bind(wacs1_throughput_task, wacs1_throughput_cpu_id);
	wake_up_process(wacs1_throughput_task);

	mdelay(50);
	/*kthread_stop(wacs0_throughput_task);*/
	/*kthread_stop(wacs1_throughput_task);*/
	PWRAPLOG("stop wacs0_throughput_task and wacs1_throughput_task.\n");
	PWRAPLOG("2-core read throughput,end.\n");
	/*-----------------------------------------------------------------------------------*/
#endif
#if 0
	dsb();
	PWRAPLOG("3-core read throughput,start.\n");
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0xE);	/*Only WACS0 and WACS1*/
	wacs0_throughput_task = kthread_create(_throughput_wacs0_test, 0, "wacs0_concurrence");
	kthread_bind(wacs0_throughput_task, wacs0_throughput_cpu_id);
	wake_up_process(wacs0_throughput_task);

	wacs1_throughput_task = kthread_create(_throughput_wacs1_test, 0, "wacs1_concurrence");
	kthread_bind(wacs1_throughput_task, wacs1_throughput_cpu_id);
	wake_up_process(wacs1_throughput_task);

	wacs2_throughput_task = kthread_create(_throughput_wacs2_test, 0, "wacs2_concurrence");
	kthread_bind(wacs2_throughput_task, wacs2_throughput_cpu_id);
	wake_up_process(wacs2_throughput_task);
	pwrap_delay_us(50000);
	/*kthread_stop(wacs0_throughput_task);*/
	/*kthread_stop(wacs1_throughput_task);*/
	/*kthread_stop(wacs2_throughput_task);*/
	/*PWRAPLOG("stop wacs0_throughput_task /wacs1_throughput_task/wacs2_throughput_task.\n");*/
	PWRAPLOG("3-core read throughput,end.\n");
#endif
	if (return_value == 0)
		PWRAPLOG("tc_throughput_test pass.\n");
	else
		PWRAPLOG("tc_throughput_test fail.res=%d\n", return_value);
}

/*#ifdef PWRAP_CONCURRENCE_TEST*/

/*###############################concurrence_test start#########################
//---define wacs direction flag:  read:WACS0_READ_WRITE_FLAG=0;write:WACS0_READ_WRITE_FLAG=0;*/
/*#define RANDOM_TEST*/
/*#define NORMAL_TEST*/
/*#define stress_test_on_concurrence*/

static U8 wacs0_send_write_cmd_done;
static U8 wacs0_send_read_cmd_done;
static U8 wacs0_read_write_flag;


static U8 wacs1_send_write_cmd_done;
static U8 wacs1_send_read_cmd_done;
static U8 wacs1_read_write_flag;

static U8 wacs2_send_write_cmd_done;
static U8 wacs2_send_read_cmd_done;
static U8 wacs2_read_write_flag;


static U16 wacs0_test_value = 0x10;
static U16 wacs1_test_value = 0x20;
static U16 wacs2_test_value = 0x30;


U32 wacs_read_cmd_done = 0;
U32 test_count0 = 0;
U32 test_count1 = 0;

static U16 concurrence_fail_count_cpu0;
static U16 concurrence_fail_count_cpu1;
static U16 concurrence_pass_count_cpu0;
static U16 concurrence_pass_count_cpu1;

U32 g_spm_pass_count0 = 0;
U32 g_spm_fail_count0 = 0;
U32 g_spm_pass_count1 = 0;
U32 g_spm_fail_count1 = 0;

U32 g_pwm_pass_count0 = 0;
U32 g_pwm_fail_count0 = 0;
U32 g_pwm_pass_count1 = 0;
U32 g_pwm_fail_count1 = 0;

U32 g_wacs0_pass_count0 = 0;
U32 g_wacs0_fail_count0 = 0;
U32 g_wacs0_pass_count1 = 0;
U32 g_wacs0_fail_count1 = 0;

U32 g_wacs1_pass_count0 = 0;
U32 g_wacs1_fail_count0 = 0;
U32 g_wacs1_pass_count1 = 0;
U32 g_wacs1_fail_count1 = 0;

U32 g_wacs2_pass_count0 = 0;
U32 g_wacs2_fail_count0 = 0;
U32 g_wacs2_pass_count1 = 0;
U32 g_wacs2_fail_count1 = 0;


U32 g_stress0_cpu0_count = 0;
U32 g_stress1_cpu0_count = 0;
U32 g_stress2_cpu0_count = 0;
U32 g_stress3_cpu0_count = 0;
U32 g_stress4_cpu0_count = 0;

U32 g_stress0_cpu1_count = 0;
U32 g_stress1_cpu1_count = 0;
U32 g_stress2_cpu1_count = 0;
U32 g_stress3_cpu1_count = 0;
U32 g_stress4_cpu1_count = 0;
U32 g_stress5_cpu1_count = 0;

U32 g_stress0_cpu0_count0 = 0;
U32 g_stress1_cpu0_count0 = 0;
U32 g_stress0_cpu1_count0 = 0;

U32 g_stress0_cpu0_count1 = 0;
U32 g_stress1_cpu0_count1 = 0;
U32 g_stress0_cpu1_count1 = 0;

U32 g_stress2_cpu0_count1 = 0;
U32 g_stress3_cpu0_count1 = 0;

U32 g_random_count0 = 0;
U32 g_random_count1 = 0;
U32 g_wacs0_pass_cpu0 = 0;
U32 g_wacs0_pass_cpu1 = 0;
U32 g_wacs0_pass_cpu2 = 0;
U32 g_wacs0_pass_cpu3 = 0;

U32 g_wacs0_fail_cpu0 = 0;
U32 g_wacs0_fail_cpu1 = 0;
U32 g_wacs0_fail_cpu2 = 0;
U32 g_wacs0_fail_cpu3 = 0;

U32 g_wacs1_pass_cpu0 = 0;
U32 g_wacs1_pass_cpu1 = 0;
U32 g_wacs1_pass_cpu2 = 0;
U32 g_wacs1_pass_cpu3 = 0;

U32 g_wacs1_fail_cpu0 = 0;
U32 g_wacs1_fail_cpu1 = 0;
U32 g_wacs1_fail_cpu2 = 0;
U32 g_wacs1_fail_cpu3 = 0;

U32 g_wacs2_pass_cpu0 = 0;
U32 g_wacs2_pass_cpu1 = 0;
U32 g_wacs2_pass_cpu2 = 0;
U32 g_wacs2_pass_cpu3 = 0;

U32 g_wacs2_fail_cpu0 = 0;
U32 g_wacs2_fail_cpu1 = 0;
U32 g_wacs2_fail_cpu2 = 0;
U32 g_wacs2_fail_cpu3 = 0;

static DEFINE_MUTEX(wrap_read_write_sync);



/*--------------------------------------------------------
//    Function : pwrap_wacs0()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
static S32 _concurrence_pwrap_wacs0(U32 write, U32 adr, U32 wdata, U32 *rdata, U32 read_cmd_done)
{
	U32 reg_rdata = 0;
	U32 wacs_write = 0;
	U32 wacs_adr = 0;
	U32 wacs_cmd = 0;
	PWRAPFUC();
	if (read_cmd_done == 0) {
		reg_rdata = WRAP_RD32(PMIC_WRAP_WACS0_RDATA);
		if (GET_INIT_DONE0(reg_rdata) != 1) {
			PWRAPERR("initialization isn't finished when write data\n");
			return 1;
		}
		if (GET_WACS0_FSM(reg_rdata) != WACS_FSM_IDLE) {	/*IDLE State*/
			PWRAPERR("WACS0 is not in IDLE state\n");
			return 2;
		}
		/*check argument validation*/
		if ((write & ~(0x1)) != 0)
			return 3;
		if ((adr & ~(0xffff)) != 0)
			return 4;
		if ((wdata & ~(0xffff)) != 0)
			return 5;

		wacs_write = write << 31;
		wacs_adr = (adr >> 1) << 16;
		wacs_cmd = wacs_write | wacs_adr | wdata;
		WRAP_WR32(PMIC_WRAP_WACS0_CMD, wacs_cmd);
	} else {
		if (write == 0) {
			do {
				reg_rdata = WRAP_RD32(PMIC_WRAP_WACS0_RDATA);
				if (GET_INIT_DONE0(reg_rdata) != 1) {
					/*wrapper may be reset when error happen,so need to check if init is done*/
					PWRAPERR("initialization isn't finished when read data\n");
					return 6;
				}
			} while (GET_WACS0_FSM(reg_rdata) != WACS_FSM_WFVLDCLR);	/*WFVLDCLR*/

			*rdata = GET_WACS0_RDATA(reg_rdata);
			WRAP_WR32(PMIC_WRAP_WACS0_VLDCLR, 1);
		}
	}
	return 0;
}

/*--------------------------------------------------------
//    Function : pwrap_wacs1()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------*/
static S32 _concurrence_pwrap_wacs1(U32 write, U32 adr, U32 wdata, U32 *rdata, U32 read_cmd_done)
{
	U32 reg_rdata = 0;
	U32 wacs_write = 0;
	U32 wacs_adr = 0;
	U32 wacs_cmd = 0;
	if (read_cmd_done == 0) {
		PWRAPFUC();
		reg_rdata = WRAP_RD32(PMIC_WRAP_WACS1_RDATA);
		if (GET_INIT_DONE0(reg_rdata) != 1) {
			PWRAPERR("initialization isn't finished when write data\n");
			return 1;
		}
		if (GET_WACS0_FSM(reg_rdata) != WACS_FSM_IDLE)	{/*IDLE State*/
			PWRAPERR("WACS1 is not in IDLE state\n");
			return 2;
		}
		/*check argument validation*/
		if ((write & ~(0x1)) != 0)
			return 3;
		if ((adr & ~(0xffff)) != 0)
			return 4;
		if ((wdata & ~(0xffff)) != 0)
			return 5;

		wacs_write = write << 31;
		wacs_adr = (adr >> 1) << 16;
		wacs_cmd = wacs_write | wacs_adr | wdata;

		WRAP_WR32(PMIC_WRAP_WACS1_CMD, wacs_cmd);
	} else {
		if (write == 0) {
			do {
				reg_rdata = WRAP_RD32(PMIC_WRAP_WACS1_RDATA);
				if (GET_INIT_DONE0(reg_rdata) != 1) {
					/*wrapper may be reset when error happen,so need to check if init is done*/
					PWRAPERR("initialization isn't finished when read data\n");
					return 6;
				}
			} while (GET_WACS0_FSM(reg_rdata) != WACS_FSM_WFVLDCLR);	/*WFVLDCLR State*/

			*rdata = GET_WACS0_RDATA(reg_rdata);
			WRAP_WR32(PMIC_WRAP_WACS1_VLDCLR, 1);
		}
	}
	return 0;
}

/*----wacs API implement for concurrence test-----------------------*/
static S32 _concurrence_pwrap_wacs2(U32 write, U32 adr, U32 wdata, U32 *rdata, U32 read_cmd_done)
{
	U32 reg_rdata = 0;
	U32 wacs_write = 0;
	U32 wacs_adr = 0;
	U32 wacs_cmd = 0;
	if (read_cmd_done == 0) {
		PWRAPFUC();
		reg_rdata = WRAP_RD32(PMIC_WRAP_WACS2_RDATA);
		if (GET_INIT_DONE0(reg_rdata) != 1)
			return 1;
		if (GET_WACS0_FSM(reg_rdata) != WACS_FSM_IDLE) {	/*IDLE State*/
			PWRAPERR("WACS2 is not in IDLE state\n");
			return 2;
		}
		/* check argument validation*/
		if ((write & ~(0x1)) != 0)
			return 3;
		if ((adr & ~(0xffff)) != 0)
			return 4;
		if ((wdata & ~(0xffff)) != 0)
			return 5;

		wacs_write = write << 31;
		wacs_adr = (adr >> 1) << 16;
		wacs_cmd = wacs_write | wacs_adr | wdata;

		WRAP_WR32(PMIC_WRAP_WACS2_CMD, wacs_cmd);
	} else {
		if (write == 0) {
			do {
				reg_rdata = WRAP_RD32(PMIC_WRAP_WACS2_RDATA);
			} while (GET_WACS0_FSM(reg_rdata) != WACS_FSM_WFVLDCLR);	/*WFVLDCLR*/

			*rdata = GET_WACS0_RDATA(reg_rdata);
			WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR, 1);
		}
	}
	return 0;
}

static void _concurrence_wacs0_test(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 rand_number = 0;
	PWRAPFUC();
	while (1) {
#ifdef RANDOM_TEST
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1)
			msleep(10);
		else
#endif
		{
			rdata = 0;
			mutex_lock(&wrap_read_write_sync);
			pwrap_wacs0(1, WACS0_TEST_REG, wacs0_test_value, &rdata);
			pwrap_wacs0(0, WACS0_TEST_REG, wacs0_test_value, &rdata);
			mutex_unlock(&wrap_read_write_sync);

			if (rdata != wacs0_test_value) {
				g_wacs0_fail_count0++;
				pwrap_dump_all_register();
				PWRAPERR
				    ("read test error(using WACS0),wacs0_test_value=%x, rdata=%x\n",
				     wacs0_test_value, rdata);
				switch (raw_smp_processor_id()) {
				case 0:
					g_wacs0_fail_cpu0++;
					break;
				case 1:
					g_wacs0_fail_cpu1++;
					break;
				case 2:
					g_wacs0_fail_cpu2++;
					break;
				case 3:
					g_wacs0_fail_cpu3++;
					break;
				default:
					break;
				}
			} else {
				g_wacs0_pass_count0++;

				switch (raw_smp_processor_id()) {
				case 0:
					g_wacs0_pass_cpu0++;
					break;
				case 1:
					g_wacs0_pass_cpu1++;
					break;
				case 2:
					g_wacs0_pass_cpu2++;
					break;
				case 3:
					g_wacs0_pass_cpu3++;
					break;
				default:
					break;
				}
			}
			wacs0_test_value += 0x1;

		}
	}
}

static void _concurrence_wacs1_test(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 rand_number = 0;
	PWRAPFUC();
	while (1) {
#ifdef RANDOM_TEST
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1)
			msleep(10);
		else
#endif
		{
			rdata = 0;
			mutex_lock(&wrap_read_write_sync);
			pwrap_wacs1(1, WACS1_TEST_REG, wacs1_test_value, &rdata);
			pwrap_wacs1(0, WACS1_TEST_REG, wacs1_test_value, &rdata);
			mutex_unlock(&wrap_read_write_sync);
			if (rdata != wacs1_test_value) {
				g_wacs1_fail_count0++;
				pwrap_dump_all_register();
				PWRAPERR
				    ("read test error(using WACS1),wacs1_test_value=%x, rdata=%x\n",
				     wacs1_test_value, rdata);
				switch (raw_smp_processor_id()) {
				case 0:
					g_wacs1_fail_cpu0++;
					break;
				case 1:
					g_wacs1_fail_cpu1++;
					break;
				case 2:
					g_wacs1_fail_cpu2++;
					break;
				case 3:
					g_wacs1_fail_cpu3++;
					break;
				default:
					break;
				}
			} else {
				g_wacs1_pass_count0++;
				switch (raw_smp_processor_id()) {
				case 0:
					g_wacs1_pass_cpu0++;
					break;
				case 1:
					g_wacs1_pass_cpu1++;
					break;
				case 2:
					g_wacs1_pass_cpu2++;
					break;
				case 3:
					g_wacs1_pass_cpu3++;
					break;
				default:
					break;
				}
			}
			wacs1_test_value += 0x3;
		}
	}
}

static void _concurrence_wacs2_test(void)
{
	U32 rdata = 0;
	U32 reg_rdata = 0;
	U32 rand_number = 0;
	PWRAPFUC();
	while (1) {
#ifdef RANDOM_TEST
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1)
			msleep(10);
		else
#endif
		{

			rdata = 0;
			mutex_lock(&wrap_read_write_sync);
			pwrap_write(WACS2_TEST_REG, wacs2_test_value);
			pwrap_read(WACS2_TEST_REG, &rdata);
			mutex_unlock(&wrap_read_write_sync);
			if (rdata != wacs2_test_value) {
				g_wacs2_fail_count0++;
				pwrap_dump_all_register();
				switch (raw_smp_processor_id()) {
				case 0:
					g_wacs2_fail_cpu0++;
					break;
				case 1:
					g_wacs2_fail_cpu1++;
					break;
				case 2:
					g_wacs2_fail_cpu2++;
					break;
				case 3:
					g_wacs2_fail_cpu3++;
					break;
				default:
					break;
				}
				PWRAPERR
				    ("read test error(using WACS2),wacs2_test_value=%x, rdata=%x\n",
				     wacs2_test_value, rdata);
			} else {
				g_wacs2_pass_count0++;
				switch (raw_smp_processor_id()) {
				case 0:
					g_wacs2_pass_cpu0++;
					break;
				case 1:
					g_wacs2_pass_cpu1++;
					break;
				case 2:
					g_wacs2_pass_cpu2++;
					break;
				case 3:
					g_wacs2_pass_cpu3++;
					break;
				default:
					break;
				}
			}
			wacs2_test_value += 0x2;
		}
	}
}


U32 spm_task = 0;
U32 spm_cpu_id = 1;

U32 wacs0_task = 0;
U32 wacs0_cpu_id = 1;
U32 wacs1_task = 0;
U32 wacs1_cpu_id = 1;
U32 wacs2_task = 0;
U32 wacs2_cpu_id = 1;


U32 log0_task = 0;
U32 log0_cpu_id = 0;

U32 log1_task = 0;
U32 log1_cpu_id = 1;

U32 kthread_stress0_cpu0 = 0;
U32 stress0_cpu_id = 0;

U32 kthread_stress1_cpu0 = 0;
U32 stress1_cpu_id = 0;

U32 kthread_stress2_cpu0 = 0;
U32 stress2_cpu_id = 0;

U32 kthread_stress3_cpu0 = 0;
U32 stress3_cpu_id = 0;

U32 kthread_stress4_cpu0 = 0;
U32 stress4_cpu_id = 0;

U32 kthread_stress0_cpu1 = 0;
U32 stress01_cpu_id = 0;

U32 kthread_stress1_cpu1 = 0;
U32 kthread_stress2_cpu1 = 0;
U32 kthread_stress3_cpu1 = 0;
U32 kthread_stress4_cpu1 = 0;
U32 kthread_stress5_cpu1 = 0;

static S32 _concurrence_spm_test_code(unsigned int spm)
{
	PWRAPFUC();
#ifdef ENABLE_SPM_ON_LDVT
	U32 i = 0;
	while (1) {
		/*mtk_pmic_dvfs_wrapper_test(10);
		//i--;*/
	}
#endif
}

static S32 _concurrence_log0(unsigned int spm)
{
	PWRAPFUC();
	U32 i = 1;
	U32 index = 0;
	U32 cpu_id = 0;
	U32 rand_number = 0;
	U32 reg_value = 0;
	while (1) {

		PWRAPLOG("wacs0,cup0,pass count=%d,fail count=%d\n", g_wacs0_pass_cpu0,
			 g_wacs0_fail_cpu0);
		PWRAPLOG("wacs1,cup0,pass count=%d,fail count=%d\n", g_wacs1_pass_cpu1,
			 g_wacs1_fail_cpu1);
		PWRAPLOG("wacs2,cup0,pass count=%d,fail count=%d\n", g_wacs2_pass_cpu2,
			 g_wacs2_fail_cpu2);
		PWRAPLOG("\n");
#if 0
		PWRAPLOG("g_stress0_cpu0_count0=%d\n", g_stress0_cpu0_count0);
		PWRAPLOG("g_stress1_cpu0_count0=%d\n", g_stress1_cpu0_count0);
		PWRAPLOG("g_stress0_cpu1_count0=%d\n", g_stress0_cpu1_count0);
		PWRAPLOG("g_random_count0=%d\n", g_random_count0);
		PWRAPLOG("g_random_count1=%d\n", g_random_count1);
#endif
		reg_value = WRAP_RD32(PMIC_WRAP_HARB_STA1);
		PWRAPLOG("PMIC_WRAP_HARB_STA1=%d\n", reg_value);

		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			cpu_id = (wacs0_cpu_id++) % 2;
			if (wait_task_inactive(wacs0_task, TASK_UNINTERRUPTIBLE)) {
				PWRAPLOG("wacs0_cpu_id=%d\n", cpu_id);
				kthread_bind(wacs0_task, cpu_id);
			} else
				wacs0_cpu_id--;
		}

		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			cpu_id = (wacs1_cpu_id++) % 2;
			if (wait_task_inactive(wacs1_task, TASK_UNINTERRUPTIBLE)) {
				PWRAPLOG("wacs1_cpu_id=%d\n", cpu_id);
				kthread_bind(wacs1_task, cpu_id);
			} else
				wacs1_cpu_id--;
		}

		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			cpu_id = (wacs2_cpu_id++) % 2;
			if (wait_task_inactive(wacs2_task, TASK_UNINTERRUPTIBLE)) {
				PWRAPLOG("wacs2_cpu_id=%d\n", cpu_id);
				kthread_bind(wacs2_task, cpu_id);
			} else
				wacs2_cpu_id--;
		}
		schedule_timeout(10 * HZ);
	}

}

static S32 _concurrence_log1(unsigned int spm)
{
	PWRAPFUC();
	U32 i = 0;
	while (1) {
		PWRAPLOG("wacs0,pass count=%.10d,fail count=%d\n", g_wacs0_pass_count0,
			 g_wacs0_fail_count0);
		PWRAPLOG("wacs1,pass count=%.10d,fail count=%d\n", g_wacs1_pass_count0,
			 g_wacs1_fail_count0);
		PWRAPLOG("wacs2,pass count=%.10d,fail count=%d\n", g_wacs2_pass_count0,
			 g_wacs2_fail_count0);
		PWRAPLOG("\n");
#if 0
		PWRAPLOG("g_stress0_cpu0_count1=%d\n", g_stress0_cpu0_count1);
		PWRAPLOG("g_stress1_cpu0_count1=%d\n", g_stress1_cpu0_count1);
		PWRAPLOG("g_stress0_cpu1_count1=%d\n", g_stress0_cpu1_count1);
#endif

		msleep(2000);
		schedule_timeout(2000 * HZ);
	}

}

static S32 _concurrence_stress0_cpu0(unsigned int stress)
{
	PWRAPFUC();
	U32 i = 0;
	U32 rand_number = 0;

	while (1) {
		g_random_count0++;
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			g_random_count1++;
			for (i = 0; i < 100000; i++) {
				if (raw_smp_processor_id() == 0)
					g_stress0_cpu0_count0++;
				else if (raw_smp_processor_id() == 1)
					g_stress0_cpu0_count1++;
			}
		}
	}
}

static S32 _concurrence_stress1_cpu0(unsigned int stress)
{
	PWRAPFUC();
	U32 rand_number = 0;
	U32 i = 0;
	for (;;) {
		for (i = 0; i < 100000; i++) {
			if (raw_smp_processor_id() == 0)
				g_stress1_cpu0_count0++;
			else if (raw_smp_processor_id() == 1)
				g_stress1_cpu0_count1++;
		}
	}
}

static S32 _concurrence_stress2_cpu0(unsigned int stress)
{
	PWRAPFUC();
	U32 i = 0;
	U32 rand_number = 0;
	for (;;) {
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			for (i = 0; i < 100000; i++)
				g_stress1_cpu0_count++;
		}
	}
}

static S32 _concurrence_stress3_cpu0(unsigned int stress)
{
	PWRAPFUC();
	U32 i = 0;
	U32 rand_number = 0;
	for (;;) {
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			for (i = 0; i < 100000; i++)
				g_stress3_cpu0_count++;
		}
	}
}


static S32 _concurrence_stress4_cpu0(unsigned int stress)
{
	PWRAPFUC();
	U32 i = 0;
	U32 rand_number = 0;
	for (;;) {
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			for (i = 0; i < 100000; i++)
				g_stress4_cpu0_count++;
		}
	}
}

static S32 _concurrence_stress0_cpu1(unsigned int stress)
{
	PWRAPFUC();
	U32 i = 0;
	U32 rand_number = 0;
	for (;;) {
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			for (i = 0; i < 100000; i++) {
				if (raw_smp_processor_id() == 0)
					g_stress0_cpu1_count0++;
				else if (raw_smp_processor_id() == 1)
					g_stress0_cpu1_count1++;
			}
		}
	}
}

static S32 _concurrence_stress1_cpu1(unsigned int stress)
{
	PWRAPFUC();
	U32 i = 0;
	U32 rand_number = 0;
	for (;;) {
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			for (i = 0; i < 100000; i++)
				g_stress1_cpu1_count++;
		}
	}
}

static S32 _concurrence_stress2_cpu1(unsigned int stress)
{
	PWRAPFUC();
	U32 i = 0;
	U32 rand_number = 0;
	for (;;) {
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			for (i = 0; i < 100000; i++)
				g_stress2_cpu0_count++;
		}
	}
}

static S32 _concurrence_stress3_cpu1(unsigned int stress)
{
	PWRAPFUC();
	U32 i = 0;
	U32 rand_number = 0;
	for (;;) {
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			for (i = 0; i < 100000; i++)
				g_stress3_cpu0_count++;
		}
	}
}

static S32 _concurrence_stress4_cpu1(unsigned int stress)
{
	PWRAPFUC();
	U32 i = 0;
	U32 rand_number = 0;

	while (1) {
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			for (i = 0; i < 100000; i++)
				g_stress4_cpu1_count++;
		}
	}
}

static S32 _concurrence_stress5_cpu1(unsigned int stress)
{
	PWRAPFUC();
	U32 i = 0;
	U32 rand_number = 0;

	while (1) {
		rand_number = (U32) prandom_u32();
		if ((rand_number % 2) == 1) {
			for (i = 0; i < 100000; i++)
				g_stress5_cpu1_count++;
		}
	}
}

/*----wacs concurrence test start ------------------------------------------*/

static S32 tc_concurrence_test(void)
{
	UINT32 res = 0;
	U32 rdata = 0;
	U32 i = 0;
	res = 0;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1 };

	PWRAPFUC();
	wacs0_task = kthread_create(_concurrence_wacs0_test, 0, "wacs0_concurrence");
	if (IS_ERR(wacs0_task)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*kthread_bind(wacs0_task, wacs0_cpu_id);*/
	wake_up_process(wacs0_task);

	wacs1_task = kthread_create(_concurrence_wacs1_test, 0, "wacs1_concurrence");
	if (IS_ERR(wacs1_task)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*kthread_bind(wacs1_task, wacs1_cpu_id);*/
	wake_up_process(wacs1_task);

	wacs2_task = kthread_create(_concurrence_wacs2_test, 0, "wacs2_concurrence");
	if (IS_ERR(wacs2_task)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*kthread_bind(wacs2_task, wacs2_cpu_id);*/
	wake_up_process(wacs2_task);

	log1_task = kthread_create(_concurrence_log1, 0, "log1_concurrence");
	if (IS_ERR(log1_task)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*sched_setscheduler(log1_task, SCHED_FIFO, &param);*/
	kthread_bind(log1_task, log1_cpu_id);
	wake_up_process(log1_task);
#ifdef stress_test_on_concurrence
	kthread_stress0_cpu0 =
	    kthread_create(_concurrence_stress0_cpu0, 0, "stress0_cpu0_concurrence");
	if (IS_ERR(kthread_stress0_cpu0)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	kthread_bind(kthread_stress0_cpu0, 0);
	wake_up_process(kthread_stress0_cpu0);

	kthread_stress1_cpu0 =
	    kthread_create(_concurrence_stress1_cpu0, 0, "stress0_cpu1_concurrence");
	if (IS_ERR(kthread_stress1_cpu0)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	kthread_bind(kthread_stress1_cpu0, 0);
	wake_up_process(kthread_stress1_cpu0);

	kthread_stress2_cpu0 =
	    kthread_create(_concurrence_stress2_cpu0, 0, "stress0_cpu1_concurrence");
	if (IS_ERR(kthread_stress2_cpu0)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*kthread_bind(kthread_stress2_cpu0, 0);*/
	wake_up_process(kthread_stress2_cpu0);

	kthread_stress3_cpu0 =
	    kthread_create(_concurrence_stress3_cpu0, 0, "stress0_cpu1_concurrence");
	if (IS_ERR(kthread_stress3_cpu0)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*kthread_bind(kthread_stress3_cpu0, 0);*/
	wake_up_process(kthread_stress3_cpu0);

	/*kthread_stress4_cpu0 = kthread_create(_concurrence_stress4_cpu0,0,"stress0_cpu1_concurrence");*/
	if (IS_ERR(kthread_stress4_cpu0)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}

	kthread_stress0_cpu1 =
	    kthread_create(_concurrence_stress0_cpu1, 0, "stress0_cpu1_concurrence");
	if (IS_ERR(kthread_stress0_cpu1)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	kthread_bind(kthread_stress0_cpu1, 1);
	wake_up_process(kthread_stress0_cpu1);

	kthread_stress1_cpu1 =
	    kthread_create(_concurrence_stress1_cpu1, 0, "stress0_cpu1_concurrence");
	if (IS_ERR(kthread_stress1_cpu1)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*kthread_bind(kthread_stress1_cpu1, 1);*/
	wake_up_process(kthread_stress1_cpu1);

	kthread_stress2_cpu1 =
	    kthread_create(_concurrence_stress2_cpu1, 0, "stress0_cpu1_concurrence");
	if (IS_ERR(kthread_stress2_cpu1)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*kthread_bind(kthread_stress2_cpu1, 0);*/
	wake_up_process(kthread_stress2_cpu1);

	kthread_stress3_cpu1 =
	    kthread_create(_concurrence_stress3_cpu1, 0, "stress0_cpu1_concurrence");
	if (IS_ERR(kthread_stress3_cpu1)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*kthread_bind(kthread_stress3_cpu1, 1);*/
	wake_up_process(kthread_stress3_cpu1);

	kthread_stress4_cpu1 =
	    kthread_create(_concurrence_stress4_cpu1, 0, "stress0_cpu1_concurrence");
	if (IS_ERR(kthread_stress3_cpu1)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*kthread_bind(kthread_stress4_cpu1, 1);*/
	wake_up_process(kthread_stress4_cpu1);

	kthread_stress5_cpu1 =
	    kthread_create(_concurrence_stress5_cpu1, 0, "stress0_cpu1_concurrence");
	if (IS_ERR(kthread_stress3_cpu1)) {
		PWRAPERR("Unable to start kernelthread\n");
		res = -5;
	}
	/*kthread_bind(kthread_stress5_cpu1, 1);*/
	wake_up_process(kthread_stress5_cpu1);

#endif
	if (res == 0) {
		/*delay 8 hour*/
		U32 i, j;
		for (i = 0; i < 8; i++)
			for (j = 0; j < 60; j++)
				msleep(60000);
		PWRAPLOG("stop concurrent thread\n");

		kthread_stop(wacs0_task);
		kthread_stop(wacs1_task);
		kthread_stop(wacs2_task);
		kthread_stop(log1_task);
	}

	if (res == 0) {
		U32 count = g_wacs0_fail_count0 + g_wacs0_fail_count0 + g_wacs0_fail_count0;
		if (count == 0)
			PWRAPLOG("tc_concurrence_test pass.\n");
		else
			PWRAPLOG("tc_concurrence_test failed %d.\n", count);
	} else {
		PWRAPLOG("tc_concurrence_test build environment fail.res=%d\n", res);
	}
	return res;
}

/*-------------------concurrence_test end-------------------------------------*/

static S32 mt_pwrap_dvt(U32 nbr)
{
	S32 ret = 0;
	switch (nbr) {
	case INIT:
		tc_wrap_init_test();
		break;
	case ACCESS:
		tc_wrap_access_test();
		break;
	case STATUS_UPDATE:
		tc_status_update_test();
		break;
	case DUAL_IO:
		tc_dual_io_test();
		break;
	case REG_RW:
		tc_reg_rw_test();
		break;
	case MUX_SWITCH:
		tc_mux_switch_test();
		break;
	case SOFT_RESET:
		tc_soft_reset_test();
		break;
	case HIGH_PRI:
		tc_high_pri_test();
		break;
	case ENCRYPTION:
		tc_spi_encryption_test();
		break;
	case WDT:
		tc_wdt_test();
		break;
	case INTERRUPT:
		tc_int_test();
		break;
	case CONCURRENCE:
		tc_concurrence_test();
		break;
	case CLK_MANAGER:
		tc_clk_mag_test();
		break;
	case SINGLE_IO:
		tc_single_io_test();
		break;
	case RESET_PATTERN:
		tc_reset_pattern_test();
		break;
	case WDT_1:
		tc_wdt_test_sub1();
		break;
	case WDT_2:
		tc_wdt_test_sub2();
		break;
	case WDT_3:
		tc_wdt_test_sub3();
		break;
	case WDT_4:
		tc_wdt_test_sub4();
		break;
	case CLK_GATING:
		tc_clock_gating_test();
		break;
	case INFRASYS_PMICSPI:
		tc_infrasys_pmicspi_pdn_test();
		break;
	case INFRASYS_PWRAP:
		tc_infrasys_pwrap_pdn_test();
		break;
	default:
		PWRAPERR("unsupport(%d)\n", nbr);
		ret = -1;
		break;
	}

	return ret;
}

static ssize_t mt_pwrap_dvt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "[WRAP]driver need registered!! ");
}

static ssize_t mt_pwrap_dvt_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	U32 nbr = 0;
	if (1 == sscanf(buf, "%d", &nbr))
		mt_pwrap_dvt(nbr);
	return count;
}

DEVICE_ATTR(dvt, 0664, mt_pwrap_dvt_show, mt_pwrap_dvt_store);

/* ============================================================================== */
/* Pwrap device driver */
/* ============================================================================== */
static int dvt_pwrap_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_debug("[Power/PWRAP] ******** dvt_pwrap_probe!! ********\n");

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_dvt);

	return 0;
}


struct platform_device dvt_pwrap_device = {
	.name = "dvt-pwrap",
	.id = -1,
};

static struct platform_driver dvt_pwrap_driver = {
	.probe = dvt_pwrap_probe,
	.driver = {
		   .name = "dvt-pwrap",
		   },
};

#define VERSION     "LDVT"
static int __init mt_pwrap_init(void)
{
	S32 ret = 0;

	PWRAPLOG("HAL init: version %s\n", VERSION);


	ret = platform_device_register(&dvt_pwrap_device);
	if (ret) {
		pr_info("[Power/PWRAP] ""****[mt_pwrap_init] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&dvt_pwrap_driver);
	if (ret) {
		pr_info("[Power/PWRAP] ""****[mt_pwrap_init] Unable to register driver (%d)\n", ret);
		return ret;
	}

	free_irq(MT_PMIC_WRAP_IRQ_ID, NULL);
	ret =
	    request_irq(MT_PMIC_WRAP_IRQ_ID, mt_pwrap_dvt_irq, IRQF_TRIGGER_HIGH, "pwrap_dvt",
			NULL);
	if (ret) {
		PWRAPERR("register IRQ failed (%d)\n", ret);
		return ret;
	}

	return ret;
}

module_init(mt_pwrap_init);
