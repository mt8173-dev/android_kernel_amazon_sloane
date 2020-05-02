#include <linux/module.h>
#include <mach/mtk_meminfo.h>

/* return the size of memory from start pfn to max pfn, */
/* _NOTE_ */
/* the memory area may be discontinuous */
unsigned int get_memory_size(void)
{
	return (unsigned int)(max_pfn << PAGE_SHIFT);
}
EXPORT_SYMBOL(get_memory_size);

