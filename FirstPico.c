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

    uint offset = pio_add_program(pio, &zipled_program);

    // pio_add_program_at_offset(pio, &ws2812_program, PIO_WS2812_INST_HEAD);
    zipled_program_init(pio, sm, offset, (uint)ZIPLED);
    // pio_sm_config c = zipled_program_get_default_config(offset);

    // sm_config_set_sideset_pins(&c, ZIPLED);
    // sm_config_set_out_shift(&c, false, true, 24); // shift-left, autopull, threshold 24
    // sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // // Set PIO clock divider to get ~8 MHz clock
    // // System clock (default) is 125 MHz → divider = 125 / 8 = 15.625
    // float div = clock_get_hz(clk_sys) / 8000000.0f;
    // sm_config_set_clkdiv(&c, div);

    // pio_gpio_init(pio, ZIPLED);
    // pio_sm_set_consecutive_pindirs(pio, sm, ZIPLED, 1, true);

    // pio_sm_init(pio, sm, offset, &c);
    // pio_sm_set_enabled(pio, sm, true);

    uint32_t colors[numLeds];

    while (true) {
        if (rState) {
            if (r >= 255) r = 0;
            else r += 5;
            rState = false;
            // printf("R IS NOW: %d\n", r);
            sleep_ms(100);
        }

        if (gState) {
            if (g >= 255) g = 0;
            else g += 5;
            gState = false;
            // printf("G IS NOW: %d\n", g);
            sleep_ms(100);
        }

        if (bState) {
            if (b >= 255) b = 0;
            else b += 5;
            bState = false;
            // printf("B IS NOW: %d\n", b);
            sleep_ms(100);
        }

        uint elems = pio_sm_get_tx_fifo_level(pio, sm);

        printf("FIFO QUEUE 1: %d\n", elems);

        // pio_sm_put_blocking(pio, sm, 0xFF000000);

        for (int i = 0; i < numLeds; i++) {
            colors[i] = (g << 16) | (r << 8) | b;
            uint32_t color = colors[i] << 8; // align to top 24 bits
            // uint32_t color = colors[i];
            // if (i == 0) printf("COLOR %d IS: %d\n", i, color);
            // pio_sm_put_blocking(pio, sm, color);
            // pio->txf[sm] = color << 8u;
            // pio_sm_put(pio, sm, color);
        }

        elems = pio_sm_get_tx_fifo_level(pio, sm);

        printf("FIFO QUEUE 2: %d\n", elems);

        printf("FIFO FULL?: %d\n", pio_sm_is_tx_fifo_full(pio, sm));
        printf("FIFO EMPTY?: %d\n", pio_sm_is_tx_fifo_empty(pio, sm));



        // Ensure the FIFO has room before blasting
        // for (int i = 0; i < numLeds; i++) {
        //     while (pio_sm_is_tx_fifo_full(pio, sm));
        //     pio->txf[sm] = colors[i];
        //     // pio->txf[sm] = 0x00FF00;
        // }

        uint32_t test_color = (0x10 << 16) | (0x00 << 8) | 0x00;
        pio_sm_put_blocking(pio, sm, test_color);

        sleep_us(80);

        // sleep_ms(1000);
    }
}
