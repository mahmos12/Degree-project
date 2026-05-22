#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_dev.h"
#include "ssd1306.h"
#include "max30102.h"
#include "mpu6050.h"
#include "thingspeak.h"

// Shared system state
volatile float g_bpm = -1;
volatile int g_finger = 0;
volatile int g_countdown = 0;
volatile int g_ready = 0;

// Motion classification states
typedef enum{
    STATE_SITTING = 0,
    STATE_WALKING,
    STATE_RUNNING
} motion_state_t;

volatile motion_state_t g_motion = STATE_SITTING;

// -------- PULSE TASK -- Reads heart rate sensor and manages measurement state ------
void pulse_task(void *arg){
    int prev_finger = 0;
    TickType_t finger_start = 0;

    while (1){
        float bpm = max30102_get_bpm();
        int finger = max30102_finger_present();
        g_finger = finger;
        // Start measurement timer when finger is detected
        if (finger && !prev_finger){
            finger_start = xTaskGetTickCount();
            g_ready = 0;
        }

        if (finger){
            int elapsed = ((xTaskGetTickCount() - finger_start) *portTICK_PERIOD_MS) / 1000;

            // Wait for sensor signal to stabilize
            if (elapsed < 10){
                g_countdown = 10 - elapsed;
                g_bpm = -1;
            }else{
                g_ready = 1;
                g_countdown = 0;

                if (bpm > 0){
                    g_bpm = bpm;
                }
            }
        }else{

            // Reset state when finger is removed
            g_bpm = -1;
            g_ready = 0;

            g_countdown = 0;
        }

        prev_finger = finger;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


// -------- MOTION TASK --------
// Reads accelerometer data and classifies activity level
void motion_task(void *arg){
    int16_t x, y, z;
    float motion_avg = 0;

    while (1){
        if (mpu6050_read(&x, &y, &z)){
            float fx = (float)x;
            float fy = (float)y;
            float fz = (float)z;

            // Calculate acceleration magnitude
            float mag = sqrtf(fx * fx + fy * fy + fz * fz);

            // Remove gravity component
            float motion = fabsf(mag - 16384.0f);

            // Smooth motion value
            motion_avg = 0.92f * motion_avg + 0.08f * motion;
            
            // Classify movement 
            if (motion_avg > 5000){
                g_motion = STATE_RUNNING;
            }else if (motion_avg > 1800){
                g_motion = STATE_WALKING;
            }else{
                g_motion = STATE_SITTING;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


// -------- DISPLAY TASK --------
// Updates OLED display with heart rate and activity data
void display_task(void *arg){
    while (1){
        ssd1306_clear();

        if (!g_finger){
            ssd1306_draw_text_big(0, 5, "TAP SENSOR");
        }else if (!g_ready){
            char buf[8];
            sprintf(buf, "%d", g_countdown);
            ssd1306_draw_text_big(50, 0, buf);
        }else if (g_bpm > 0){
            char buf[16];
            sprintf(buf, "%d BPM", (int)g_bpm);
            ssd1306_draw_text_big(0, 0, buf);
        }else{
            ssd1306_draw_text_big(40, 0, "...");
        }

        // Show current motion state
        switch (g_motion){
            case STATE_RUNNING:
                ssd1306_draw_text_big(0, 40, "RUNNING");
                break;

            case STATE_WALKING:
                ssd1306_draw_text_big(0, 40, "WALKING");
                break;

            default:
                ssd1306_draw_text_big(0, 40, "SITTING");
                break;
        }

        ssd1306_update();

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// -------- THINGSPEAK TASK -- Uploads sensor data to ThingSpeak ------ 
void thingspeak_task(void *arg){
    while (1){
        int motion = 0;

        switch (g_motion){
            case STATE_WALKING:
                motion = 1;
                break;
            case STATE_RUNNING:
                motion = 2;
                break;
            default:
                motion = 0;
                break;
        }

        thingspeak_send((int)g_bpm,motion,g_finger);

        // ThingSpeak free tier update interval
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}


// -------- MAIN --------
// Initializes peripherals and starts system tasks
void app_main(){
    i2c_master_init();
    ssd1306_init();
    max30102_init();
    mpu6050_init();
    thingspeak_init();

    xTaskCreate(pulse_task, "pulse", 4096, NULL, 5, NULL);
    xTaskCreate(motion_task, "motion", 4096, NULL, 5, NULL);
    xTaskCreate(display_task, "display", 4096, NULL, 5, NULL);
    xTaskCreate(thingspeak_task, "thingspeak", 4096, NULL, 5, NULL);
}