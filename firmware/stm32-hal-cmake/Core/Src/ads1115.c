#include "ads1115.h"

/* ADS1115 pointer register */
#define ADS1115_REG_CONV   0x00u
#define ADS1115_REG_CFG    0x01u

/*
 * OS=1 | MUX=100 (AIN0 vs GND) | PGA=001 (±4.096V) | MODE=1 single | DR=110 (475 SPS)
 */
static uint16_t build_cfg(void)
{
    uint16_t v = 0;
    v |= (1u << 15);              /* OS start single conversion */
    v |= (4u << 12);              /* MUX AIN0–GND */
    v |= (1u << 9);               /* PGA ±4.096 V */
    v |= (1u << 8);               /* single-shot */
    v |= (6u << 5);               /* DR 475 SPS */
    return v;
}

void ads1115_init(Ads1115_Handle *h, I2C_HandleTypeDef *hi2c, uint16_t dev_addr_8bit)
{
    h->hi2c = hi2c;
    h->dev_addr_8bit = dev_addr_8bit;
}

static HAL_StatusTypeDef write_reg16(Ads1115_Handle *h, uint8_t reg, uint16_t val)
{
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (uint8_t)((val >> 8) & 0xFFu);
    buf[2] = (uint8_t)(val & 0xFFu);
    return HAL_I2C_Master_Transmit(h->hi2c, h->dev_addr_8bit, buf, 3u, 50u);
}

static HAL_StatusTypeDef read_reg16(Ads1115_Handle *h, uint8_t reg, uint16_t *out)
{
    uint8_t rx[2];
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(h->hi2c, h->dev_addr_8bit, &reg, 1u, 50u);
    if (st != HAL_OK) return st;
    st = HAL_I2C_Master_Receive(h->hi2c, h->dev_addr_8bit, rx, 2u, 50u);
    if (st != HAL_OK) return st;
    *out = ((uint16_t)rx[0] << 8) | rx[1];
    return HAL_OK;
}

HAL_StatusTypeDef ads1115_read_channel0_counts(Ads1115_Handle *h, int16_t *out_counts)
{
    uint16_t cfg = build_cfg();
    HAL_StatusTypeDef st = write_reg16(h, ADS1115_REG_CFG, cfg);
    if (st != HAL_OK) return st;

    /* ~475 SPS → ~2.1 ms + margin */
    HAL_Delay(4);

    uint16_t raw = 0;
    st = read_reg16(h, ADS1115_REG_CONV, &raw);
    if (st != HAL_OK) return st;

    *out_counts = (int16_t)raw;
    return HAL_OK;
}
