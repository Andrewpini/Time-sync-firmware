
#ifndef APP_GPIO_H
#define APP_GPIO_H

void gpiote_init(void);

void sync_master_gpio_init(void);

void led_blink_ms(uint32_t delay_ms, uint32_t blink_count);

void led_blink_stop(void);

void leds_init(void);

void button_init_dfu(void);

void sync_line_init(void);

#endif
