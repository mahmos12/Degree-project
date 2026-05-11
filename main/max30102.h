#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>

void max30102_init(void);
uint32_t max30102_read_ir(void);
float max30102_get_bpm(void);
int max30102_finger_present(void);

#endif