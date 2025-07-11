#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "string.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"

#include "zipled.pio.h"

int numLeds = 5;

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define ZIPLED 16

#define BUFFERSIZE 64
char uartData[BUFFERSIZE] = {0};  // Adjust size as needed
volatile int uartIndex = 0;
PIO pio = pio0;
uint sm;

typedef enum {
    NO_MESSAGE,
    FIRST_MESSAGE,
    SECOND_MESSAGE,
    THIRD_MESSAGE
} MSG_STATUS;

volatile MSG_STATUS status = NO_MESSAGE;

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

void on_uart_rx() {
    while (uart_is_readable(UART_ID)) {
        char c = uart_getc(UART_ID);
        if (c == '\n' || c == '\r' || c == '\0') {
            break;
        }

        if (uartIndex < BUFFERSIZE - 1)
            uartData[uartIndex++] = c;
        else {
            printf("UART BUFFER OVERFLOW!!");
            uartIndex = 0;
        }
    }

    uartData[uartIndex] = '\0';  // Null-terminate string

    if (contains(uartData, "START LIGHT")) {
        status = FIRST_MESSAGE;

        uartIndex = 0;
        memset(uartData, 0, sizeof(uartData));
    }
    else if (contains(uartData, "START SECOND")) {
        status = SECOND_MESSAGE;

        uartIndex = 0;
        memset(uartData, 0, sizeof(uartData));
    }
    else if (contains(uartData, "START THIRD")) {
        status = THIRD_MESSAGE;

        uartIndex = 0;
        memset(uartData, 0, sizeof(uartData));
    }
}

//
// PICO 1
//

int main()
{
    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    sm = pio_claim_unused_sm(pio, true);
    // Initializing the zipled program described in zipled.pio
    uint offset = pio_add_program(pio, &zipled_program);    
    zipled_program_init(pio, sm, offset, (uint)ZIPLED);

    // Initialize the onboard LED for indication of watchdog reboots
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Enable UART interrupt when RX data is available
    irq_set_exclusive_handler(UART1_IRQ, on_uart_rx);
    irq_set_enabled(UART1_IRQ, true);
    uart_set_irqs_enabled(UART_ID, true, false); // RX only

    if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1000);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
    } else {
        printf("Clean boot\n");
    }

    watchdog_enable(10000, 1);

    while (true) {    
        watchdog_update();

        const char *msg;

        switch (status) {
            case FIRST_MESSAGE:
                printf("FIRST UART INTERRUPT MESSAGE RECEIVED");
                pio_sm_put_blocking(pio, sm, (0 << 24));
                pio_sm_put_blocking(pio, sm, (0 << 16));
                pio_sm_put_blocking(pio, sm, (0 << 8));
                pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));
                pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

                sleep_ms(50);

                msg = "FIRST OK\n";
                uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));

                status = NO_MESSAGE;
                break;
            case SECOND_MESSAGE:
                printf("SECOND UART INTERRUPT MESSAGE RECEIVED");
                pio_sm_put_blocking(pio, sm, (0 << 24));
                pio_sm_put_blocking(pio, sm, (0 << 16));
                pio_sm_put_blocking(pio, sm, (0 << 8));
                pio_sm_put_blocking(pio, sm, (50 << 24) | (50 << 16));
                pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

                sleep_ms(50);

                msg = "SECOND OK\n";
                uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));

                status = NO_MESSAGE;
                break;
            case THIRD_MESSAGE:
                printf("THIRD UART INTERRUPT MESSAGE RECEIVED");
                pio_sm_put_blocking(pio, sm, (0 << 24));
                pio_sm_put_blocking(pio, sm, (0 << 16));
                pio_sm_put_blocking(pio, sm, (50 << 8));
                pio_sm_put_blocking(pio, sm, (50 << 24) | (50 << 16));
                pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

                sleep_ms(50);

                msg = "THIRD OK\n";
                uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));

                status = NO_MESSAGE;
                break;
            case NO_MESSAGE:
                // do nothing
                break;
            default:
                printf("Didn't recoqnize the status message\n");
                break;
        }

        printf("LOOPING\n");
        sleep_ms(2000);
    }
}
