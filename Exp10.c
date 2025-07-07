#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "zipled.pio.h"

int numLeds = 5;

#define RBUTTON 13
#define GBUTTON 14
#define BBUTTON 15

#define ZIPLED 16

uint8_t buttons[] = {RBUTTON, GBUTTON, BBUTTON};

volatile bool rState = false, gState = false, bState = false;

void button_pressed(uint gpio, uint32_t event_mask) {
    if (event_mask & GPIO_IRQ_EDGE_FALL) {
        switch (gpio) {
            case RBUTTON:
                rState = true;
                break;
            case GBUTTON:
                gState = true;
                break;
            case BBUTTON:
                bState = true;
                break;
            default:
                printf("Button Press wasn't registered properly\n");
                break;
        }
    }
}

int main()
{
    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    // Set the initial colours to have the ZIP LEDs all off
    uint32_t r = 0, g = 0, b = 0;

    uint32_t colors[numLeds];

    // Initialize buttons
    for (int i = 0; i < sizeof(buttons); i++) {
        gpio_init(buttons[i]);
        gpio_set_dir(buttons[i], GPIO_IN);
        gpio_pull_down(buttons[i]);  // Use internal pull-down resistor

        //Setting Interrupt for each of the button pins.
        gpio_set_irq_enabled_with_callback(buttons[i], GPIO_IRQ_EDGE_FALL, true, &button_pressed);
    }

    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    // Initializing the zipled program described in zipled.pio
    uint offset = pio_add_program(pio, &zipled_program);    
    zipled_program_init(pio, sm, offset, (uint)ZIPLED);

    while (true) {
        if (rState) {
            if (r >= 255) r = 0;
            else r += 5;
            rState = false;
        }

        if (gState) {
            if (g >= 255) g = 0;
            else g += 5;
            gState = false;
        }

        if (bState) {
            if (b >= 255) b = 0;
            else b += 5;
            bState = false;
        }

        for (int i = 0; i < numLeds; i++) {
            colors[i] = (g << 16) | (r << 8) | b;
            uint32_t color = colors[i] << 8; // align to top 24 bits (since rgb only has 24 bits (8 bits for each of the three colors))
            // This is what sets the color
            pio_sm_put_blocking(pio, sm, color);
        }

        sleep_ms(50);
    }
}
