#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "ssd1306/ssd1306.h"

#define SDA_PIN 4
#define SCL_PIN 5
#define I2C_PORT i2c0
#define OLED_ADDR 0x3C
#define SLEEPTIME 25

void scan_i2c() {
    printf("I2C scan start...\n");

    for (int addr = 0; addr < 127; addr++) {
        uint8_t rxdata;
        int result = i2c_read_blocking(I2C_PORT, addr, &rxdata, 1, false);

        if (result >= 0) {
            printf("Device found at 0x%02X\n", addr);
            sleep_ms(1000);
        }
    }

    printf("Scan done.\n");
}

void i2c_init_oled() {
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz fast mode

    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
}

void animation(void) {
    const char *words[]= {"SSD1306", "DISPLAY", "DRIVER"};

    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, OLED_ADDR, I2C_PORT);
    ssd1306_clear(&disp);

    printf("ANIMATION!\n");

    char buf[8];

    for(;;) {
        for(int y=0; y<31; ++y) {
            ssd1306_draw_line(&disp, 0, y, 127, y);
            ssd1306_show(&disp);
            sleep_ms(SLEEPTIME);
            ssd1306_clear(&disp);
        }

        for(int y=0, i=1; y>=0; y+=i) {
            ssd1306_draw_line(&disp, 0, 31-y, 127, 31+y);
            ssd1306_draw_line(&disp, 0, 31+y, 127, 31-y);
            ssd1306_show(&disp);
            sleep_ms(SLEEPTIME);
            ssd1306_clear(&disp);
            if(y==32) i=-1;
        }

        for(int i=0; i<sizeof(words)/sizeof(char *); ++i) {
            ssd1306_draw_string(&disp, 8, 24, 2, words[i]);
            ssd1306_show(&disp);
            sleep_ms(800);
            ssd1306_clear(&disp);
        }

        for(int y=31; y<63; ++y) {
            ssd1306_draw_line(&disp, 0, y, 127, y);
            ssd1306_show(&disp);
            sleep_ms(SLEEPTIME);
            ssd1306_clear(&disp);
        }

        ssd1306_draw_string(&disp, 5, 40, 1, "HELLO WORLD");
        ssd1306_show(&disp);
        sleep_ms(2000);
        ssd1306_clear(&disp);

        for (int i = 0; i < 128; ++i) {
            ssd1306_draw_string(&disp, i, 40, 1, "HELLO WORLD");
            ssd1306_show(&disp);
            sleep_ms(SLEEPTIME);
            ssd1306_clear(&disp);
        }

        for (int i = 0; i < 64; ++i) {
            ssd1306_draw_string(&disp, 5, i, 1, "HELLO WORLD");
            ssd1306_show(&disp);
            sleep_ms(SLEEPTIME);
            ssd1306_clear(&disp);
        }

        sleep_ms(2000);
    }
}

int main()
{
    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    // Initialize the LED and button
    // gpio_init(PICO_DEFAULT_LED_PIN);
    // gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    // gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN);

    i2c_init_oled();

    while (true) {
        animation();

        sleep_ms(1000);
    //     tight_loop_contents();
    }
}
