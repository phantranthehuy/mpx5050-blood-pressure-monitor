#ifndef ADS1115_H
#define ADS1115_H

#include <stdint.h>

typedef struct {
    uint8_t _unused;
} Ads1115_Handle;

#ifdef __cplusplus
extern "C" {
#endif

void ads1115_init(Ads1115_Handle *h);
/** 0 = OK, khác 0 = lỗi bus */
int ads1115_read_channel0_counts(Ads1115_Handle *h, int16_t *out_counts);

#ifdef __cplusplus
}
#endif

#endif
