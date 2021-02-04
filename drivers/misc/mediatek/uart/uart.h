#ifndef UART_H
#define UART_H
#if defined(ENABLE_SYSFS)
/*define sysfs entry for configuring debug level and sysrq*/
ssize_t mtk_uart_attr_show(struct kobject *kobj, struct attribute *attr, char *buffer);
ssize_t mtk_uart_attr_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size);
ssize_t mtk_uart_debug_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_debug_store(struct kobject *kobj, const char *page, size_t size);
ssize_t mtk_uart_sysrq_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_sysrq_store(struct kobject *kobj, const char *page, size_t size);
ssize_t mtk_uart_vffsz_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_vffsz_store(struct kobject *kobj, const char *page, size_t size);
ssize_t mtk_uart_conse_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_conse_store(struct kobject *kobj, const char *page, size_t size);
ssize_t mtk_uart_vff_en_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_vff_en_store(struct kobject *kobj, const char *page, size_t size);
ssize_t mtk_uart_lsr_status_show(struct kobject *kobj, char *page);
ssize_t mtk_uart_lsr_status_store(struct kobject *kobj, const char *page, size_t size);
void mtk_uart_switch_tx_to_gpio(struct mtk_uart *uart);
void mtk_uart_switch_to_tx(struct mtk_uart *uart);
void mtk_uart_switch_rx_to_gpio(struct mtk_uart *uart);
void mtk_uart_switch_to_rx(struct mtk_uart *uart);
#endif
#endif

