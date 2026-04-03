/**
 ******************************************************************************
 * @file           : test_peak_detection.h
 * @brief          : Test and example functions for peak detection algorithm
 * @author         : Blood Pressure Monitor Team
 * @date           : 2026-03-21
 ******************************************************************************
 */

#ifndef TEST_PEAK_DETECTION_H
#define TEST_PEAK_DETECTION_H

#include "peak_detection.h"
#include <stdint.h>
#include <stdbool.h>

/* ==================== Test Data Samples ==================== */

/**
 * @brief Simulated blood pressure measurement data
 * This represents a typical 20-30 second measurement
 */
typedef struct {
    const float *pressure_samples;    /* Array of pressure measurements */
    uint16_t sample_count;            /* Number of samples */
    const char *description;          /* Test description */
    float expected_systolic;          /* Expected systolic value for verification */
    float expected_diastolic;         /* Expected diastolic value for verification */
    float tolerance;                  /* Tolerance for test verification (%) */
} test_data_t;

/* ==================== Function Prototypes ==================== */

/**
 * @brief Run all tests
 * @return true if all tests passed, false otherwise
 */
bool run_all_tests(void);

/**
 * @brief Test with real-like pressure data
 */
bool test_realistic_measurement(void);

/**
 * @brief Test with noisy data
 */
bool test_noisy_data(void);

/**
 * @brief Test edge cases
 */
bool test_edge_cases(void);

/**
 * @brief Test filter characteristics
 */
bool test_filter_response(void);

/**
 * @brief Display test results
 */
void display_test_results(const bp_measurement_t *measurement, 
                         const char *test_name);

/**
 * @brief Generate synthetic pressure waveform
 */
uint16_t generate_synthetic_waveform(float *buffer, uint16_t size,
                                     float systolic, float diastolic,
                                     float noise_level);

#endif /* TEST_PEAK_DETECTION_H */
