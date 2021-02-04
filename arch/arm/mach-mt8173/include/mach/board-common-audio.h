
#ifndef __BOARD_COMMON_AUDIO_H__
#define __BOARD_COMMON_AUDIO_H__

struct mt_audio_custom_gpio_data {
	unsigned long aud_clk_mosi;	/* gpio_aud_clk_mosi_pin */
};

struct mt_audio_platform_data {
	struct mt_audio_custom_gpio_data gpio_data;
	int mt_audio_board_channel_type;	/* 0=Stereo, 1=MonoLeft, 2=MonoRight */
};

int mt_audio_init(struct mt_audio_custom_gpio_data *gpio_data, int mt_audio_board_channel_type);

#endif				/* __BOARD_COMMON_AUDIO_H__ */
