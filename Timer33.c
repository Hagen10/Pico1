#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "string.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"

#define RED_LED 13
#define GREEN_LED 14

#define START_BUTTON 15
// 33 Minutes
#define TIME 1980000

uint8_t leds[] = {RED_LED, GREEN_LED};

volatile alarm_id_t timer_id = -1;

// Tracking interrupt timestamps to prevent switch bouncing causing additional interrupts
volatile absolute_time_t last_interrupt_time = 0;
const uint DEBOUNCE_MS = 150;

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    printf("%d seconds have passed!\n", TIME);
    gpio_put(RED_LED, 1);
    return 0; // 0 means one-shot; non-zero would reschedule
}

void button_pressed(uint gpio, uint32_t event_mask) {
    absolute_time_t now = get_absolute_time();
    printf("BUTTON PRESSED\n");
    
    if (event_mask & GPIO_IRQ_EDGE_FALL
        && absolute_time_diff_us(last_interrupt_time, now) > DEBOUNCE_MS * 1000) {
        
        if (timer_id != -1) {
            // Reset timer
            cancel_alarm(timer_id);
            timer_id = -1;
            gpio_put(GREEN_LED, 1);
            gpio_put(RED_LED, 0);
        }
        else {
            // start timer
            timer_id = add_alarm_in_ms(TIME, alarm_callback, NULL, false);
            gpio_put(GREEN_LED, 0);
            gpio_put(RED_LED, 0);
        }

        last_interrupt_time = now;
    }
}

//
// PICO 1
//

int main()
{
    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    // Initialize all the LEDs
    for (int i = 0; i < sizeof(leds); i++) {
        gpio_init(leds[i]);
        gpio_set_dir(leds[i], GPIO_OUT);
    }

    // Initializing button
    gpio_init(START_BUTTON);
    gpio_set_dir(START_BUTTON, GPIO_IN);
    gpio_pull_down(START_BUTTON);  // Use internal pull-down resistor
    gpio_set_irq_enabled_with_callback(START_BUTTON, GPIO_IRQ_EDGE_FALL, true, &button_pressed);

    // Timer is ready to be started
    gpio_put(GREEN_LED, 1);

    while (true) {    
        tight_loop_contents();
    }
}
