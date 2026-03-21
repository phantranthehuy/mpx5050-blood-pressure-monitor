/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Blood Pressure Monitor Team
 * @brief          : Main program body with peak detection algorithm
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include <stdint.h>
#include <stdio.h>
#include "peak_detection.h"
#include "sensor_interface.h"

/* ==================== Definitions ==================== */
#define ADC_BUFFER_SIZE         256
#define MAX_PEAKS_TO_DETECT     20
#define SAMPLING_PERIOD_MS      10      /* 10ms = 100Hz sampling rate */

/* ==================== Global Variables ==================== */
static peak_detector_t g_peak_detector;
static uint32_t g_current_time = 0;

/* ==================== Function Declarations ==================== */
void system_init(void);
void adc_init(void);
uint16_t read_adc_sample(void);
void process_measurement(void);

/* ==================== Main Program ==================== */
int main(void)
{
    /* Initialize system */
    system_init();
    adc_init();
    sensor_interface_init();

    /* Initialize peak detector */
    if (!peak_detector_init(&g_peak_detector, ADC_BUFFER_SIZE, MAX_PEAKS_TO_DETECT)) {
        /* Initialization failed */
        return -1;
    }

    /* Main measurement loop */
    while (1) {
        /* Read ADC sample */
        uint16_t adc_value = read_adc_sample();

        /* Convert to pressure in mmHg */
        float pressure_mmhg = sensor_adc_to_pressure_mmhg(adc_value);

        /* Process sample through peak detector */
        peak_data_t *peak_data = peak_detector_process_sample(
            &g_peak_detector,
            pressure_mmhg,
            g_current_time
        );

        if (peak_data != NULL) {
            if (peak_data->is_peak) {
                /* Peak detected - can trigger an event here */
                /* Example: LED blink, event logging, etc. */
            }
        }

        /* Check if we have enough peaks for a measurement */
        if (peak_detector_get_peaks_count(&g_peak_detector) >= 5) {
            /* Process measurement */
            process_measurement();
            
            /* Reset for next measurement */
            peak_detector_reset(&g_peak_detector, pressure_mmhg);
        }

        /* Update time (in real application, use systick or timer) */
        g_current_time += SAMPLING_PERIOD_MS;

        /* Small delay to simulate sampling period */
        /* In real application, use timer interrupt */
    }

    /* Cleanup */
    peak_detector_deinit(&g_peak_detector);
    return 0;
}

/* ==================== Function Implementations ==================== */

/**
 * @brief Initialize system peripherals
 */
void system_init(void)
{
    /* Initialize MCU peripherals */
    /* Clock configuration */
    /* GPIO initialization */
    /* Add your STM32 initialization code here */
}

/**
 * @brief Initialize ADC
 */
void adc_init(void)
{
    /* ADC initialization for MPX5050 sensor on PA0 (or appropriate pin) */
    /* Add your ADC configuration here */
}

/**
 * @brief Read ADC sample
 */
uint16_t read_adc_sample(void)
{
    /* Read from ADC channel connected to MPX5050 sensor */
    /* This is a placeholder - implement with actual ADC reading */
    return 0;
}

/**
 * @brief Process measurement and display results
 */
void process_measurement(void)
{
    bp_measurement_t bp_result;

    /* Calculate blood pressure from detected peaks */
    if (peak_detector_calculate_bp(&g_peak_detector, &bp_result)) {
        if (bp_result.measurement_valid) {
            /* Display or process results */
            printf("Blood Pressure Measurement:\r\n");
            printf("Systolic:  %d mmHg\r\n", bp_result.systolic);
            printf("Diastolic: %d mmHg\r\n", bp_result.diastolic);
            printf("MAP:       %d mmHg\r\n", bp_result.mean_arterial);
            printf("Pulse:     %d bpm\r\n\r\n", bp_result.pulse_rate);
        } else {
            printf("Measurement invalid - please try again\r\n");
        }
    }

    /* Get statistics */
    float min_p, max_p, mean_p;
    peak_detector_get_statistics(&g_peak_detector, &min_p, &max_p, &mean_p);
    printf("Statistics - Min: %.2f, Max: %.2f, Mean: %.2f mmHg\r\n", 
           min_p, max_p, mean_p);
}
