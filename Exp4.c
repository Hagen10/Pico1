#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define MOTOR_CONTROL 15

int main()
{
    gpio_init(MOTOR_CONTROL);
    gpio_set_function(MOTOR_CONTROL, GPIO_FUNC_PWM);

    // Additional PWM CONFIGURATION
    uint slice_num = pwm_gpio_to_slice_num(MOTOR_CONTROL);
    pwm_set_enabled(slice_num, true);

    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    while (true) {
        printf("SPINNING UP\n\n");
        for (int i = 0; i < 65536; i += 100) {
            pwm_set_gpio_level(MOTOR_CONTROL, i);
            sleep_ms(5);
        }

        sleep_ms(2500);
        printf("SPINNING DOWN\n\n");

        for (int i = 65535; i > 0; i -= 100) {
            pwm_set_gpio_level(MOTOR_CONTROL, i);
            sleep_ms(5);
        }

        sleep_ms(2500);
    }
}
