
#ifndef DRIFT_MEASURE_H
#define DRIFT_MEASURE_H

void sync_line_event_handler(void);

void reset_drift_measure_params(void);

void sync_master_set(uint32_t interval);

void sync_master_unset(void);

void set_initial_line_state(bool state);

void sync_line_debouncer_init(void);

#endif
