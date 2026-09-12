/*
 * status_led.c — see status_led.h. Pin: Rev D §8.1.
 *
 * SPDX-License-Identifier: MIT
 */

#include "status_led.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <stdatomic.h>

#if CONFIG_OSCILLA_STATUS_LED

#define LED_GPIO        27      /* Rev D §8.1; strapping pin, driven after boot only */
#define LED_ON_LEVEL    0       /* active-low */
#define TICK_MS         20
#define FLASH_MS        40
#define BOOT_HALF_MS    100
#define KICK_STALE_MS   1000
#define TASK_PRIO       1       /* below dispatch: the LED must never compete with `stop` */

static atomic_llong s_last_kick_us;
static atomic_bool  s_ready;
static atomic_bool  s_fault;
static atomic_bool  s_activity;

static void led(bool on)
{
    gpio_set_level(LED_GPIO, on ? LED_ON_LEVEL : !LED_ON_LEVEL);
}

static void led_task(void *arg)
{
    (void)arg;
    int64_t next_beat_us = 0;
    int64_t flash_until_us = 0;

    for (;;) {
        int64_t now = esp_timer_get_time();

        if (atomic_load(&s_fault)) {
            led(true);
        } else if (!atomic_load(&s_ready)) {
            led((now / (BOOT_HALF_MS * 1000)) % 2 == 0);
        } else {
            bool alive = now - atomic_load(&s_last_kick_us) < KICK_STALE_MS * 1000;

            if (atomic_exchange(&s_activity, false)) {
                flash_until_us = now + FLASH_MS * 1000;
            }
            if (alive && now >= next_beat_us) {
                flash_until_us = now + FLASH_MS * 1000;
                next_beat_us = now + (int64_t)CONFIG_OSCILLA_STATUS_LED_PERIOD_MS * 1000;
            }
            led(alive && now < flash_until_us);
        }
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

esp_err_t status_led_start(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) return err;
    led(false);

    atomic_store(&s_last_kick_us, esp_timer_get_time());
    return xTaskCreate(led_task, "led", 2048, NULL, TASK_PRIO, NULL) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

void status_led_kick(void)     { atomic_store(&s_last_kick_us, esp_timer_get_time()); }
void status_led_ready(void)    { atomic_store(&s_ready, true); }
void status_led_activity(void) { atomic_store(&s_activity, true); }
void status_led_set_fault(bool fault) { atomic_store(&s_fault, fault); }

#else /* LED disabled: every call is a no-op */

esp_err_t status_led_start(void) { return ESP_OK; }
void status_led_kick(void) {}
void status_led_ready(void) {}
void status_led_activity(void) {}
void status_led_set_fault(bool fault) { (void)fault; }

#endif
