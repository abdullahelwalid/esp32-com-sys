#ifndef STA_WIFI_H
#define STA_WIFI_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Connect to the AP device.
void sta_wifi_init(void);

// Wait until connected (STA got IP).
void sta_wifi_wait_connected(TickType_t ticks_to_wait);

#endif /* STA_WIFI_H */

