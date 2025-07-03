#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define SEGA 18
#define SEGB 17
#define SEGC 16
#define SEGD 15
#define SEGE 14
#define SEGF 13
#define SEGG 12

uint8_t pins[] = {SEGA, SEGB, SEGC, SEGD, SEGE, SEGF, SEGG};

uint8_t digits[11][7] = {
    {1,1,1,1,1,1,0},
    {0,1,1,0,0,0,0},
    {1,1,0,1,1,0,1},
    {1,1,1,1,0,0,1},
    {0,1,1,0,0,1,1},
    {1,0,1,1,0,1,1},
    {1,0,1,1,1,1,1},
    {1,1,1,0,0,0,0},
    {1,1,1,1,1,1,1},
    {1,1,1,0,0,1,1},
    {0,0,0,0,0,0,0}
};

void displayNumber(uint8_t number) {
    uint8_t pin = 0;
    for (int segment = 0; segment < sizeof(pins); segment++) {
        gpio_put(pins[pin], digits[number][segment]);
        pin++;
    }
}

int main()
{
    for (int i = 0; i < sizeof(pins); i++) {
        // Initialize the LED and button
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
    }

    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    while (true) {
        for (int i = 0; i < 10; i++) {
            displayNumber(i);
            sleep_ms(500);
        }

        for (int i = 9; i >= 0; i--) {
            displayNumber(i);
            sleep_ms(500);
        }
    }
}
