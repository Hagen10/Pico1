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

#define I2C_ADDRESS 0x42
#define I2C_SDA 2
#define I2C_SCL 3

#define BUFFERSIZE 64
char uartData[BUFFERSIZE] = {0};  // Adjust size as needed
volatile int uartIndex = 0;
PIO pio = pio0;
uint sm;

volatile bool uart_received = false;

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

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void on_uart_rx() {
    // printf("INTERRUPT TRIGGERED\n");
    // int i = 0;
    while (uart_is_readable(UART_ID)) {
        char c = uart_getc(UART_ID);
        // printf("UART received: %c\n", c);
        if (c == '\n' || c == '\r' || c == '\0') {
            break;
        }

        if (uartIndex < BUFFERSIZE - 1)
            uartData[uartIndex++] = c;
        else printf("UART BUFFER OVERFLOW!!");
    }

    uartData[uartIndex] = '\0';  // Null-terminate string

    // Check if input matches "15215"
    if (contains(uartData, "START LIGHT")) {
        uart_received = true;

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

    char i2cBuffer[20] = {0};

    // I2C setup
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));

    // Enable slave mode
    i2c_set_slave_mode(i2c_default, true, I2C_ADDRESS);

    if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(1000);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        // return 0;
    } else {
        printf("Clean boot\n");
    }

    // watchdog_enable(10000, 1);

    while (true) {    
        // watchdog_update();

        if (uart_received) {
            printf("INTERRUPT MESSAGE RECEIVED");
            pio_sm_put_blocking(pio, sm, (0 << 24));
            pio_sm_put_blocking(pio, sm, (0 << 16));
            pio_sm_put_blocking(pio, sm, (0 << 8));
            pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));
            pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

            sleep_ms(50);

            const char *msg = "OK\n";
            uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));

            uart_received = false;
        }

        // // Check if input matches "15215"
        // if (contains(uartBuffer, "START LIGHT")) {
        //     printf("TURNING ON LIGHTS\n");
        //     // BIT ORDER IS GREEN-RED-BLUE
        //     pio_sm_put_blocking(pio, sm, (0 << 24));
        //     pio_sm_put_blocking(pio, sm, (0 << 16));
        //     pio_sm_put_blocking(pio, sm, (0 << 8));
        //     pio_sm_put_blocking(pio, sm, (0 << 24) | (0 << 16));
        //     pio_sm_put_blocking(pio, sm, (50 << 16) | (50 << 8));

        //     sleep_ms(50);

        //     const char *msg = "OK 1\n";
        //     uart_write_blocking(UART_ID, (const uint8_t *)msg, strlen(msg));


        //     i = 0;
        //     while (i < sizeof(i2cBuffer) - 1) {
        //         char c;
        //         i2c_read_raw_blocking(i2c_default, (uint8_t*) &c, 1);

        //         printf("GOT: %c - %i\n", c, (int)c);

        //         // Stripping string from new lines, null terminations
        //         if (c == '\n' || c == '\r' || c == '\0') {
        //             break;
        //         }

        //         i2cBuffer[i++] = c;

        //         if (i > 0) printf("ADDING TO i2cBUFFER - %c\n", i2cBuffer[i - 1]);
        //     }

        // i2cBuffer[i] = '\0';  // Null-terminate string

        // printf("GOT SOMETHING: %s \n", i2cBuffer);
        // }

        // Clear buffer and index for next input
        // memset(uartBuffer, 0, sizeof(uartBuffer));

        // memset(i2cBuffer, 0, sizeof(i2cBuffer));


        // watchdog_update();

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

        printf("LOOPING\n");
        sleep_ms(1000);
    }
}
