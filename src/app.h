#ifndef APP_H
#define APP_H

void app_init(void);
void app_spo2_measurement_start(void);
uint16_t app_spo2_val_get(void);

void app_co2_measurement_start(void);
float app_co2_val_get(void);

#endif /* APP_H */