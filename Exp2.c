#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#define PHOTO_TRANSISTOR 26

int main()
{
    // Initialize the LED and button
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    adc_init();
    adc_gpio_init(PHOTO_TRANSISTOR);
    adc_select_input(0);

    // Might need to configure it depending on how much light comes in to the room
    uint16_t lightLevelSwitchAt;

    // Making the light level be dependent on the current adc_read
    lightLevelSwitchAt = adc_read() * 0.8;

    // Could this be achieved with an interrupt instead?

    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    while (true) {
        uint16_t result = adc_read();
        if (result < lightLevelSwitchAt) gpio_put(PICO_DEFAULT_LED_PIN, 0);
        else gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1000);
        printf("Getting the following result: %d - lightLevelSwitchAt: %d\n\n", result, lightLevelSwitchAt);
    }
}
