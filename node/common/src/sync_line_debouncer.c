#include "app_timer.h"
#include "log.h"
#include "nrf_gpio.h"
#include "boards.h"




APP_TIMER_DEF(m_debounce_timer);


void debounce_test(uint32_t val)
{
//    uint32_t line_status = nrf_gpio_pin_read(SYNC_OUT);
    app_timer_start(m_debounce_timer, APP_TIMER_TICKS(val), NULL);
    nrf_gpio_pin_toggle(SYNC_OUT);
}

static void debounce_timeout_handler(void * p_context)
{
    nrf_gpio_pin_toggle(SYNC_OUT);
}

void debounce_test_init(void)
{
   APP_ERROR_CHECK(app_timer_create(&m_debounce_timer, APP_TIMER_MODE_SINGLE_SHOT, debounce_timeout_handler));
}
