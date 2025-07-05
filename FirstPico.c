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

#define PROPELLER 26
#define FROM_MIN 100
#define FROM_MAX 1000
#define TO_MIN 0
#define TO_MAX 9

uint16_t scale(uint16_t value) {
    return TO_MIN + (((value - FROM_MIN) * (TO_MAX - TO_MIN)) / (FROM_MAX - FROM_MIN));
}

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
        // sleep_ms(50);
        pin++;
    }
}

// void loadingSymbol(void) {
//     for (int pin = 0; pin < sizeof(pins) - 1; pin++) {
//         gpio_put(pins[pin], 1);
//         sleep_ms(100);
//     }

//     for (int pin = 0; pin < sizeof(pins) - 1; pin++) {
//         gpio_put(pins[pin], 0);
//         sleep_ms(100);
//     }
// }

// Lights up one segment at a time when looping through the passed array
// void spinningLight(uint8_t arr[], uint8_t arrSize) {
//     uint8_t lastSegment = 0;

//     for (int pin = 0; pin < arrSize; pin++) {
//         gpio_put(arr[pin], 1);
//         // Turning off the light for the previous element
//         lastSegment = pin - 1 >= 0 ? arr[pin - 1] : arr[arrSize - 1];
//         gpio_put(lastSegment, 0);
//         sleep_ms(50);
//     }

//     // Setting the last turned on element back to 0
//     gpio_put(lastSegment + 1, 0);
// }

int main()
{
    for (int i = 0; i < sizeof(pins); i++) {
        // Initialize the LED and button
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
    }

    adc_init();
    adc_gpio_init(PROPELLER);
    adc_select_input(0);

    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    for (int i = 0; i < 10; i++) {
        displayNumber(i);
        sleep_ms(100);
    }

    while (true) {

        uint16_t speed = adc_read();

        uint16_t calcSpeed = scale(speed);

        printf("read speed: %d, calc speed: %d\n", speed, calcSpeed);

        displayNumber(calcSpeed);

        sleep_ms(200);

        // for (int i = 0; i < 10; i++) {
        //     displayNumber(i);
        //     sleep_ms(500);
        // }

        // for (int i = 9; i >= 0; i--) {
        //     displayNumber(i);
        //     sleep_ms(500);
        // }

        // // More and more segments light up in this method
        // for (int i = 0; i < 5; i++) loadingSymbol();

        // //Only one segment lights up at a time. We're passing the size of pins - 1 
        // // because we don't want the middle segment (SEGG) to light up for the pins array.
        // for (int i = 0; i < 10; i++) spinningLight(pins, sizeof(pins) - 1);
        // // Different style
        // for (int i = 0; i < 10; i++) spinningLight(altPins, sizeof(altPins));
        // // Another different style
        // for (int i = 0; i < 10; i++) spinningLight(altPins2, sizeof(altPins2));
    }
}
