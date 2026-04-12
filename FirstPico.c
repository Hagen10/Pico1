#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define MOTOR_CONTROL 15
#define BUTTON_DOWN 8
#define BUTTON_UP 9
#define SPEED_MAX 65536
#define SPEED_MIN 0
#define STEP 1000

uint16_t current_speed = SPEED_MIN;
// Tracking interrupt timestamps to prevent switch bouncing causing additional interrupts
volatile absolute_time_t last_interrupt_time = 0;
const uint DEBOUNCE_MS = 150;

void change_speed(uint gpio, uint32_t event_mask) {
    absolute_time_t now = get_absolute_time();
    if (event_mask & GPIO_IRQ_EDGE_FALL 
        && absolute_time_diff_us(last_interrupt_time, now) > DEBOUNCE_MS * 1000)
    {
        switch (gpio) {
            case BUTTON_DOWN:
                if (current_speed - STEP >= SPEED_MIN) {
                    current_speed -= STEP;
                    pwm_set_gpio_level(MOTOR_CONTROL, current_speed);
                    // pwm_set_gpio_level(MOTOR_CONTROL, 700);

                    printf("decreasing speed to: %d\n", current_speed);
                }
                break;
            case BUTTON_UP:
                if (current_speed + STEP <= SPEED_MAX) {
                    current_speed += STEP;
                    pwm_set_gpio_level(MOTOR_CONTROL, current_speed);
                    // pwm_set_gpio_level(MOTOR_CONTROL, 3200);
                    printf("increasing speed to: %d\n", current_speed);
                } 
                break;
            default:
                printf("Button Press wasn't registered properly\n");
        }
        last_interrupt_time = now;
    }
}

int main()
{
    gpio_init(MOTOR_CONTROL);
    gpio_set_function(MOTOR_CONTROL, GPIO_FUNC_PWM);

    // Additional PWM CONFIGURATION
    uint slice_num = pwm_gpio_to_slice_num(MOTOR_CONTROL);
    pwm_set_enabled(slice_num, true);

    gpio_init(BUTTON_DOWN);
    gpio_set_dir(BUTTON_DOWN, GPIO_IN);

    gpio_init(BUTTON_UP);
    gpio_set_dir(BUTTON_UP, GPIO_IN);

    gpio_set_irq_enabled_with_callback(BUTTON_DOWN, GPIO_IRQ_EDGE_FALL, true, &change_speed);
    gpio_set_irq_enabled_with_callback(BUTTON_UP, GPIO_IRQ_EDGE_FALL, true, &change_speed);


    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    while (true) {
        tight_loop_contents();
    }
}
