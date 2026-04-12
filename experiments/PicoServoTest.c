#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define SERVO_MOTOR 15
#define BUTTON_DOWN 8
#define BUTTON_UP 9
#define FROM_MIN 0
#define FROM_MAX 4095
#define TO_MIN 500
#define TO_MAX 3200

/*
    This was just a test to use to buttons to decrease and increase the
    rotation of the servo motor instead of doing it with a potentiometer
    as in experiment 5.
*/

uint16_t current_setting = FROM_MIN;
// Tracking interrupt timestamps to prevent switch bouncing causing additional interrupts
volatile absolute_time_t last_interrupt_time = 0;
const uint DEBOUNCE_MS = 150;

void change_servo(uint gpio, uint32_t event_mask) {
    absolute_time_t now = get_absolute_time();
    if (event_mask & GPIO_IRQ_EDGE_FALL 
        && absolute_time_diff_us(last_interrupt_time, now) > DEBOUNCE_MS * 1000)
    {
        switch (gpio) {
            case BUTTON_DOWN:
                if (current_setting - 100 >= FROM_MIN) {
                    current_setting -= 100;
                    // pwm_set_gpio_level(SERVO_MOTOR, current_setting);
                    pwm_set_gpio_level(SERVO_MOTOR, 700);

                    printf("decreasing currentSetting to: %d\n", current_setting);
                }
                break;
            case BUTTON_UP:
                if (current_setting + 100 <= FROM_MAX) {
                    current_setting += 100;
                    // pwm_set_gpio_level(SERVO_MOTOR, current_setting);
                    pwm_set_gpio_level(SERVO_MOTOR, 3200);
                    printf("increasing currentSetting to: %d\n", current_setting);
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
    gpio_init(SERVO_MOTOR);
    gpio_set_function(SERVO_MOTOR, GPIO_FUNC_PWM);

    // Additional PWM CONFIGURATION
    uint slice_num = pwm_gpio_to_slice_num(SERVO_MOTOR);
    pwm_set_enabled(slice_num, true);

    // Set the PWM frequency to 50 Hz
    pwm_set_wrap(slice_num, 24999); // TOP value

    // Set the clock divider to get 50 Hz
    pwm_set_clkdiv(slice_num, 100.0f);

    gpio_init(BUTTON_DOWN);
    gpio_set_dir(BUTTON_DOWN, GPIO_IN);

    gpio_init(BUTTON_UP);
    gpio_set_dir(BUTTON_UP, GPIO_IN);

    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    printf("SPINNING UP\n\n");
    sleep_ms(5000);

    pwm_set_gpio_level(SERVO_MOTOR, current_setting);

    gpio_set_irq_enabled_with_callback(BUTTON_DOWN, GPIO_IRQ_EDGE_FALL, true, &change_servo);
    gpio_set_irq_enabled_with_callback(BUTTON_UP, GPIO_IRQ_EDGE_FALL, true, &change_servo);

    while (true) {
        tight_loop_contents();
    }
}
