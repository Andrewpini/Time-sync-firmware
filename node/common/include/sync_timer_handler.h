#ifndef SYNC_TIMER_HANDLER_H
#define SYNC_TIMER_HANDLER_H

uint32_t sync_timer_get_adjusted_timestamp(void);

uint32_t sync_timer_get_raw_timestamp(void);

int32_t sync_timer_set_timer_offset(int32_t incoming_timestamp);

void sync_timer_increment_timer_offset(int32_t increment);

void sync_timer_reset(void);

int32_t sync_timer_get_current_offset(void);

#endif