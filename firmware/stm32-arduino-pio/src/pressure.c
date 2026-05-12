#include "pressure.h"
#include "board_config.h"

float pressure_counts_to_mmhg(int16_t adc_counts)
{
    /* Điện áp tại AIN0 (cảm biến nối trực tiếp, không chia áp). */
    const float v_lsb = ADS1115_PGA_FSR_VOLTS / 32768.0f;
    float vout = (float)adc_counts * v_lsb;

    /* Datasheet MPX5050: P_kPa = (Vout/Vs - 0.04) / 0.018 (ratiometric theo Vs). */
    float p_kpa = (vout / MPX5050_VS_VOLTS - 0.04f) / 0.018f;

    /* 760 mmHg / 101.325 kPa */
    const float kpa_to_mmhg = 7.50061683f;
    return p_kpa * kpa_to_mmhg;
}
