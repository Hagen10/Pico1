#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define RED_LED 14
#define ORANGE_LED 15
#define YELLOW_LED 16
#define GREEN_LED 17

uint8_t leds[] = {RED_LED, ORANGE_LED, YELLOW_LED, GREEN_LED};

#define POTENTIOMETER 26

int main()
{
    // Initialize all the LEDs
    for (int i = 0; i < sizeof(leds); i++) {
        gpio_init(leds[i]);
        gpio_set_dir(leds[i], GPIO_OUT);
    }

    adc_init();
    adc_gpio_init(POTENTIOMETER);
    adc_select_input(0);

    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    while (true) {

        uint16_t capVoltage = adc_read();

        // NOT 65535 but around 4096
        int percentage = (int)(capVoltage / 40.96);

        printf("voltage read: %d, percentage calculated: %d\n", capVoltage, percentage);

        if ((percentage > 25) && (percentage <= 50)) {
            gpio_put(RED_LED, 1);
            gpio_put(ORANGE_LED, 0);
            gpio_put(YELLOW_LED, 0);
            gpio_put(GREEN_LED, 0);
        }
        else if ((percentage > 50) && (percentage <= 75)) {
            gpio_put(RED_LED, 1);
            gpio_put(ORANGE_LED, 1);
            gpio_put(YELLOW_LED, 0);
            gpio_put(GREEN_LED, 0);
        } 
        else if ((percentage > 75) && (percentage <= 90)) {
            gpio_put(RED_LED, 1);
            gpio_put(ORANGE_LED, 1);
            gpio_put(YELLOW_LED, 1);
            gpio_put(GREEN_LED, 0);
        }
        else if (percentage > 90) {
            gpio_put(RED_LED, 1);
            gpio_put(ORANGE_LED, 1);
            gpio_put(YELLOW_LED, 1);
            gpio_put(GREEN_LED, 1);
        }
        else {
            gpio_put(RED_LED, 0);
            gpio_put(ORANGE_LED, 0);
            gpio_put(YELLOW_LED, 0);
            gpio_put(GREEN_LED, 0);
        }

        sleep_ms(50);
    }
}
