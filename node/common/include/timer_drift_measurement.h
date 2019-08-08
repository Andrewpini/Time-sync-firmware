
#ifndef DRIFT_MEASURE_H
#define DRIFT_MEASURE_H

void sync_line_event_handler(void);

void send_drift_timing_sample(uint32_t adjusted_sync_timer);

void reset_drift_measure_params(void);
#endif
