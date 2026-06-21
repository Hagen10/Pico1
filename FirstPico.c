#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include <string.h>

#define GREEN_LED 16
#define SWITCH 5
#define RED_LED 15

const uint32_t EDGE_DEBOUNCE_US = 30000;       // 30 ms
const uint32_t DOT_DASH_THRESHOLD_US = 250000; // 250 ms
#define MESSAGE_TIMEOUT_MS 1400

const char target_sos[] = "...---...";

volatile absolute_time_t last_edge_time = {0};
volatile absolute_time_t press_start_time = {0};
volatile absolute_time_t last_symbol_time = {0};
volatile bool waiting_for_release = false;
volatile bool message_ready = false;

char morse_buffer[32];
volatile int morse_len = 0;

void switch_pressed(uint gpio, uint32_t event_mask)
{
    if (gpio != SWITCH)
        return;

    absolute_time_t now = get_absolute_time();
    uint64_t dt = absolute_time_diff_us(last_edge_time, now);
    if (dt < EDGE_DEBOUNCE_US)
        return;
    last_edge_time = now;

    if (event_mask & GPIO_IRQ_EDGE_FALL)
    {
        press_start_time = now;
        pwm_set_gpio_level(RED_LED, 65000);
        waiting_for_release = true;
        return;
    }

    if ((event_mask & GPIO_IRQ_EDGE_RISE) && waiting_for_release)
    {
        uint64_t press_us = absolute_time_diff_us(press_start_time, now);

        if (morse_len < (int)sizeof(morse_buffer) - 1)
        {
            pwm_set_gpio_level(RED_LED, 0);
            morse_buffer[morse_len++] = press_us < DOT_DASH_THRESHOLD_US ? '.' : '-';
            morse_buffer[morse_len] = '\0';
            printf("MORSE: %s\n", morse_buffer);
            last_symbol_time = now;
        }
        waiting_for_release = false;
    }
}

void blink_symbol(bool dash)
{
    pwm_set_gpio_level(GREEN_LED, 65000);
    sleep_ms(dash ? 800 : 100);
    pwm_set_gpio_level(GREEN_LED, 0);
    sleep_ms(200);
}

void send_morse_ok(void)
{
    printf("Printing answer!\n");
    const char *ok = "--- -.-";
    for (const char *p = ok; *p; ++p)
    {
        if (*p == '.')
            blink_symbol(false);
        else if (*p == '-')
            blink_symbol(true);
        else
            sleep_ms(200);
    }
}

int main()
{
    gpio_init(GREEN_LED);
    gpio_set_function(GREEN_LED, GPIO_FUNC_PWM);

    gpio_init(RED_LED);
    gpio_set_function(RED_LED, GPIO_FUNC_PWM);

    // Additional PWM CONFIGURATION
    uint slice_num = pwm_gpio_to_slice_num(GREEN_LED);
    pwm_set_enabled(slice_num, true);

    uint slice2_num = pwm_gpio_to_slice_num(RED_LED);
    pwm_set_enabled(slice2_num, true);

    gpio_init(SWITCH);
    gpio_set_dir(SWITCH, GPIO_IN);
    gpio_pull_up(SWITCH); // Use internal pull-down resistor
    gpio_set_irq_enabled_with_callback(SWITCH, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &switch_pressed);

    // Needed for getting logs from `printf` via USB
    stdio_init_all();

    while (true)
    {
        absolute_time_t now = get_absolute_time();

        if (morse_len > 0 &&
            absolute_time_diff_us(last_symbol_time, now) > MESSAGE_TIMEOUT_MS * 1000)
        {
            printf("Starting new listening session!\n");
            morse_buffer[morse_len] = '\0';
            if (strcmp(morse_buffer, target_sos) == 0)
            {
                message_ready = true;
            }
            morse_len = 0;
        }

        if (message_ready)
        {
            send_morse_ok();
            message_ready = false;
        }

        sleep_ms(50);
    }
}
