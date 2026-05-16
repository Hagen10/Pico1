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
#define I2C_PORT i2c0
#define OLED_ADDR 0x3C
#define IMU_ADDR 0x68
#define MAG_ADDR 0x0C
#define SLEEPTIME 25

#define OLED_W 128
#define OLED_H 64
#define CENTER_Y (OLED_H / 2)
#define CENTER_X (OLED_W / 2)

ssd1306_t disp;

int16_t ax, ay, az, gx, gy, gz, my, mx, mz;

int32_t gx_offset = 0, gy_offset = 0, gz_offset = 0, ax_offset = 0, ay_offset = 0, az_offset = 0, mx_offset = 0, my_offset = 0, mz_offset = 0;

static char text[32];

// Tracking interrupt timestamps to prevent switch bouncing causing additional interrupts
volatile absolute_time_t last_interrupt_time = 0;
const uint DEBOUNCE_MS = 150;

// AI

float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
float beta = 0.01f; // tuning parameter

float get_yaw()
{
    float yaw = atan2f(2.0f * (q1 * q2 + q0 * q3),
                       q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3);

    float yaw_deg = yaw * 180.0f / M_PI;

    if (yaw_deg < 0)
        yaw_deg += 360.0f;

    return yaw_deg;
}

void fullMadgwickUpdate(float gx, float gy, float gz,
                        float ax, float ay, float az,
                        float mx, float my, float mz,
                        float dt)
{
    float qDot1, qDot2, qDot3, qDot4;
    float norm;
    float hx, hy, _2bx, _2bz;
    float s0, s1, s2, s3;
    float _2q0mx, _2q0my, _2q0mz, _2q1mx;
    float _2q0 = 2.0f * q0;
    float _2q1 = 2.0f * q1;
    float _2q2 = 2.0f * q2;
    float _2q3 = 2.0f * q3;
    float _2q0q2 = 2.0f * q0 * q2;
    float _2q2q3 = 2.0f * q2 * q3;
    float q0q0 = q0 * q0;
    float q0q1 = q0 * q1;
    float q0q2 = q0 * q2;
    float q0q3 = q0 * q3;
    float q1q1 = q1 * q1;
    float q1q2 = q1 * q2;
    float q1q3 = q1 * q3;
    float q2q2 = q2 * q2;
    float q2q3 = q2 * q3;
    float q3q3 = q3 * q3;

    // Normalize accelerometer
    norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm == 0.0f)
        return;
    ax /= norm;
    ay /= norm;
    az /= norm;

    // Normalize magnetometer
    norm = sqrtf(mx * mx + my * my + mz * mz);
    if (norm == 0.0f)
        return;
    mx /= norm;
    my /= norm;
    mz /= norm;

    // Reference direction of Earth's magnetic field
    _2q0mx = 2.0f * q0 * mx;
    _2q0my = 2.0f * q0 * my;
    _2q0mz = 2.0f * q0 * mz;
    _2q1mx = 2.0f * q1 * mx;

    hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2 + mx * q1q1 + _2q1 * my * q2 + _2q1 * mz * q3 - mx * q2q2 - mx * q3q3;

    hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1 + _2q1mx * q2 - my * q1q1 + my * q2q2 + _2q2 * mz * q3 - my * q3q3;

    _2bx = sqrtf(hx * hx + hy * hy);
    _2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0 + _2q1mx * q3 - mz * q1q1 + _2q2 * my * q3 - mz * q2q2 + mz * q3q3;

    // Gradient descent correction
    s0 = -_2q2 * (2.0f * (q1q3 - q0q2) - ax) + _2q1 * (2.0f * (q0q1 + q2q3) - ay) - _2bz * q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

    s1 = _2q3 * (2.0f * (q1q3 - q0q2) - ax) + _2q0 * (2.0f * (q0q1 + q2q3) - ay) - 4.0f * q1 * (1 - 2.0f * (q1q1 + q2q2) - az) + _2bz * q3 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q3 - 4.0f * _2bz * q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

    s2 = -_2q0 * (2.0f * (q1q3 - q0q2) - ax) + _2q3 * (2.0f * (q0q1 + q2q3) - ay) - 4.0f * q2 * (1 - 2.0f * (q1q1 + q2q2) - az) + (-4.0f * _2bx * q2 - _2bz * q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + (_2bx * q0 - 4.0f * _2bz * q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

    s3 = _2q1 * (2.0f * (q1q3 - q0q2) - ax) + _2q2 * (2.0f * (q0q1 + q2q3) - ay) + (-4.0f * _2bx * q3 + _2bz * q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) + (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) + _2bx * q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

    // Normalize step
    norm = sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    s0 /= norm;
    s1 /= norm;
    s2 /= norm;
    s3 /= norm;

    // Apply feedback
    qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz) - beta * s0;
    qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy) - beta * s1;
    qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx) - beta * s2;
    qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx) - beta * s3;

    // Integrate
    q0 += qDot1 * dt;
    q1 += qDot2 * dt;
    q2 += qDot3 * dt;
    q3 += qDot4 * dt;

    // Normalize quaternion
    norm = sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 /= norm;
    q1 /= norm;
    q2 /= norm;
    q3 /= norm;
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
            sleep_ms(1000);
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

void icm20948_init_acc_gyro(i2c_inst_t *i2c, uint8_t addr)
{
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

void init_mag(void)
{
    uint8_t buf;

    select_bank(I2C_PORT, IMU_ADDR, BANK0);

    write_reg(I2C_PORT, IMU_ADDR, INT_PIN_CFG, 0x02);
    read_regs(I2C_PORT, MAG_ADDR, 0x01, &buf, 1);

    printf("MAG WHO_AM_I: 0x%02X\n", buf);

    write_reg(I2C_PORT, MAG_ADDR, AK09916_CNTL2, 0x08); // 100 Hz continuous
}

bool read_mag(void)
{
    uint8_t status;
    read_regs(I2C_PORT, MAG_ADDR, AK09916_ST1, &status, 1);
    if (!(status & 0x01))
        return false;

    uint8_t buf[8];
    read_regs(I2C_PORT, MAG_ADDR, AK09916_XOUT_L, buf, 8);

    uint8_t st2 = buf[7];
    if (st2 & 0x08)
        return false; // data overflow / invalid sample

    mx = (int16_t)((buf[1] << 8) | buf[0]);
    my = (int16_t)((buf[3] << 8) | buf[2]);
    mz = (int16_t)((buf[5] << 8) | buf[4]);

    return true;
}

void calibrate_mag(void)
{
    int16_t mag_max[3] = {-32767, -32767, -32767};
    int16_t mag_min[3] = {32767, 32767, 32767};
    int valid = 0;

    while (valid < 500)
    {
        if (!read_mag())
        {
            sleep_ms(10);
            continue;
        }

        mag_max[0] = fmax(mag_max[0], mx);
        mag_max[1] = fmax(mag_max[1], my);
        mag_max[2] = fmax(mag_max[2], mz);
        mag_min[0] = fmin(mag_min[0], mx);
        mag_min[1] = fmin(mag_min[1], my);
        mag_min[2] = fmin(mag_min[2], mz);

        valid++;
        sleep_ms(20);
    }

    mx_offset = (mag_max[0] + mag_min[0]) / 2;
    my_offset = (mag_max[1] + mag_min[1]) / 2;
    mz_offset = (mag_max[2] + mag_min[2]) / 2;
}

void read_imu(i2c_inst_t *i2c, uint8_t addr)
{
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

void calibrate_acc_and_gyro(void)
{
    printf("CALIBRATING!!!");

    int16_t range = 1000;

    for (int i = 0; i < range; i++)
    {
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

void get_roll_pitch(float *roll, float *pitch)
{
    *roll = atan2f(2.0f * (q0 * q1 + q2 * q3),
                   1.0f - 2.0f * (q1 * q1 + q2 * q2));

    *pitch = asinf(2.0f * (q0 * q2 - q3 * q1));

    // convert to degrees
    *roll *= 180.0f / M_PI;
    *pitch *= 180.0f / M_PI;
}

void draw_horizon(float roll, float pitch)
{

    float roll_rad = roll * M_PI / 180.0f;

    // pixels per degree (tune this!)
    float pitch_scale = 2.0f;

    int y_offset = (int)(pitch * pitch_scale);

    float cos_r = cosf(roll_rad);
    float sin_r = sinf(roll_rad);

    for (int x = 0; x < OLED_W; x++)
    {
        int x_rel = x - CENTER_X;

        // rotate line using roll
        int y = CENTER_Y + y_offset + (int)(x_rel * sin_r);

        // Use this instead of below for loop if you just want to see the horizon
        // if (y >= 0 && y < OLED_H)
        // {
        //     ssd1306_draw_pixel(&disp, x, y);
        // }

        for (int yy = 0; yy < OLED_H; yy++)
        {
            if (yy < y)
                ssd1306_draw_pixel(&disp, x, yy); // sky
            else
                ssd1306_clear_pixel(&disp, x, yy); // ground

        }
    }
}

void draw_center_marker()
{
    int cx = CENTER_X;
    int cy = CENTER_Y;

    ssd1306_draw_pixel(&disp, cx, cy);
    ssd1306_draw_pixel(&disp, cx - 2, cy);
    ssd1306_draw_pixel(&disp, cx + 2, cy);
}

int main()
{
    // Needed for getting logs from `printf` via USB
    stdio_init_all();

    i2c_setup();

    sleep_ms(5000);

    icm20948_init_acc_gyro(I2C_PORT, IMU_ADDR);

    uint8_t whoami;
    read_regs(i2c0, IMU_ADDR, 0x00, &whoami, 1);
    printf("WHO_AM_I: 0x%02X\n", whoami);

    init_mag();
    init_oled();
    calibrate_acc_and_gyro();
    // scan_i2c();
    calibrate_mag();

    // AI
    absolute_time_t last_time = get_absolute_time();
    static float mag_x_filt = 0.0f, mag_y_filt = 0.0f, mag_z_filt = 0.0f;
    bool mag_init = false;
    const float alpha = 0.2f;

    while (1)
    {
        read_imu(I2C_PORT, IMU_ADDR);

        if (!read_mag())
        {
            sleep_ms(5);
            continue;
        }

        // Convert units
        float axf = ax;
        float ayf = ay;
        float azf = az;

        gx -= gx_offset;
        gy -= gy_offset;
        gz -= gz_offset;

        float gxf = gx * (M_PI / 180.0f) / 131.0f; // rad/s
        float gyf = gy * (M_PI / 180.0f) / 131.0f;
        float gzf = gz * (M_PI / 180.0f) / 131.0f;

        mx -= mx_offset;
        my -= my_offset;
        mz -= mz_offset;

        float mxf = mx;
        float myf = my;
        float mzf = mz;

        if (!mag_init)
        {
            mag_x_filt = mxf;
            mag_y_filt = myf;
            mag_z_filt = mzf;
            mag_init = true;
        }

        // Suggested smoothing filter
        mag_x_filt = alpha * mxf + (1.0f - alpha) * mag_x_filt;
        mag_y_filt = alpha * myf + (1.0f - alpha) * mag_y_filt;
        mag_z_filt = alpha * mzf + (1.0f - alpha) * mag_z_filt;
        // End of smoothing filter

        // Time delta
        absolute_time_t now = get_absolute_time();
        float dt = absolute_time_diff_us(last_time, now) / 1000000.0f;
        last_time = now;

        // Update filter with smoothing filter
        fullMadgwickUpdate(gxf, gyf, gzf,
                           axf, ayf, azf,
                           mag_x_filt, mag_y_filt, mag_z_filt,
                           dt);

        // ORIGINAL YAW OUTPUT
        // float heading = get_yaw();
        // ssd1306_clear(&disp);
        // snprintf(text, sizeof(text), "Yaw: %.1f", heading);
        // ssd1306_draw_string(&disp, 5, 28, 1, text);
        // ssd1306_show(&disp);
        // ORIGINAL YAW OUTPUT END

        ssd1306_clear(&disp);
        float roll, pitch;
        get_roll_pitch(&roll, &pitch);
        draw_horizon(roll, pitch);
        draw_center_marker();
        ssd1306_show(&disp);

        sleep_ms(50);
    }
}
