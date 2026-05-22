#include "max30102.h"
#include "i2c_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define MAX30102_ADDR 0x57

// Finger detection thresholds
#define FINGER_ON_THRESHOLD   7000
#define FINGER_OFF_THRESHOLD  4000
#define FINGER_STABLE_COUNT   2

// Valid heartbeat interval range (ms)
#define MIN_PEAK_INTERVAL     400
#define MAX_PEAK_INTERVAL     1500

// Signal processing variables
static float dc_avg = 0;
static float lp = 0;
static float filtered = 0;
static float peak_env = 0;

static float prev_f = 0;
static float prev_prev_f = 0;

// BPM calculation variables
static float bpm_avg = 0;
static uint32_t last_peak_time = 0;
static int sample_count = 0;

// Finger detection state
static int finger_present = 0;
static int finger_counter = 0;

// Heartbeat interval buffer
static uint32_t intervals[4] = {0};
static int interval_index = 0;

// Initialize MAX30102 sensor
void max30102_init(){
    // Reset device
    i2c_write(MAX30102_ADDR, 0x09, 0x40);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Configure FIFO
    i2c_write(MAX30102_ADDR, 0x08, 0x4F);

    // Enable SpO2 mode
    i2c_write(MAX30102_ADDR, 0x09, 0x03);

    // Sample rate and pulse width
    i2c_write(MAX30102_ADDR, 0x0A, 0x24);

    // IR LED current
    i2c_write(MAX30102_ADDR, 0x0C, 0x10);

    // Red LED current
    i2c_write(MAX30102_ADDR, 0x0D, 0x10);

    // Clear FIFO pointers
    i2c_write(MAX30102_ADDR, 0x04, 0x00);
    i2c_write(MAX30102_ADDR, 0x05, 0x00);
    i2c_write(MAX30102_ADDR, 0x06, 0x00);
}

// Read IR sample from sensor FIFO
uint32_t max30102_read_ir(){
    uint8_t data[6];

    if (i2c_read(MAX30102_ADDR, 0x07, data, 6) != ESP_OK){
        return 0;
    }

    uint32_t ir = ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5];

    return ir & 0x3FFFF;
}

// Return current finger detection status
int max30102_finger_present(void){
    return finger_present;
}

// Process IR signal and estimate BPM
float max30102_get_bpm(){
    uint32_t ir = max30102_read_ir();

    // Check finger removal
    if (finger_present){
        if (ir < FINGER_OFF_THRESHOLD){
            if (++finger_counter > FINGER_STABLE_COUNT){
                // Reset algorithm state
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

                for (int i = 0; i < 4; i++){
                    intervals[i] = 0;
                }

                interval_index = 0;

                return -1;
            }
        }else{
            finger_counter = 0;
        }
    } else {
        // Check finger placement
        if (ir > FINGER_ON_THRESHOLD){
            if (++finger_counter > FINGER_STABLE_COUNT){

                finger_present = 1;
                finger_counter = 0;
            }
        }else{
            return -1;
        }
    }

    if (!finger_present){
        return -1;
    }
    sample_count++;

    // Remove DC component
    dc_avg = 0.95f * dc_avg + 0.05f * ir;
    float signal = ir - dc_avg;

    // High-pass filtering
    lp = 0.8f * lp + 0.2f * signal;
    filtered = signal - lp;

    // Allow filter to stabilize
    if (sample_count < 50){
        prev_prev_f = prev_f;
        prev_f = filtered;

        return -1;
    }

    // Adaptive peak envelope
    peak_env = 0.9f * peak_env + 0.1f * fabsf(filtered);

    float threshold = peak_env * 0.65f;

    float min_peak = peak_env * 0.72f;

    float bpm_out = -1;

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Peak detection lockout period
    uint32_t refractory = 640;

    if (last_peak_time != 0 && (now - last_peak_time) < refractory){
        prev_prev_f = prev_f;
        prev_f = filtered;

        return -1;
    }

    // Calculate signal slopes
    float slope_up = prev_f - prev_prev_f;

    float slope_down = filtered - prev_f;

    static float peak_candidate = 0;

    // Track possible peak
    if (prev_f > threshold && prev_f > prev_prev_f && prev_f > filtered){

        if (prev_f > peak_candidate){
            peak_candidate = prev_f;
        }
    }

    // Confirm heartbeat peak
    if (peak_candidate > 0 && slope_up < 0 && slope_down < 0){
        peak_candidate = 0;

        if (last_peak_time != 0){
            uint32_t diff =
                now - last_peak_time;

            if (diff > MIN_PEAK_INTERVAL && diff < MAX_PEAK_INTERVAL){
                intervals[interval_index] = diff;

                interval_index = (interval_index + 1) % 4;

                uint32_t sum = 0;
                int count = 0;

                // Average recent beat intervals
                for (int i = 0; i < 4; i++){
                    if (intervals[i] > 0){
                        sum += intervals[i];
                        count++;
                    }
                }

                if (count > 0){
                    float avg_diff = (float)sum / count;

                    float bpm = 60000.0f / avg_diff;

                    // Accept realistic BPM values
                    if (bpm > 40 && bpm < 180){
                        // Smooth BPM output
                        bpm_avg = 0.7f * bpm_avg + 0.3f * bpm;
                        bpm_out = bpm_avg;
                    }
                }
            }
        }
        last_peak_time = now;
    }

    // Update filter history
    prev_prev_f = prev_f;
    prev_f = filtered;

    return bpm_out;
}