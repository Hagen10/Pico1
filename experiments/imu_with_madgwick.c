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
absolute_time_t last_time, last_display_time, now;
uint8_t whoami;
repeating_timer_t imu_timer;

int16_t ax, ay, az, gx, gy, gz, my, mx, mz;
int32_t gx_offset = 0, gy_offset = 0, gz_offset = 0, ax_offset = 0, ay_offset = 0, az_offset = 0, mx_offset = 0, my_offset = 0, mz_offset = 0;
float axf = 0.0f, ayf = 0.0f, azf = 0.0f, gxf = 0.0f, gyf = 0.0f, gzf = 0.0f, mxf = 0.0f, myf = 0.0f, mzf = 0.0f;
float gyro_bias_x = 0.0f, gyro_bias_y = 0.0f, gyro_bias_z = 0.0f;

float accel_variance = 0.0f;

float mag_x_filt = 0.0f, mag_y_filt = 0.0f, mag_z_filt = 0.0f;
bool mag_init = false;
bool mag_read = false;

float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;

// Tuning parameters
const float alpha = 0.6f;
const float MOTION_THRESHOLD = 0.1f; // m/s² (tunable)
const float BIAS_ALPHA = 0.0001f;    // slow convergence
const float beta = 0.01f;            // tuning parameter

volatile bool imu_update_ready = false;
const uint32_t IMU_UPDATE_PERIOD_US = 5000; // 200 Hz = 5000 us
const uint32_t DISPLAY_UPDATE_US = 50000;   // 20 Hz display

// Display text
static char text[32];

// Timer callback for IMU updates
bool imu_timer_callback(repeating_timer_t *rt)
{
    imu_update_ready = true;
    return true; // keep repeating
}

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

    // From github which gives them a higher range but still stable
    write_reg(i2c, addr, GYRO_CONFIG_1, 0x29);
    write_reg(i2c, addr, GYRO_SMPLRT_DIV, 0x0A);

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

void update_gyro_bias()
{
    // Check if device is stationary (low acceleration variance)
    static float ax_prev = 0.0f, ay_prev = 0.0f, az_prev = 0.0f;

    float accel_delta_x = fabsf(axf - ax_prev);
    float accel_delta_y = fabsf(ayf - ay_prev);
    float accel_delta_z = fabsf(azf - az_prev);

    accel_variance = (accel_delta_x + accel_delta_y + accel_delta_z) / 3.0f;

    // Only update bias when stationary
    if (accel_variance < MOTION_THRESHOLD)
    {
        gyro_bias_x += BIAS_ALPHA * gxf;
        gyro_bias_y += BIAS_ALPHA * gyf;
        gyro_bias_z += BIAS_ALPHA * gzf;
    }

    ax_prev = axf;
    ay_prev = ayf;
    az_prev = azf;
}

// Add alternative to Madgwick when magnetometer fails
void complementary_filter_update(float gx, float gy, float gz,
                                 float ax, float ay, float az,
                                 float dt)
{
    const float ACCEL_WEIGHT = 0.02f;

    // Get current roll/pitch from accelerometer
    float accel_roll = atan2f(ay, sqrtf(ax * ax + az * az));
    float accel_pitch = atan2f(-ax, sqrtf(ay * ay + az * az));

    // Get current angles from quaternion
    float q_roll = atan2f(2.0f * (q0 * q1 + q2 * q3),
                          1.0f - 2.0f * (q1 * q1 + q2 * q2));
    float q_pitch = asinf(2.0f * (q0 * q2 - q3 * q1));

    // Fuse with gyro
    q_roll = q_roll + gx * dt;
    q_pitch = q_pitch + gy * dt;

    // Correct with accel (low weight for drift control)
    q_roll = q_roll * (1.0f - ACCEL_WEIGHT) + accel_roll * ACCEL_WEIGHT;
    q_pitch = q_pitch * (1.0f - ACCEL_WEIGHT) + accel_pitch * ACCEL_WEIGHT;

    // Reconstruct quaternion from roll/pitch (no yaw correction)
    float cr = cosf(q_roll * 0.5f);
    float sr = sinf(q_roll * 0.5f);
    float cp = cosf(q_pitch * 0.5f);
    float sp = sinf(q_pitch * 0.5f);

    q0 = cr * cp;
    q1 = sr * cp;
    q2 = cr * sp;
    q3 = sr * sp;
}

int main()
{
    // Needed for getting logs from `printf` via USB
    stdio_init_all();

    i2c_setup();

    sleep_ms(5000);

    icm20948_init_acc_gyro(I2C_PORT, IMU_ADDR);

    // DEBUG STUFF
    read_regs(i2c0, IMU_ADDR, 0x00, &whoami, 1);
    printf("WHO_AM_I: 0x%02X\n", whoami);

    init_mag();
    init_oled();
    calibrate_acc_and_gyro();
    // scan_i2c();
    calibrate_mag();

    last_time = get_absolute_time();
    last_display_time = get_absolute_time();

    add_repeating_timer_us(IMU_UPDATE_PERIOD_US, imu_timer_callback, NULL, &imu_timer);

    while (1)
    {
        if (!imu_update_ready)
        {
            tight_loop_contents();
            continue;
        }
        imu_update_ready = false;

        read_imu(I2C_PORT, IMU_ADDR);

        mag_read = read_mag();

        axf = (ax - ax_offset);
        ayf = (ay - ay_offset);
        azf = (az - az_offset);

        gxf = (gx - gx_offset - gyro_bias_x) * (M_PI / 180.0f) / 131.0f; // rad/s
        gyf = (gy - gy_offset - gyro_bias_y) * (M_PI / 180.0f) / 131.0f;
        gzf = (gz - gz_offset - gyro_bias_z) * (M_PI / 180.0f) / 131.0f;

        // Update bias estimation
        update_gyro_bias();

        if (mag_read)
        {
            mxf = mx - mx_offset;
            myf = my - my_offset;
            mzf = mz - mz_offset;

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
        }

        // Time delta
        now = get_absolute_time();
        float dt = absolute_time_diff_us(last_time, now) / 1000000.0f;
        last_time = now;

        // Update filter with smoothing filter running at 200 Hz
        fullMadgwickUpdate(gxf, gyf, gzf,
                           axf, ayf, azf,
                           mag_x_filt, mag_y_filt, mag_z_filt,
                           dt);

        // Update display at 20 Hz only
        if (absolute_time_diff_us(last_display_time, now) >= DISPLAY_UPDATE_US)
        {
            last_display_time = now;

            ssd1306_clear(&disp);
            float roll, pitch;
            get_roll_pitch(&roll, &pitch);

            draw_horizon(roll, pitch);
            draw_center_marker();

            // Optional: show bias magnitude
            snprintf(text, sizeof(text), "Bias: %.3f",
                     sqrtf(gyro_bias_x * gyro_bias_x + gyro_bias_y * gyro_bias_y + gyro_bias_z * gyro_bias_z));
            ssd1306_draw_string(&disp, 0, 50, 1, text);

            ssd1306_show(&disp);
        }
    }
}
