#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2c_dev.h"
#include "ssd1306.h"
#include "max30102.h"
#include "mpu6050.h"

// shared data
volatile float g_bpm = -1;
volatile int g_finger = 0;
volatile const char *g_state = "SIT";


// -------- PULSE --------
void pulse_task(void *arg)
{
    while (1)
    {
        float bpm = max30102_get_bpm();
        int finger = max30102_finger_present();

        g_finger = finger;

        if (finger && bpm > 0)
        {
            g_bpm = bpm;
        }

        if (!finger)
        {
            g_bpm = -1;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


// -------- MOTION --------
void motion_task(void *arg)
{
    int16_t x, y, z;
    float motion_avg = 0;

    while (1)
    {
        if (mpu6050_read(&x, &y, &z))
        {
            float fx = (float)x;
            float fy = (float)y;
            float fz = (float)z;

            float mag = sqrtf(fx*fx + fy*fy + fz*fz);
            float motion = fabsf(mag - 16384);

            motion_avg = 0.95f * motion_avg + 0.05f * motion;

            if (motion_avg > 2500) g_state = "RUN";
            else if (motion_avg > 1000) g_state = "WALK";
            else g_state = "SIT";
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


// -------- DISPLAY --------
void display_task(void *arg)
{
    while (1)
    {
        ssd1306_clear();

        // TOP
        if (!g_finger)
        {
            ssd1306_draw_text_big(0,5, "TAP SENSOR");
        }
        else if (g_bpm > 0)
        {
            char buf[16];
            sprintf(buf, "PULS: %d", (int)g_bpm);
            ssd1306_draw_text_big(2, 0, buf);
        }
        else
        {
            ssd1306_draw_text_big(0, 0, "...");
        }

        // BOTTOM
        ssd1306_draw_text_big(0, 32, g_state);

        ssd1306_update();

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


// -------- MAIN --------
void app_main()
{
    i2c_master_init();

    ssd1306_init();
    max30102_init();
    mpu6050_init();

    xTaskCreate(pulse_task, "pulse", 4096, NULL, 5, NULL);
    xTaskCreate(motion_task, "motion", 4096, NULL, 5, NULL);
    xTaskCreate(display_task, "display", 4096, NULL, 5, NULL);
}