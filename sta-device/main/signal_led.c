#include "signal_led.h"

#include "driver/gpio.h"

/* Onboard LED: GPIO2 on many DevKit boards. */
#define SIGNAL_LED_GPIO 2
/* 1 = Espressif DevKit-style (GPIO low = LED on). */
#define SIGNAL_LED_ACTIVE_LOW 0

/* Relative gate: peak must exceed quiet_floor by this much (adds to quiet_floor/4). */
#define SIGNAL_MARGIN_ADD (12000)
/* Keep LED on for a few updates after last activity. */
#define SIGNAL_HOLD_UPDATES 4

static int s_hold = 0;
static int32_t s_quiet_floor = 0;
static bool s_floor_ready = false;

static void signal_led_set(bool on)
{
#if SIGNAL_LED_ACTIVE_LOW
    gpio_set_level(SIGNAL_LED_GPIO, on ? 0 : 1);
#else
    gpio_set_level(SIGNAL_LED_GPIO, on ? 1 : 0);
#endif
}

void signal_led_init(void)
{
    gpio_reset_pin(SIGNAL_LED_GPIO);
    gpio_set_direction(SIGNAL_LED_GPIO, GPIO_MODE_OUTPUT);
    signal_led_set(false);

    s_hold = 0;
    s_quiet_floor = 0;
    s_floor_ready = false;
}

void signal_led_update_from_pcm16(const int16_t *samples, int sample_count)
{
    int32_t peak = 0;
    for (int i = 0; i < sample_count; i++) {
        int32_t a = samples[i] < 0 ? -(int32_t)samples[i] : (int32_t)samples[i];
        if (a > peak) {
            peak = a;
        }
    }

    if (!s_floor_ready) {
        s_quiet_floor = peak > 0 ? peak : 1;
        s_floor_ready = true;
    } else if (peak < s_quiet_floor) {
        s_quiet_floor = (31 * s_quiet_floor + peak) / 32;
    } else {
        s_quiet_floor += (peak - s_quiet_floor) >> 11;
    }

    int64_t margin = (int64_t)SIGNAL_MARGIN_ADD + ((int64_t)s_quiet_floor / 4);
    if ((int64_t)peak > (int64_t)s_quiet_floor + margin) {
        s_hold = SIGNAL_HOLD_UPDATES;
    } else if (s_hold > 0) {
        s_hold--;
    }

    signal_led_set(s_hold > 0);
}

