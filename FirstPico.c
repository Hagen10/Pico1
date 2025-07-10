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

int numLeds = 5;

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define ZIPLED 16

#define I2C_RECEIVER 0x42
#define I2C_SDA 2
#define I2C_SCL 3

volatile bool sendUART = true;
char uartData[16] = {0};  // Adjust size as needed


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

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

// UART RX handler
void on_uart_rx() {
    printf("INTERRUPT TRIGGERED\n");
    int i = 0;
    while (uart_is_readable(UART_ID)) {
        char c = uart_getc(UART_ID);
        printf("UART received: %c\n", c);
        if (c == '\n' || c == '\r' || c == '\0') {
            break;
        }

        uartData[i++] = c;
    }

    uartData[i] = '\0';  // Null-terminate string

     // Check if input matches "15215"
    if (contains(uartData, "OK")) {
        printf("GOT CONFIRMATION FROM PICO1\n");

        sendUART = false;

        // gpio_put(PICO_DEFAULT_LED_PIN, 1);
        // sleep_ms(250);
        // gpio_put(PICO_DEFAULT_LED_PIN, 0);
        // sleep_ms(250);
        // gpio_put(PICO_DEFAULT_LED_PIN, 1);
        // sleep_ms(250);
        // gpio_put(PICO_DEFAULT_LED_PIN, 0);
        // sleep_ms(250);
        // gpio_put(PICO_DEFAULT_LED_PIN, 1);
        // sleep_ms(250);
        // gpio_put(PICO_DEFAULT_LED_PIN, 0);

        const char *msg = "START I2C\n";
        i2c_write_blocking(i2c_default, I2C_RECEIVER, (const uint8_t *)msg, strlen(msg), false);
    }

    memset(uartData, 0, sizeof(uartData));
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

    // Buffer for uart input
    // char uartBuffer[16] = {0};  // Adjust size as needed
    // int i = 0;

    // I2C setup
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));

    // printf("\nI2C Bus Scan\n");
    // printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
 
    // for (int addr = 0; addr < (1 << 7); ++addr) {
    //     if (addr % 16 == 0) {
    //         printf("%02x ", addr);
    //     }
 
    //     // Perform a 1-byte dummy read from the probe address. If a slave
    //     // acknowledges this address, the function returns the number of bytes
    //     // transferred. If the address byte is ignored, the function returns
    //     // -1.
 
    //     // Skip over any reserved addresses.
    //     int ret;
    //     uint8_t rxdata;
    //     if (reserved_addr(addr))
    //         ret = PICO_ERROR_GENERIC;
    //     else
    //         ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);
 
    //     printf(ret < 0 ? "." : "@");
    //     printf(addr % 16 == 15 ? "\n" : "  ");
    // }
    // printf("Done.\n");


    if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1000);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        // return 0;
    } else {
        printf("Clean boot\n");
    }
    // watchdog_enable(4000, 1);

    while (true) {    
        // watchdog_update();

        if (sendUART) {
            const char *msg = "START LIGHT\n";
            uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));

            printf("SENT UART MESSAGE\n");
        }
        // int i = 0;
        // while (i < sizeof(uartBuffer) - 1) {
        // // for (int i = 0; i < sizeof(uartBuffer); i++) {
        //     char c;
        //     uart_read_blocking(UART_ID, (uint8_t*) &c, 1);

        //     // uart_getc

        //     if (c == '\n' || c == '\r' || c == '\0') {
        //         break;
        //     }

        //     uartBuffer[i++] = c;
        // }

        // uartBuffer[i] = '\0';  // Null-terminate string

        // // Check if input matches "15215"
        // if (contains(uartBuffer, "OK")) {
        //     printf("GOT CONFIRMATION FROM PICO1\n");

        //     gpio_put(PICO_DEFAULT_LED_PIN, 1);
        //     sleep_ms(250);
        //     gpio_put(PICO_DEFAULT_LED_PIN, 0);
        //     sleep_ms(250);
        //     gpio_put(PICO_DEFAULT_LED_PIN, 1);
        //     sleep_ms(250);
        //     gpio_put(PICO_DEFAULT_LED_PIN, 0);
        //     sleep_ms(250);
        //     gpio_put(PICO_DEFAULT_LED_PIN, 1);
        //     sleep_ms(250);
        //     gpio_put(PICO_DEFAULT_LED_PIN, 0);

        //     const char *msg = "START I2C\n";
        //     i2c_write_blocking(i2c_default, 0x42, (const uint8_t *)msg, strlen(msg), false);

        // }

        // // Clear buffer and index for next input
        // memset(uartBuffer, 0, sizeof(uartBuffer));

        // watchdog_update();

        sleep_ms(5000);
    }
}
