#ifndef DISPLAY_H
#define DISPLAY_H

enum sensor_type
{
    SENSOR_NONE,
    SENSOR_SPO2,
    SENSOR_CO2,

    SENSOR_TOP,
};

void display_init(void);

void display_print(enum sensor_type type, float val);

#endif /* DISPLAY_H */