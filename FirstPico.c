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
#define MAG_ADDR 0x0C
#define SLEEPTIME 25

ssd1306_t disp;

int16_t ax, ay, az, gx, gy, gz, my, mx, mz;

int32_t gx_offset = 0, gy_offset = 0, gz_offset = 0, ax_offset = 0, ay_offset = 0, az_offset = 0;

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

void icm20948_init_acc_gyro(i2c_inst_t *i2c, uint8_t addr) {
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
    // write_reg(i2c, addr, GYRO_CONFIG_1, 0x00);
    // Gyro low pass
    // From ChatGPT
    // write_reg(i2c, addr, GYRO_CONFIG_1, 0x05);
    // From github which gives them a higher range but still stable
    write_reg(i2c, addr, GYRO_CONFIG_1, 0x29);
    write_reg(i2c, addr, GYRO_SMPLRT_DIV, 0x0A);

    // Accel config (±2g)
    // write_reg(i2c, addr, ACCEL_CONFIG, 0x00);
    // Accel low-pass
    // From chatgpt
    // write_reg(i2c, addr, ACCEL_CONFIG, 0x05);
    // From github which gives them a higher range but still stable
    write_reg(i2c, addr, ACCEL_CONFIG, 0x11);
    write_reg(i2c, addr, ACCEL_SMPLRT_DIV_2, 0x0A);
}

void init_mag(i2c_inst_t *i2c, uint8_t addr) {
    select_bank(i2c, addr, BANK0);

    // Magnet configure
    write_reg(i2c, addr, USER_CTRL, 0x20);  // I2C_MST_EN
    sleep_ms(10);

    select_bank(i2c, addr, BANK3);
    // Setting clock cycle to 345.60 Hz which is recommended when targeting 400.
    write_reg(i2c, addr, I2C_MST_CTRL, 0x07);

    sleep_ms(10);

    // Reset magnetometer
    write_reg(i2c, addr, I2C_SLV0_ADDR, 0x0C); // I2C_SLV0_ADDR (write mode)
    write_reg(i2c, addr, I2C_SLV0_REG, AK09916_CNTL3); // I2C_SLV0_REG
    write_reg(i2c, addr, I2C_SLV0_DO, 0x01); // I2C_SLV0_DO (mode 100Hz)
    write_reg(i2c, addr, I2C_SLV0_CTRL, 0x81); // enable, 1 byte
    sleep_ms(100);
    // Disable SLV0 after write
    write_reg(i2c, addr, I2C_SLV0_CTRL, 0x00);

    // Set continuous mode
    write_reg(i2c, addr, I2C_SLV0_ADDR, 0x0C); // I2C_SLV0_ADDR (write mode)
    write_reg(i2c, addr, I2C_SLV0_REG, AK09916_CNTL2); // I2C_SLV0_REG
    write_reg(i2c, addr, I2C_SLV0_DO, 0x08); // I2C_SLV0_DO (mode 100Hz)
    write_reg(i2c, addr, I2C_SLV0_CTRL, 0x81); // enable, 1 byte
    sleep_ms(50);

    // Disable SLV0 after write
    write_reg(i2c, addr, I2C_SLV0_CTRL, 0x00);

    // Configure continuous read via SLV0
    write_reg(i2c, addr, I2C_SLV0_ADDR, 0x8C);
    write_reg(i2c, addr, I2C_SLV0_REG, AK09916_XOUT_L);
    write_reg(i2c, addr, I2C_SLV0_CTRL, 0x86); // 6 bytes
    sleep_ms(10);

    select_bank(i2c, addr, BANK0);
}

void icm20948_init_mag(i2c_inst_t *i2c, uint8_t addr) {
     // MAGNET CONFIGURATION!!!!
    
    // Back to bank 0
    select_bank(i2c, addr, BANK0);

    // Magnet configure
    write_reg(i2c, addr, USER_CTRL, 0x20);  // I2C_MST_EN

    sleep_ms(10);

    select_bank(i2c, addr, BANK3);
    // Setting clock cycle to 345.60 Hz which is recommended when targeting 400.
    write_reg(i2c, addr, I2C_MST_CTRL, 0x07);

    sleep_ms(10);

    select_bank(i2c, addr, BANK3);
    write_reg(i2c, addr, I2C_SLV0_ADDR, 0x0C); // I2C_SLV0_ADDR (write mode)
    write_reg(i2c, addr, I2C_SLV0_REG, AK09916_CNTL2); // I2C_SLV0_REG
    write_reg(i2c, addr, I2C_SLV0_DO, 0x08); // I2C_SLV0_DO (mode 100Hz)
    write_reg(i2c, addr, I2C_SLV0_CTRL, 0x81); // enable, 1 byte
    sleep_ms(50);

    select_bank(i2c, addr, BANK3);
    // FORCING A RE-TRIGGER
    write_reg(i2c, addr, 0x05, 0x00);
    sleep_ms(5);
    write_reg(i2c, addr, 0x05, 0x81);
    sleep_ms(50);

    // CHECKING CNTL2 AGAIN
    select_bank(i2c, addr, BANK3);
    write_reg(i2c, addr, I2C_SLV0_ADDR, 0x8C); // I2C_SLV0_ADDR (read mode, 0x0C | 0x80)
    write_reg(i2c, addr, I2C_SLV0_REG, AK09916_CNTL2); // start register (HXL)
    write_reg(i2c, addr, I2C_SLV0_CTRL, 0x81); // enable, read 6 bytes

    sleep_ms(10);

    select_bank(i2c, addr, BANK0);

    uint8_t val;
    read_regs(i2c, addr, EXT_SENS_DATA_00, &val, 1);

    printf("CNTL2: 0x%02X\n", val);

    read_regs(i2c, addr, USER_CTRL, &val, 1);

    printf("USERCONTROL: 0x%02X\n", val);

    select_bank(i2c, addr, BANK3);

    write_reg(i2c, addr, I2C_SLV0_ADDR, 0x8C);
    write_reg(i2c, addr, I2C_SLV0_REG, AK09916_XOUT_L);
    write_reg(i2c, addr, I2C_SLV0_CTRL, 0x86); // 6 bytes
}

void init_mag_2(void) {
    uint8_t buf;

    select_bank(I2C_PORT, IMU_ADDR, BANK0);

    write_reg(I2C_PORT, IMU_ADDR, INT_PIN_CFG, 0x02);
    read_regs(I2C_PORT, MAG_ADDR, 0x01, &buf, 1);

    printf("MAG WHO_AM_I: 0x%02X\n", buf);

    write_reg(I2C_PORT, MAG_ADDR, AK09916_CNTL2, 0x08);
}

void read_mag() {
    uint8_t buf[6];

    // select_bank(I2C_PORT, IMU_ADDR, BANK3);
    // write_reg(I2C_PORT, IMU_ADDR, I2C_SLV0_REG, 0x10);
    // write_reg(I2C_PORT, IMU_ADDR, I2C_SLV0_CTRL, 0x81);

    // select_bank(I2C_PORT, IMU_ADDR, BANK0);
    // read_regs(I2C_PORT, IMU_ADDR, EXT_SENS_DATA_00, buf, 6);

    // mx = (buf[1] << 8) | buf[0];
    // my = (buf[3] << 8) | buf[2];
    // mz = (buf[5] << 8) | buf[4];


    // akiona github
    uint8_t bufs[8];

    read_regs(I2C_PORT, MAG_ADDR, AK09916_XOUT_L, bufs, 8);

    mx = (bufs[1] << 8) | bufs[0];
    my = (bufs[3] << 8) | bufs[2];
    mz = (bufs[5] << 8) | bufs[4];


    // Read 1 byte from mag WHO_AM_I (0x01)




    // // TO CHECK THAT THE MAG WHO AM IS 0x09.
    // select_bank(I2C_PORT, IMU_ADDR, BANK3);
    // write_reg(I2C_PORT, IMU_ADDR, I2C_SLV0_ADDR, 0x8C);
    // write_reg(I2C_PORT, IMU_ADDR, I2C_SLV0_REG, WHO_AM_I_AK09916);
    // write_reg(I2C_PORT, IMU_ADDR, I2C_SLV0_CTRL, 0x81);

    // sleep_ms(10);

    // uint8_t whoami;
    // select_bank(I2C_PORT, IMU_ADDR, BANK0);
    // read_regs(I2C_PORT, IMU_ADDR, EXT_SENS_DATA_00, &whoami, 1);

    // printf("MAG WHO_AM_I: 0x%02X\n", whoami);
}

void read_imu(i2c_inst_t *i2c, uint8_t addr) {
    uint8_t buf[12];

    select_bank(i2c, addr, BANK0);

    // Read accel (6 bytes) + gyro (6 bytes)
    read_regs(i2c, addr, 0x2D, buf, 12);

    ax = (buf[0] << 8) | buf[1];
    ay = (buf[2] << 8) | buf[3];
    az = (buf[4] << 8) | buf[5];

    gx = (buf[6] << 8) | buf[7];
    gy = (buf[8] << 8) | buf[9];
    gz = (buf[10] << 8) | buf[11];

    // printf("ACCEL: %d %d %d\n", ax, ay, az);
    // printf("GYRO : %d %d %d\n\n", gx, gy, gz);
}

void calibrate_acc_and_gyro(void) {
    printf("CALIBRATING!!!");

    int16_t range = 1000;

    for (int i = 0; i < range; i++) {
        read_imu(I2C_PORT, IMU_ADDR);

        gx_offset += gx;
        gy_offset += gy;
        gz_offset += gz;

        ax_offset += ax;
        ay_offset += ay;
        az_offset += az;

        sleep_ms(2);
        // printf("TEMP OFFSETS : %d %d %d\n\n", gx_offset, gy_offset, gz_offset);
    }

    gx_offset /= range;
    gy_offset /= range;
    gz_offset /= range;

    ax_offset /= range;
    ay_offset /= range;
    az_offset /= range;

    printf("DONE CALIBRATING! - OFFSETS : %d %d %d\n\n", gx_offset, gy_offset, gz_offset);
}

void displayText(void) {
    ssd1306_clear(&disp);

    ssd1306_draw_string(&disp, 0, 0, 1, "ACCEL:");
    snprintf(text, sizeof(text), "%d %d %d", ax - ax_offset, ay - ay_offset, az - az_offset);
    // snprintf(text, sizeof(text), "%d %d %d", ax, ay, az);
    ssd1306_draw_string(&disp, 5, 8, 1, text);


    ssd1306_draw_string(&disp, 0, 20, 1, "GYRO:");
    snprintf(text, sizeof(text), "%d %d %d", gx - gx_offset, gy - gy_offset, gz - gz_offset);
    // snprintf(text, sizeof(text), "%d %d %d", gx, gy, gz);
    ssd1306_draw_string(&disp, 5, 28, 1, text);


    ssd1306_draw_string(&disp, 0, 40, 1, "COMPAS:");
    snprintf(text, sizeof(text), "%d %d %d", mx, my, mz);
    ssd1306_draw_string(&disp, 5, 48, 1, text);

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

    icm20948_init_acc_gyro(I2C_PORT, IMU_ADDR);

    uint8_t whoami;
    read_regs(i2c0, IMU_ADDR, 0x00, &whoami, 1);
    printf("WHO_AM_I: 0x%02X\n", whoami);

    // icm20948_init_mag(I2C_PORT, IMU_ADDR);
    // init_mag(I2C_PORT, IMU_ADDR);
    init_mag_2();

    init_oled();

    calibrate_acc_and_gyro();

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
        read_mag();

        displayText();
        sleep_ms(200);

        // animation();

        // sleep_ms(1000);
        // tight_loop_contents();
    }
}
