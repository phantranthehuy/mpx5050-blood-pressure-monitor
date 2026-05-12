#include "pressure.h"
#include "board_config.h"

float pressure_counts_to_mmhg(int16_t adc_counts)
{
    float x = (float)adc_counts + (float)PRESSURE_ADC_OFFSET_COUNTS;
    return x * PRESSURE_ADC_SCALE_MMHG_PER_COUNT;
}
