#include "max30102.h"
#include "i2c_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define MAX30102_ADDR 0x57

#define FINGER_ON_THRESHOLD   7000
#define FINGER_OFF_THRESHOLD  4000
#define FINGER_STABLE_COUNT   2

#define MIN_PEAK_INTERVAL     400
#define MAX_PEAK_INTERVAL     1500

static float dc_avg = 0;
static float lp = 0;
static float filtered = 0;
static float peak_env = 0;

static float prev_f = 0;
static float prev_prev_f = 0;

static float bpm_avg = 0;

static uint32_t last_peak_time = 0;

static int sample_count = 0;

static int finger_present = 0;
static int finger_counter = 0;

static uint32_t intervals[8] = {0};
static int interval_index = 0;

void max30102_init()
{
    i2c_write(MAX30102_ADDR, 0x09, 0x40);
    vTaskDelay(pdMS_TO_TICKS(100));

    i2c_write(MAX30102_ADDR, 0x08, 0x4F);
    i2c_write(MAX30102_ADDR, 0x09, 0x03);
    i2c_write(MAX30102_ADDR, 0x0A, 0x24);

    i2c_write(MAX30102_ADDR, 0x0C, 0x10);
    i2c_write(MAX30102_ADDR, 0x0D, 0x10);

    i2c_write(MAX30102_ADDR, 0x04, 0x00);
    i2c_write(MAX30102_ADDR, 0x05, 0x00);
    i2c_write(MAX30102_ADDR, 0x06, 0x00);
}

uint32_t max30102_read_ir()
{
    uint8_t data[6];

    if (i2c_read(MAX30102_ADDR, 0x07, data, 6) != ESP_OK)
        return 0;

    uint32_t ir = ((uint32_t)data[3] << 16) |
                  ((uint32_t)data[4] << 8) |
                  data[5];

    return ir & 0x3FFFF;
}

int max30102_finger_present(void)
{
    return finger_present;
}

float max30102_get_bpm()
{
    uint32_t ir = max30102_read_ir();

    if (finger_present)
    {
        if (ir < FINGER_OFF_THRESHOLD)
        {
            if (++finger_counter > FINGER_STABLE_COUNT)
            {
                finger_present = 0;
                finger_counter = 0;

                dc_avg = 0;
                lp = 0;
                filtered = 0;
                peak_env = 0;

                prev_f = 0;
                prev_prev_f = 0;

                bpm_avg = 0;

                last_peak_time = 0;

                sample_count = 0;

                for (int i = 0; i < 8; i++)
                {
                    intervals[i] = 0;
                }

                interval_index = 0;

                return -1;
            }
        }
        else
        {
            finger_counter = 0;
        }
    }
    else
    {
        if (ir > FINGER_ON_THRESHOLD)
        {
            if (++finger_counter > FINGER_STABLE_COUNT)
            {
                finger_present = 1;
                finger_counter = 0;
            }
        }
        else
        {
            return -1;
        }
    }

    if (!finger_present)
        return -1;

    sample_count++;

    // Remove DC component
    dc_avg = 0.95f * dc_avg + 0.05f * ir;
    float signal = ir - dc_avg;

    // High-pass effect
    lp = 0.8f * lp + 0.2f * signal;
    filtered = signal - lp;

    // Wait for filter stabilization
    if (sample_count < 50)
    {
        prev_prev_f = prev_f;
        prev_f = filtered;
        return -1;
    }

    // Adaptive threshold
    peak_env = 0.9f * peak_env + 0.1f * fabsf(filtered);

    float threshold = peak_env * 0.5f;
    float min_peak = peak_env * 0.55f;

    float bpm_out = -1;

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Refractory period
    uint32_t refractory = 420;

    // Reject peaks too close together
    if (last_peak_time != 0 &&
        (now - last_peak_time) < refractory)
    {
        prev_prev_f = prev_f;
        prev_f = filtered;
        return -1;
    }

    float slope_up = prev_f - prev_prev_f;
    float slope_down = filtered - prev_f;

    // Peak detection
    if (slope_up > 0 &&
        slope_down < 0 &&
        prev_f > threshold &&
        prev_f > min_peak)
    {
        if (last_peak_time != 0)
        {
            uint32_t diff = now - last_peak_time;

            if (diff > MIN_PEAK_INTERVAL &&
                diff < MAX_PEAK_INTERVAL)
            {
                intervals[interval_index] = diff;
                interval_index = (interval_index + 1) % 8;

                uint32_t sum = 0;
                int count = 0;

                for (int i = 0; i < 8; i++)
                {
                    if (intervals[i] > 0)
                    {
                        sum += intervals[i];
                        count++;
                    }
                }

                if (count > 0)
                {
                    float avg_diff = (float)sum / count;
                    float bpm = 60000.0f / avg_diff;

                    if (bpm > 40 && bpm < 180)
                    {
                        bpm_avg = bpm;
                        bpm_out = bpm_avg;
                    }
                }
            }
        }

        last_peak_time = now;
    }

    prev_prev_f = prev_f;
    prev_f = filtered;

    return bpm_out;
}