#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "nordic_common.h"
#include "app_error.h"
#include "config.h"
#include "nrf_gpio.h"
#include "timer.h"
#include "command_system.h"
#include "socket.h"

#define LOG(...)                            printf(__VA_ARGS__)
#define SOCKET_UDP                          3

/* Variables for calculating drift */
static volatile int m_prev_counter_val;
static volatile int m_prev_drift;
static volatile int m_current_drift;
static volatile uint32_t m_time_tic;
static volatile bool m_updated_drift_rdy;

//TODO /* TEMPORARY UNTIL WE GET THE NETWORK MODULE STRAIGHT!!!!!*/
static uint8_t own_MAC[6]          = {0};



int calc_drift_time(uint32_t counter_val){
    int temp_diff = m_prev_counter_val - counter_val;
    if(temp_diff < - (DRIFT_TIMER_MAX / 2)){
      return DRIFT_TIMER_MAX - abs(temp_diff);
    } else {
      return temp_diff;
    }
}

/*Triggers each time the sync-line is set high by the master node*/
void GPIOTE_IRQHandler(void)
{
    if (NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_SYNC_IN]){
        
        NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL_SYNC_IN] = 0;

        /*Snapshots the value of timer 0 and calculates the current drift*/
        m_current_drift = m_prev_drift - calc_drift_time(NRF_TIMER0->CC[1]);
        LOG("Raw counter value : %d\n", NRF_TIMER0->CC[1]);
        LOG("Current timer drift (in relation to sync master) : %d microseconds\n", m_current_drift);
        m_time_tic++;
        m_prev_drift = m_current_drift;
        m_updated_drift_rdy = true;

        m_prev_counter_val = NRF_TIMER0->CC[1];
    }
}

/* Function to send timing samples over Ethernet, using UDP */
void send_drift_timing_sample(void)
{
    if(m_updated_drift_rdy){

        uint8_t buf[SCAN_REPORT_LENGTH];
        uint8_t len = 0;

        uint8_t target_IP[4] = {10, 0, 0, 4};    
        uint32_t target_port = 15000;
        get_target_ip_and_port(target_IP, &target_port);

        if(!is_network_busy())
        {
            set_network_busy(true);
        
            sprintf((char *)&buf[0], "{ \"nodeID\" : \"%02x:%02x:%02x:%02x:%02x:%02x\", \"drift\" : %d, \"timetic\" : %d}", 
                            own_MAC[0], own_MAC[1], own_MAC[2], own_MAC[3], own_MAC[4], own_MAC[5],
                            m_current_drift,
                            m_time_tic);
        

            len = strlen((const char *)&buf[0]);
        
        
            uint32_t err = sendto(SOCKET_UDP, &buf[0], len, target_IP, target_port);
            if(err < 1){
                LOG("Error during sending: %d\n", err);
            }
       
        
            set_network_busy(false);
        }
        m_updated_drift_rdy = false;
    }
}

void reset_drift_measure_params(void){
m_prev_counter_val = 0;
m_prev_drift = 0;
m_current_drift = 0;
m_time_tic = 0;
m_updated_drift_rdy = 0;
}