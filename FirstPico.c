#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "ssd1306/ssd1306.h"
#include "ICM20948/ICM20948_register.h"

#define SDA_PIN 4
#define SCL_PIN 5
#define I2C_PORT i2c0
#define OLED_ADDR 0x3C
#define IMU_ADDR 0x68
#define SLEEPTIME 25

ssd1306_t disp;

static char text[32];

// Tracking interrupt timestamps to prevent switch bouncing causing additional interrupts
volatile absolute_time_t last_interrupt_time = 0;
const uint DEBOUNCE_MS = 150;

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

void i2c_setup() {
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz fast mode

    // pins for OLED and IMU
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
}

void init_oled(void) {
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, OLED_ADDR, I2C_PORT);
    ssd1306_clear(&disp);
}

void animation(void) {
    const char *words[]= {"SSD1306", "DISPLAY", "DRIVER"};

    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, OLED_ADDR, I2C_PORT);
    ssd1306_clear(&disp);

    printf("ANIMATION!\n");

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

// Write to IMU register 
void write_reg(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    i2c_write_blocking(i2c, addr, buf, 2, false);
}

// Read IMU register
void read_regs(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
    i2c_write_blocking(i2c, addr, &reg, 1, true);
    i2c_read_blocking(i2c, addr, buf, len, false);
}

// Select bank
void select_bank(i2c_inst_t *i2c, uint8_t addr, uint8_t bank) {
    write_reg(i2c, addr, REG_BANK_SEL, bank << 4);
}

void icm20948_init(i2c_inst_t *i2c, uint8_t addr) {
    sleep_ms(100);

    // Reset device
    select_bank(i2c, addr, BANK0);
    write_reg(i2c, addr, 0x06, 0x80);
    sleep_ms(100);

    // Wake up (clear sleep bit)
    write_reg(i2c, addr, 0x06, 0x01);

    // Optional: configure accel/gyro (Bank 2)
    select_bank(i2c, addr, BANK2);

    // Gyro config (±250 dps)
    write_reg(i2c, addr, 0x01, 0x00);

    // Accel config (±2g)
    write_reg(i2c, addr, 0x14, 0x00);

    // Back to bank 0
    select_bank(i2c, addr, BANK0);
}

void read_imu(i2c_inst_t *i2c, uint8_t addr) {
    uint8_t buf[12];

    select_bank(i2c, addr, BANK0);

    // Read accel (6 bytes) + gyro (6 bytes)
    read_regs(i2c, addr, 0x2D, buf, 12);

    int16_t ax = (buf[0] << 8) | buf[1];
    int16_t ay = (buf[2] << 8) | buf[3];
    int16_t az = (buf[4] << 8) | buf[5];

    int16_t gx = (buf[6] << 8) | buf[7];
    int16_t gy = (buf[8] << 8) | buf[9];
    int16_t gz = (buf[10] << 8) | buf[11];

    printf("ACCEL: %d %d %d\n", ax, ay, az);
    printf("GYRO : %d %d %d\n\n", gx, gy, gz);

    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "ACCEL:");
    snprintf(text, sizeof(text), "%d %d %d", ax, ay, az);
    ssd1306_draw_string(&disp, 5, 8, 1, text);


    ssd1306_draw_string(&disp, 0, 20, 1, "GYRO:");
    snprintf(text, sizeof(text), "%d %d %d", gx, gy, gz);

    ssd1306_draw_string(&disp, 5, 28, 1, text);

    ssd1306_show(&disp);
}

int main()
{
    // Needed for getting logs from `printf` via USB 
    stdio_init_all();

    // Initialize the LED and button
    // gpio_init(PICO_DEFAULT_LED_PIN);
    // gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    // gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN);

    i2c_setup();

    sleep_ms(5000);

    icm20948_init(I2C_PORT, IMU_ADDR);

    uint8_t whoami;
    read_regs(i2c0, IMU_ADDR, 0x00, &whoami, 1);
    printf("WHO_AM_I: 0x%02X\n", whoami);

    init_oled();

    // snprintf(text, sizeof(text), "WHO_AM_I: 0x%02X", whoami);
    // ssd1306_draw_string(&disp, 5, 40, 1, text);

    // snprintf(text, sizeof(text), "WHO_AM_I");
    // ssd1306_draw_string(&disp, 0, 0, 1, "WHO_AM_I:");

    // snprintf(text, sizeof(text), "0x%02X", whoami);
    // ssd1306_draw_string(&disp, 5, 8, 1, text);

    // ssd1306_show(&disp);

    // scan_i2c();

    while (true) {
        read_imu(I2C_PORT, IMU_ADDR);
        sleep_ms(200);

        // animation();

        // sleep_ms(1000);
        // tight_loop_contents();
    }
}
