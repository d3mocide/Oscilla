/*
 * status_led.h — the XIAO's user LED as a liveness indicator.
 *
 *   booting   rapid blink
 *   healthy   short flash every heartbeat period
 *   activity  extra flash per command handled
 *   fault     solid on
 *   dark      no power, or the dispatch loop has stopped kicking
 *
 * The heartbeat is gated on status_led_kick(), so a wedged dispatch task goes
 * dark instead of blinking as though it were fine.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_STATUS_LED_H
#define OSCILLA_STATUS_LED_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t status_led_start(void);

/* Call from the dispatch loop; the heartbeat stops if this stops. */
void status_led_kick(void);

/* Boot finished: switch from rapid blink to heartbeat. */
void status_led_ready(void);

void status_led_activity(void);

void status_led_set_fault(bool fault);

#endif /* OSCILLA_STATUS_LED_H */
