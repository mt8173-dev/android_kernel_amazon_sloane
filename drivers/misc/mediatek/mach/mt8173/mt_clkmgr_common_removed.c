#if !defined(CONFIG_FPGA_EARLY_PORTING)

enum cg_clk_id {
	NR_CLKS
};

int enable_clock(enum cg_clk_id id, char *name)
{
	return 0;
}
EXPORT_SYMBOL(enable_clock);

int disable_clock(enum cg_clk_id id, char *name)
{
	return 0;
}
EXPORT_SYMBOL(disable_clock);

#endif /* !defined(CONFIG_FPGA_EARLY_PORTING) */
