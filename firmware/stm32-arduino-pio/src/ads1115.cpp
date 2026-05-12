#include "ads1115.h"
#include "board_config.h"
#include <Arduino.h>
#include <Wire.h>

#define ADS1115_REG_CONV 0x00u
#define ADS1115_REG_CFG 0x01u

static uint16_t build_cfg(void)
{
    uint16_t v = 0;
    v |= (1u << 15);
    v |= (4u << 12);
    v |= (1u << 9);
    v |= (1u << 8);
    v |= (6u << 5);
    return v;
}

static bool write_reg16(uint8_t reg, uint16_t val)
{
    Wire.beginTransmission((uint8_t)ADS1115_I2C_ADDR_7BIT);
    Wire.write(reg);
    Wire.write((uint8_t)((val >> 8) & 0xFFu));
    Wire.write((uint8_t)(val & 0xFFu));
    return Wire.endTransmission() == 0;
}

static bool read_reg16(uint8_t reg, uint16_t *out)
{
    Wire.beginTransmission((uint8_t)ADS1115_I2C_ADDR_7BIT);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0)
        return false;
    if (Wire.requestFrom((uint8_t)ADS1115_I2C_ADDR_7BIT, (uint8_t)2) != 2)
        return false;
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    *out = ((uint16_t)hi << 8) | lo;
    return true;
}

extern "C" void ads1115_init(Ads1115_Handle *h)
{
    (void)h;
}

extern "C" int ads1115_read_channel0_counts(Ads1115_Handle *h, int16_t *out_counts)
{
    (void)h;
    uint16_t cfg = build_cfg();
    if (!write_reg16(ADS1115_REG_CFG, cfg))
        return -1;
    delay(4);
    uint16_t raw = 0;
    if (!read_reg16(ADS1115_REG_CONV, &raw))
        return -1;
    *out_counts = (int16_t)raw;
    return 0;
}
