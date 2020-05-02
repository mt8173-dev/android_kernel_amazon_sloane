#ifndef __DRV_CLK_MT8173_PG_H
#define __DRV_CLK_MT8173_PG_H

enum subsys_id {
	SYS_VDE,
	SYS_MFG,
	SYS_VEN,
	SYS_ISP,
	SYS_DIS,
	SYS_VEN2,
	SYS_AUDIO,
	SYS_MFG_2D,
	SYS_MFG_ASYNC,
	SYS_USB,
	NR_SYSS,
};

struct pg_callbacks {
	void (*before_off)(enum subsys_id sys);
	void (*after_on)(enum subsys_id sys);
};

/* register new pg_callbacks and return previous pg_callbacks. */
extern struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb);

#endif /* __DRV_CLK_MT8173_PG_H */
