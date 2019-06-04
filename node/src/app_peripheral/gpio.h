
#ifndef APP_GPIO_H
#define APP_GPIO_H

#define LED_0                               11UL
#define LED_1                               12UL
#define LED_HP                              13UL
#define SYNC_OUT                            14UL  
#define SYNC_IN                             15UL
#define BUTTON_0                            16UL

void gpiote_init(void);

void leds_init(void);

void sync_line_init(void);

#endif
