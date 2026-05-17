#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define BUZZER 15
#define POTENTIOMETER 26
#define FROM_MIN 0
#define FROM_MAX 4095
#define TO_MIN 32000
#define TO_MAX 65535

volatile uint16_t prev_frequency = 0;

#define FREQUENCY_DIFFERENT(val) ((val) < (prev_frequency - 50) || (val) > (prev_frequency + 50))

uint16_t scale(uint16_t value) {
    return TO_MIN + (((value - FROM_MIN) * (TO_MAX - TO_MIN)) / (FROM_MAX - FROM_MIN));
}

int main()
{
    gpio_init(BUZZER);
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);

    // Additional PWM CONFIGURATION
    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_enabled(slice_num, true);

    adc_init();
    adc_gpio_init(POTENTIOMETER);
    adc_select_input(0);

    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    // Setting a 50% duty cycle (half of 65535)
    pwm_set_gpio_level(BUZZER, 32767);

    while (true) {
        uint16_t pot_result = adc_read();
        
        uint16_t scaledValue = scale(pot_result);

        if ((FREQUENCY_DIFFERENT(scaledValue))) {
            printf("POT_VALUE: %d -- scaledValue: %d\n", pot_result, scaledValue);

            pwm_set_wrap(slice_num, scaledValue);
            prev_frequency = scaledValue;
        }
        sleep_ms(50);
    }
}
