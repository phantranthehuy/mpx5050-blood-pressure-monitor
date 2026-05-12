#ifndef PRESSURE_H
#define PRESSURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

float pressure_counts_to_mmhg(int16_t adc_counts);

#ifdef __cplusplus
}
#endif

#endif
