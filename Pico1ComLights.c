#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "string.h"

#include "zipled.pio.h"

int numLeds = 5;

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define ZIPLED 16

static bool contains(const char *text, const char *words) {
    if (strlen(words) > strlen(text)) return false;

    int j = 0;

    for (int i = 0; i < strlen(text); i++) {
        if (text[i] == words[j]) {
            if (j == strlen(words) - 1) return true;
            j++;
        }
        else j = 0;
    }

    return false;
} 

int main()
{
    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    // Initialize the onboard LED for indication of watchdog reboots
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Buffer for uart input
    char uartBuffer[20] = {0};  // Adjust size as needed
    int i = 0;

    if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1000);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        // return 0;
    } else {
        printf("Clean boot\n");
    }

    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    // Initializing the zipled program described in zipled.pio
    uint offset = pio_add_program(pio, &zipled_program);    
    zipled_program_init(pio, sm, offset, (uint)ZIPLED);

    watchdog_enable(10000, 1);

    while (true) {    
        watchdog_update();

        int i = 0;
        while (i < sizeof(uartBuffer) - 1) {
        // for (int i = 0; i < sizeof(uartBuffer); i++) {
            char c;
            uart_read_blocking(UART_ID, (uint8_t*) &c, 1);

            printf("GOT: %c - %i\n", c, (int)c);

            // Stripping string from new lines, null terminations
            if (c == '\n' || c == '\r' || c == '\0') {
                break;
            }

            uartBuffer[i++] = c;

            if (i > 0) printf("ADDING TO uartBUFFER - %c\n", uartBuffer[i - 1]);
        }

        uartBuffer[i] = '\0';  // Null-terminate string

        printf("GOT SOMETHING: %s \n", uartBuffer);

        // Check if input matches "15215"
        if (contains(uartBuffer, "START LIGHT")) {
            printf("TURNING ON LIGHTS\n");
            // BIT ORDER IS GREEN-RED-BLUE
            pio_sm_put_blocking(pio, sm, (0 << 24));
            pio_sm_put_blocking(pio, sm, (0 << 16));
            pio_sm_put_blocking(pio, sm, (0 << 8));
            pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));
            pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

            sleep_ms(50);

            const char *msg = "OK 1\n";
            uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));
        }

        // Clear buffer and index for next input
        memset(uartBuffer, 0, sizeof(uartBuffer));

        watchdog_update();

        // for (int i = 0; i < numLeds; i++) {
        //     colors[i] = (g << 16) | (r << 8) | b;
        //     uint32_t color = colors[i] << 8; // align to top 24 bits (since rgb only has 24 bits (8 bits for each of the three colors))
        //     // This is what sets the color
        //     pio_sm_put_blocking(pio, sm, color);
        // }

        // Some additional color fun

        // sleep_ms(50);

        // pio_sm_put_blocking(pio, sm, (50 << 24));
        // pio_sm_put_blocking(pio, sm, (50 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 8));
        // pio_sm_put_blocking(pio, sm, (50 << 24) | (50 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (0 << 24));
        // pio_sm_put_blocking(pio, sm, (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (0 << 24));
        // pio_sm_put_blocking(pio, sm, (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 8));
        // pio_sm_put_blocking(pio, sm, (50 << 24) | (50 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (0 << 24));
        // pio_sm_put_blocking(pio, sm, (0 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 8));
        // pio_sm_put_blocking(pio, sm, (50 << 24) | (50 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (0 << 24));
        // pio_sm_put_blocking(pio, sm, (50 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 8));
        // pio_sm_put_blocking(pio, sm, (50 << 24) | (50 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (50 << 24));
        // pio_sm_put_blocking(pio, sm, (50 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 8));
        // pio_sm_put_blocking(pio, sm, (50 << 24) | (50 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (50 << 24));
        // pio_sm_put_blocking(pio, sm, (50 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 8));
        // pio_sm_put_blocking(pio, sm, (50 << 24) | (50 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 16) | (0 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (50 << 24));
        // pio_sm_put_blocking(pio, sm, (50 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 16) | (0 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (50 << 24));
        // pio_sm_put_blocking(pio, sm, (50 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 16) | (0 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (50 << 24));
        // pio_sm_put_blocking(pio, sm, (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 16) | (0 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (0 << 24));
        // pio_sm_put_blocking(pio, sm, (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 16) | (0 << 8));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 24));
        // pio_sm_put_blocking(pio, sm, (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (0 << 24));
        // pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (0 << 24));
        // pio_sm_put_blocking(pio, sm, (0 << 16));
        // pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));

        // sleep_ms(1000);

        // pio_sm_put_blocking(pio, sm, (0 << 24));
        // pio_sm_put_blocking(pio, sm, (0 << 16));
        // pio_sm_put_blocking(pio, sm, (0 << 8));
        // pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));
        // pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));

        // sleep_ms(1000);
    }
}
