#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"

static volatile uint16_t pwm_seq[1]         = {0};

/* Sets PWM properties for a pin. Duty cycle in percent. Frequency is statically set to 1000 Hz */
void pwm_set_duty_cycle(uint8_t pin, float duty_cycle)
{
    pwm_seq[0] = ( 1 << 15 ) | (uint16_t)(PWM_PERIOD_US * 10.0 * duty_cycle / 1000);
    NRF_PWM1->PSEL.OUT[0] = (pin << PWM_PSEL_OUT_PIN_Pos) | (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);
    NRF_PWM1->ENABLE = (PWM_ENABLE_ENABLE_Enabled << PWM_ENABLE_ENABLE_Pos);
    NRF_PWM1->MODE = (PWM_MODE_UPDOWN_Up << PWM_MODE_UPDOWN_Pos);
    NRF_PWM1->PRESCALER = (PWM_PRESCALER_PRESCALER_DIV_16 << PWM_PRESCALER_PRESCALER_Pos);
    NRF_PWM1->COUNTERTOP = (PWM_PERIOD_US << PWM_COUNTERTOP_COUNTERTOP_Pos); 
    NRF_PWM1->LOOP = (PWM_LOOP_CNT_Disabled << PWM_LOOP_CNT_Pos);
    NRF_PWM1->DECODER = (PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos) | (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);
    NRF_PWM1->SEQ[0].PTR = ((uint32_t)(pwm_seq) << PWM_SEQ_PTR_PTR_Pos);
    NRF_PWM1->SEQ[0].CNT = ((sizeof(pwm_seq) / sizeof(uint16_t)) << PWM_SEQ_CNT_CNT_Pos);
    NRF_PWM1->SEQ[0].REFRESH = 0;
    NRF_PWM1->SEQ[0].ENDDELAY = 0;
    NRF_PWM1->TASKS_SEQSTART[0] = 1;
}

