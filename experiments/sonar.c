#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "ssd1306/ssd1306.h"
#include "ICM20948/ICM20948_register.h"
#include <math.h>

#define SDA_PIN 4
#define SCL_PIN 5
#define TRIG_PIN 10
#define ECHO_PIN 11
#define I2C_PORT i2c0
#define OLED_ADDR 0x3C

#define OLED_W 128
#define OLED_H 64
#define CENTER_Y (OLED_H / 2)
#define CENTER_X (OLED_W / 2)

ssd1306_t disp;

// Display text
static char text[32];

void i2c_setup()
{
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz fast mode

    // pins for OLED and IMU
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
}

void hcsr04_init(void)
{
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_pull_down(ECHO_PIN); // optional
}

void init_oled(void)
{
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, OLED_ADDR, I2C_PORT);
    ssd1306_clear(&disp);
}

float hcsr04_read_distance_cm(void)
{
    // Trigger a 10µs pulse
    gpio_put(TRIG_PIN, 0);
    sleep_us(2);
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);

    // Wait for echo high
    while (!gpio_get(ECHO_PIN))
    {
        tight_loop_contents();
    }
    uint64_t start = time_us_64();

    // Wait for echo low
    while (gpio_get(ECHO_PIN))
    {
        tight_loop_contents();
    }
    uint64_t end = time_us_64();

    uint64_t pulse_width = end - start;
    float distance_cm = pulse_width * 0.0343f / 2.0f;
    return distance_cm;
}

int main()
{
    // Needed for getting logs from `printf` via USB
    stdio_init_all();

    i2c_setup();
    hcsr04_init();
    init_oled();

    sleep_ms(5000);
    printf("SLEEP DONE!.\n");

    while (1)
    {
        float dist = hcsr04_read_distance_cm();

        printf("Distance: %.2f cm\n", dist);
        ssd1306_clear(&disp);
        snprintf(text, sizeof(text), "%.2f cm", dist);
        ssd1306_draw_string(&disp, 8, 24, 2, text);
        ssd1306_show(&disp);

        sleep_ms(200);
    }
}
