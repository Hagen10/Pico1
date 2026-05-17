#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define POTENTIOMETER 26
#define RED_LED 16
#define SWITCH 15

volatile bool buttonState = false;
// Tracking interrupt timestamps to prevent switch bouncing causing additional interrupts
volatile absolute_time_t last_interrupt_time = 0;
const uint DEBOUNCE_MS = 150;

void switch_pressed(uint gpio, uint32_t event_mask) {
    absolute_time_t now = get_absolute_time();
    printf("BUTTON PRESSED\n");
    if (gpio == SWITCH 
        && event_mask & GPIO_IRQ_EDGE_FALL 
        // to prevent switch bouncing which triggers multiple interrupts
        && absolute_time_diff_us(last_interrupt_time, now) > DEBOUNCE_MS * 1000)
    {
        // Set it to the opposite state since it was pressed
        buttonState = !buttonState;
        last_interrupt_time = now;
        printf("New state: %d\n", buttonState);
    }
}

int main()
{
    gpio_init(RED_LED);
    gpio_set_function(RED_LED, GPIO_FUNC_PWM);

    // Additional PWM CONFIGURATION
    uint slice_num = pwm_gpio_to_slice_num(RED_LED);
    pwm_set_enabled(slice_num, true);

    adc_init();
    adc_gpio_init(POTENTIOMETER);
    adc_select_input(0);

    gpio_init(SWITCH);
    gpio_set_dir(SWITCH, GPIO_IN);
    gpio_pull_down(SWITCH);  // Use internal pull-down resistor
    gpio_set_irq_enabled_with_callback(SWITCH, GPIO_IRQ_EDGE_FALL, true, &switch_pressed);

    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    while (true) {
        uint16_t result = adc_read();
        if (buttonState) {
            // printf("Setting PWM to %d\n", result);
            pwm_set_gpio_level(RED_LED, result);
        }
        else {
            pwm_set_gpio_level(RED_LED, 0);
        }
    }
}
