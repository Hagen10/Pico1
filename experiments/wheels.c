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
#define SLEEPTIME 25
#define DISPLAY_UPDATE_US 50000

#define OLED_W 128
#define OLED_H 64
#define CENTER_Y (OLED_H / 2)
#define CENTER_X (OLED_W / 2)

ssd1306_t disp;
absolute_time_t last_time, last_display_time, now;
uint8_t whoami;
repeating_timer_t imu_timer;

// Display text
static char text[32];

// MOTORS
#define MOTOR_WHEEL 15
#define BUTTON_UP 12
#define BUTTON_DOWN 13

#define FROM_MIN 0
// #define FROM_MAX 4095
#define FROM_MAX 65000
#define ADD 1000

#define OFF 0
#define ON 65000
#define HALF 32500

uint16_t old_setting = OFF;
uint16_t new_setting = ON;

uint16_t current_setting = FROM_MIN;
// Tracking interrupt timestamps to prevent switch bouncing causing additional interrupts
volatile absolute_time_t last_interrupt_time = 0;
const uint DEBOUNCE_MS = 150;

void change_servo(uint gpio, uint32_t event_mask) {
    absolute_time_t now = get_absolute_time();
    if (event_mask & GPIO_IRQ_EDGE_FALL 
        && absolute_time_diff_us(last_interrupt_time, now) > DEBOUNCE_MS * 1000)
    {
        switch (gpio) {
            case BUTTON_DOWN:
                if (current_setting - ADD >= FROM_MIN) {
                    current_setting -= ADD;
                    pwm_set_gpio_level(MOTOR_WHEEL, current_setting);
                    // pwm_set_gpio_level(MOTOR_WHEEL, 700);

                    printf("decreasing currentSetting to: %d\n", current_setting);
                }
                break;
            case BUTTON_UP:
                if (current_setting + ADD <= FROM_MAX) {
                    current_setting += ADD;
                    pwm_set_gpio_level(MOTOR_WHEEL, current_setting);
                    // pwm_set_gpio_level(MOTOR_WHEEL, 3200);
                    printf("increasing currentSetting to: %d\n", current_setting);
                } 
                break;
            default:
                printf("Button Press wasn't registered properly\n");
        }
        last_interrupt_time = now;
    }
}


void scan_i2c()
{
    printf("I2C scan start...\n");

    for (int addr = 0; addr < 127; addr++)
    {
        uint8_t rxdata;
        int result = i2c_read_blocking(I2C_PORT, addr, &rxdata, 1, false);

        if (result >= 0)
        {
            printf("Device found at 0x%02X\n", addr);
            sleep_ms(100);
        }
    }

    printf("Scan done.\n");
}

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

// Write to IMU register
void write_reg(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    i2c_write_blocking(i2c, addr, buf, 2, false);
}

// Read IMU register
void read_regs(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
    i2c_write_blocking(i2c, addr, &reg, 1, true);
    i2c_read_blocking(i2c, addr, buf, len, false);
}

// Select bank
void select_bank(i2c_inst_t *i2c, uint8_t addr, uint8_t bank)
{
    write_reg(i2c, addr, REG_BANK_SEL, bank << 4);
}

void draw_center_marker()
{
    int cx = CENTER_X;
    int cy = CENTER_Y;

    ssd1306_draw_pixel(&disp, cx, cy);
    ssd1306_draw_pixel(&disp, cx - 2, cy);
    ssd1306_draw_pixel(&disp, cx + 2, cy);
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

    gpio_init(MOTOR_WHEEL);
    gpio_set_function(MOTOR_WHEEL, GPIO_FUNC_PWM);

      // Additional PWM CONFIGURATION
    uint slice_num = pwm_gpio_to_slice_num(MOTOR_WHEEL);
    pwm_set_enabled(slice_num, true);

    //   // Set the PWM frequency to 50 Hz
    // pwm_set_wrap(slice_num, 24999); // TOP value

    // // Set the clock divider to get 50 Hz
    // pwm_set_clkdiv(slice_num, 100.0f);

    // gpio_init(BUTTON_DOWN);
    // gpio_set_dir(BUTTON_DOWN, GPIO_IN);

    // gpio_init(BUTTON_UP);
    // gpio_set_dir(BUTTON_UP, GPIO_IN);

    // gpio_set_irq_enabled_with_callback(BUTTON_DOWN, GPIO_IRQ_EDGE_FALL, true, &change_servo);
    // gpio_set_irq_enabled_with_callback(BUTTON_UP, GPIO_IRQ_EDGE_FALL, true, &change_servo);

    pwm_set_gpio_level(MOTOR_WHEEL, ON);


    while (1)
    {
        float dist = hcsr04_read_distance_cm();

        if (dist < 20.0f) {
            new_setting = OFF;
        } else if (dist < 50.0f) {
            new_setting = HALF;
        }
        else {
            new_setting = ON;
        }

        if (old_setting != new_setting) {
            pwm_set_gpio_level(MOTOR_WHEEL, new_setting);
            old_setting = new_setting;
        }

        printf("Distance: %.2f cm\n", dist);
        ssd1306_clear(&disp);
        snprintf(text, sizeof(text), "S: %d", current_setting);
        ssd1306_draw_string(&disp, 8, 8, 2, text);
        snprintf(text, sizeof(text), "%.2f cm", dist);
        ssd1306_draw_string(&disp, 8, 32, 2, text);
        ssd1306_show(&disp);

        sleep_ms(200);
    }
}
