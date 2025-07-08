#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "string.h"

int numLeds = 5;

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define ZIPLED 16

static int contains(const char *text, const char *words) {
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
    char uartBuffer[16] = {0};  // Adjust size as needed
    int i = 0;

    // sleep_ms(5000);

    if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1000);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        // return 0;
    } else {
        printf("Clean boot\n");
    }
    watchdog_enable(4000, 1);

    while (true) {    
        watchdog_update();

        const char *msg = "START LIGHT\n";
        uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));

        int i = 0;
        while (i < sizeof(uartBuffer) - 1) {
        // for (int i = 0; i < sizeof(uartBuffer); i++) {
            char c;
            uart_read_blocking(UART_ID, (uint8_t*) &c, 1);

            if (c == '\n' || c == '\r' || c == '\0') {
                break;
            }

            uartBuffer[i++] = c;
        }

        uartBuffer[i] = '\0';  // Null-terminate string

        // Check if input matches "15215"
        if (contains(uartBuffer, "OK")) {
            printf("GOT CONFIRMATION FROM PICO1\n");

            gpio_put(PICO_DEFAULT_LED_PIN, 1);
            sleep_ms(250);
            gpio_put(PICO_DEFAULT_LED_PIN, 0);
            sleep_ms(250);
            gpio_put(PICO_DEFAULT_LED_PIN, 1);
            sleep_ms(250);
            gpio_put(PICO_DEFAULT_LED_PIN, 0);
            sleep_ms(250);
            gpio_put(PICO_DEFAULT_LED_PIN, 1);
            sleep_ms(250);
            gpio_put(PICO_DEFAULT_LED_PIN, 0);
        }

        // Clear buffer and index for next input
        memset(uartBuffer, 0, sizeof(uartBuffer));

        watchdog_update();
    }
}
