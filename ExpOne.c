#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/interp.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"

// My Stuff
#define BUTTON_PIN 15

void button_pressed(uint gpio, uint32_t event_mask) {
    if (gpio == BUTTON_PIN && event_mask & GPIO_IRQ_EDGE_FALL)
    {
        gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN);
    }
}

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

    // Starts at 
    volatile bool led_status = 0;
    // Initialize the LED and button
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_down(BUTTON_PIN);  // Use internal pull-down resistor

    printf("System Clock Frequency is %d Hz\n", clock_get_hz(clk_sys));
    printf("USB Clock Frequency is %d Hz\n", clock_get_hz(clk_usb));
    // For more examples of clocks use see https://github.com/raspberrypi/pico-examples/tree/master/clocks

    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_pressed);

    while (true) {
        sleep_ms(10000);
    }
}
