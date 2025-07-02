#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define SERVO_MOTOR 15
#define POTENTIOMETER 26
#define FROM_MIN 0
#define FROM_MAX 4095
#define TO_MIN 500
#define TO_MAX 3200

uint16_t scale(uint16_t value) {
    return TO_MIN + (((value - FROM_MIN) * (TO_MAX - TO_MIN)) / (FROM_MAX - FROM_MIN));
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

    adc_init();
    adc_gpio_init(POTENTIOMETER);
    adc_select_input(0);

    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    printf("SPINNING UP\n\n");
    sleep_ms(5000);

    while (true) {
        uint16_t pot_result = adc_read();
        
        uint16_t scaledValue = scale(pot_result);
        printf("POT_VALUE: %d -- scaledValue: %d\n", pot_result, scaledValue);

        pwm_set_gpio_level(SERVO_MOTOR, scaledValue);
        sleep_ms(50);
    }
}
