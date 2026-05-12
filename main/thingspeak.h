#ifndef THINGSPEAK_H
#define THINGSPEAK_H

void thingspeak_init(void);

void thingspeak_send(
    int bpm,
    int motion,
    int finger
);

#endif