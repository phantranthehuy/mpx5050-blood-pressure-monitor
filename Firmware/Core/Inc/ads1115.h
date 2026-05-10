#ifndef ADS1115_H
#define ADS1115_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t dev_addr_8bit;
} Ads1115_Handle;

void ads1115_init(Ads1115_Handle *h, I2C_HandleTypeDef *hi2c, uint16_t dev_addr_8bit);

/** Đọc kênh AIN0–GND, PGA ±4.096 V, 475 SPS single-shot. Trả về HAL_OK hoặc lỗi bus. */
HAL_StatusTypeDef ads1115_read_channel0_counts(Ads1115_Handle *h, int16_t *out_counts);

#endif
