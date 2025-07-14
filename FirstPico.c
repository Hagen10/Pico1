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

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define ZIPLED 16

#define BUFFERSIZE 64

typedef enum {
    SEND_FIRST,
    SEND_SECOND,
    SEND_THIRD,
    FINISHED
} TASK_STATUS;

volatile TASK_STATUS status = SEND_FIRST;

char uartData[BUFFERSIZE] = {0};  // Adjust size as needed
volatile int uartIndex = 0;

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


// UART RX handler
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

    if (contains(uartData, "FIRST OK")) {
        status = SEND_SECOND;

        uartIndex = 0;
        memset(uartData, 0, sizeof(uartData));
    }
    else if (contains(uartData, "SECOND OK")) {
        status = SEND_THIRD;

        uartIndex = 0;
        memset(uartData, 0, sizeof(uartData));
    }
    else if (contains(uartData, "THIRD OK")) {
        status = FINISHED;

        uartIndex = 0;
        memset(uartData, 0, sizeof(uartData));
    }
}

//
// PICO 2
//

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
            case SEND_FIRST:
                msg = "START LIGHT\n";
                uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));

                printf("SENT 1st UART MESSAGE\n");
                break;

            case SEND_SECOND:
                msg = "START SECOND\n";
                uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));

                printf("SENT 2nd UART MESSAGE\n");
                break;
            
            case SEND_THIRD:
                msg = "START THIRD\n";
                uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));

                printf("SENT 3rd UART MESSAGE\n");
                break;

            case FINISHED:
                printf("Communication Finished\n");
                gpio_put(PICO_DEFAULT_LED_PIN, 1);
                break;

        }

        watchdog_update();

        sleep_ms(1000);
    }
}
