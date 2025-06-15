#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#define PHOTO_TRANSISTOR 26

// void button_pressed(uint gpio, uint32_t event_mask) {
//     if (gpio == BUTTON_PIN && event_mask & GPIO_IRQ_EDGE_FALL)
//     {
//         gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN);
//     }
// }

int main()
{
    // // Watchdog example code
    // if (watchdog_caused_reboot()) {
    //     printf("Rebooted by Watchdog!\n");
    //     // Whatever action you may take if a watchdog caused a reboot
    // }
    
    // // Enable the watchdog, requiring the watchdog to be updated every 100ms or the chip will reboot
    // // second arg is pause on debug which means the watchdog will pause when stepping through code
    // watchdog_enable(100, 1);
    
    // // You need to call this function at least more often than the 100ms in the enable call to prevent a reboot
    // watchdog_update();

    // Initialize the LED and button
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    adc_init();
    adc_gpio_init(PHOTO_TRANSISTOR);
    adc_select_input(0);

    // gpio_init(BUTTON_PIN);
    // gpio_set_dir(BUTTON_PIN, GPIO_IN);
    // gpio_pull_down(BUTTON_PIN);  // Use internal pull-down resistor

    // gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_pressed);

    // Might need to configure it depending on how much light comes in to the room
    uint16_t lightLevelSwitchAt = 50;

    while (true) {
        uint16_t result = adc_read();
        if (result > lightLevelSwitchAt) gpio_put(PICO_DEFAULT_LED_PIN, 0);
        else gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1000);
    }
}
