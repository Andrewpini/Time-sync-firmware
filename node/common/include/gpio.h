
#ifndef APP_GPIO_H
#define APP_GPIO_H

void gpiote_init(void);

void leds_init(void);

void button_init_dfu(void);

void sync_line_init(void);

void sync_master_gpio_init(void);

#endif
