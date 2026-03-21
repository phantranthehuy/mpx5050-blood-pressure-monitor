/**
 ******************************************************************************
 * @file           : test_peak_detection.c
 * @brief          : Test implementation for peak detection algorithm
 * @author         : Blood Pressure Monitor Team
 * @date           : 2026-03-21
 ******************************************************************************
 */

#include "test_peak_detection.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* ==================== Sample Pressure Data ==================== */

/* Realistic blood pressure measurement - approximately normal BP */
/* (Systolic ~120 mmHg, Diastolic ~80 mmHg) */
static const float REALISTIC_PRESSURE_DATA[] = {
    80.0, 82.0, 85.0, 88.0, 92.0, 96.0, 100.0, 105.0, 110.0, 115.0,
    118.0, 120.0, 119.0, 117.0, 115.0, 112.0, 109.0, 106.0, 103.0, 100.0,
    98.0, 96.0, 95.0, 94.0, 93.0, 92.0, 91.0, 90.0, 89.0, 88.0,
    87.0, 86.0, 85.0, 84.0, 83.0, 82.0, 81.0, 80.0, 79.5, 79.0
};

/* Hypertensive measurement - elevated BP */
/* (Systolic ~150 mmHg, Diastolic ~100 mmHg) */
static const float HYPERTENSIVE_PRESSURE_DATA[] = {
    100.0, 105.0, 110.0, 115.0, 120.0, 125.0, 130.0, 135.0, 140.0, 145.0,
    148.0, 150.0, 149.0, 147.0, 145.0, 140.0, 135.0, 130.0, 125.0, 120.0,
    118.0, 116.0, 113.0, 111.0, 110.0, 109.0, 108.0, 107.0, 106.0, 105.0,
    104.0, 103.0, 102.0, 101.0, 100.5, 100.0
};

/* Hypotensive measurement - low BP */
/* (Systolic ~90 mmHg, Diastolic ~60 mmHg) */
static const float HYPOTENSIVE_PRESSURE_DATA[] = {
    60.0, 62.0, 65.0, 68.0, 71.0, 74.0, 77.0, 80.0, 83.0, 86.0,
    88.0, 90.0, 89.0, 87.0, 85.0, 83.0, 81.0, 79.0, 77.0, 75.0,
    74.0, 73.0, 72.0, 71.0, 70.0, 69.0, 68.0, 67.0, 66.0, 65.0,
    64.0, 63.0, 62.0, 61.0, 60.5, 60.0
};

/* ==================== Test Implementations ==================== */

/**
 * @brief Generate synthetic waveform with noise
 */
uint16_t generate_synthetic_waveform(float *buffer, uint16_t size,
                                     float systolic, float diastolic,
                                     float noise_level)
{
    if (buffer == NULL || size == 0) {
        return 0;
    }

    float baseline = (systolic + diastolic) / 2.0f;
    float amplitude = (systolic - diastolic) / 2.0f;
    uint16_t samples = 0;

    for (uint16_t i = 0; i < size; i++) {
        /* Generate sine wave with multiple harmonics */
        float angle = (2.0f * 3.14159f * i) / (size / 3.0f);
        float signal = baseline + amplitude * sinf(angle);

        /* Add harmonics for realistic waveform */
        signal += (amplitude * 0.3f) * sinf(angle * 2.0f);
        signal += (amplitude * 0.15f) * sinf(angle * 3.0f);

        /* Add random noise */
        float noise = (rand() % 100 - 50) / 100.0f * noise_level;
        buffer[i] = signal + noise;

        /* Clamp to reasonable range */
        if (buffer[i] < 30.0f) buffer[i] = 30.0f;
        if (buffer[i] > 250.0f) buffer[i] = 250.0f;

        samples++;
    }

    return samples;
}

/**
 * @brief Display test results
 */
void display_test_results(const bp_measurement_t *measurement,
                         const char *test_name)
{
    printf("\n========== %s ==========\n", test_name);

    if (measurement->measurement_valid) {
        printf("✓ Measurement VALID\n");
        printf("  Systolic:      %d mmHg\n", measurement->systolic);
        printf("  Diastolic:     %d mmHg\n", measurement->diastolic);
        printf("  MAP:           %d mmHg\n", measurement->mean_arterial);
        printf("  Pulse Rate:    %d bpm\n", measurement->pulse_rate);
    } else {
        printf("✗ Measurement INVALID\n");
    }
    printf("===========================\n");
}

/**
 * @brief Test with realistic measurement
 */
bool test_realistic_measurement(void)
{
    printf("\n[TEST 1] Realistic Blood Pressure Measurement\n");
    printf("Expected: SBP ~120 mmHg, DBP ~80 mmHg\n");

    peak_detector_t detector;
    bp_measurement_t bp_result;
    uint32_t time_ms = 0;

    /* Initialize detector */
    if (!peak_detector_init(&detector, 512, 30)) {
        printf("ERROR: Failed to initialize detector\n");
        return false;
    }

    /* Reset with baseline */
    peak_detector_reset(&detector, REALISTIC_PRESSURE_DATA[0]);

    /* Process samples */
    uint16_t sample_count = sizeof(REALISTIC_PRESSURE_DATA) / sizeof(float);
    for (uint16_t i = 0; i < sample_count; i++) {
        peak_detector_process_sample(&detector,
                                    REALISTIC_PRESSURE_DATA[i],
                                    time_ms);
        time_ms += 10;  /* 10ms per sample = 100Hz */
    }

    /* Calculate BP */
    bool success = peak_detector_calculate_bp(&detector, &bp_result);
    display_test_results(&bp_result, "Realistic Measurement");

    printf("Peaks detected: %d\n", peak_detector_get_peaks_count(&detector));

    peak_detector_deinit(&detector);
    return success && bp_result.measurement_valid;
}

/**
 * @brief Test with noisy data
 */
bool test_noisy_data(void)
{
    printf("\n[TEST 2] Noisy Data Test\n");
    printf("Testing filter robustness with artificial noise\n");

    peak_detector_t detector;
    bp_measurement_t bp_result;
    uint32_t time_ms = 0;
    float buffer[320];

    /* Generate synthetic waveform with noise */
    uint16_t sample_count = generate_synthetic_waveform(buffer, 320,
                                                        125.0f, 85.0f, 5.0f);

    /* Initialize detector */
    if (!peak_detector_init(&detector, 512, 30)) {
        printf("ERROR: Failed to initialize detector\n");
        return false;
    }

    peak_detector_reset(&detector, buffer[0]);

    /* Process samples */
    for (uint16_t i = 0; i < sample_count; i++) {
        peak_detector_process_sample(&detector, buffer[i], time_ms);
        time_ms += 10;
    }

    /* Calculate BP */
    bool success = peak_detector_calculate_bp(&detector, &bp_result);
    display_test_results(&bp_result, "Noisy Data Test");

    printf("Peaks detected: %d\n", peak_detector_get_peaks_count(&detector));

    peak_detector_deinit(&detector);
    return success;
}

/**
 * @brief Test edge cases
 */
bool test_edge_cases(void)
{
    printf("\n[TEST 3] Edge Cases Test\n");

    peak_detector_t detector;
    bp_measurement_t bp_result;
    uint32_t time_ms = 0;

    /* Test 1: Very few peaks */
    printf("\n  3a. Minimum peaks (stress test):\n");
    if (!peak_detector_init(&detector, 512, 30)) {
        printf("  ERROR: Failed to initialize\n");
        return false;
    }

    peak_detector_reset(&detector, 80.0f);
    float min_peaks[] = {80, 85, 70};
    for (uint16_t i = 0; i < 3; i++) {
        peak_detector_process_sample(&detector, min_peaks[i], time_ms);
        time_ms += 500;  /* Add delay between samples */
    }

    bool test3a = peak_detector_calculate_bp(&detector, &bp_result);
    display_test_results(&bp_result, "Minimum Peaks");
    peak_detector_deinit(&detector);

    /* Test 2: Stable pressure (no oscillation) */
    printf("\n  3b. Stable pressure (no peaks):\n");
    if (!peak_detector_init(&detector, 512, 30)) {
        printf("  ERROR: Failed to initialize\n");
        return false;
    }

    peak_detector_reset(&detector, 100.0f);
    for (uint16_t i = 0; i < 50; i++) {
        peak_detector_process_sample(&detector, 100.0f, time_ms);
        time_ms += 10;
    }

    bool test3b = peak_detector_calculate_bp(&detector, &bp_result);
    display_test_results(&bp_result, "Stable Pressure");
    peak_detector_deinit(&detector);

    return test3a || test3b;
}

/**
 * @brief Test filter response
 */
bool test_filter_response(void)
{
    printf("\n[TEST 4] Filter Response Test\n");

    peak_detector_t detector;
    uint32_t time_ms = 0;

    if (!peak_detector_init(&detector, 512, 30)) {
        printf("ERROR: Failed to initialize\n");
        return false;
    }

    peak_detector_reset(&detector, 100.0f);

    /* Test step response */
    printf("  Testing step response:\n");
    printf("  (sudden jump from 100 to 120 mmHg)\n");

    /* Steady state */
    for (int i = 0; i < 20; i++) {
        peak_detector_process_sample(&detector, 100.0f, time_ms);
        time_ms += 10;
    }

    /* Step input */
    printf("  Time(ms)\tRaw(mmHg)\tFiltered(mmHg)\n");
    for (int i = 0; i < 30; i++) {
        peak_data_t *data = peak_detector_process_sample(&detector,
                                                         120.0f,
                                                         time_ms);
        if (i % 5 == 0) {
            printf("  %4d\t\t120.0\t\t%.2f\n", time_ms, data->pressure_value);
        }
        time_ms += 10;
    }

    peak_detector_deinit(&detector);
    return true;
}

/**
 * @brief Run all tests
 */
bool run_all_tests(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║  Peak Detection Algorithm - Comprehensive Test Suite  ║\n");
    printf("║  Blood Pressure Measurement System                   ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

    bool test1 = test_realistic_measurement();
    bool test2 = test_noisy_data();
    bool test3 = test_edge_cases();
    bool test4 = test_filter_response();

    /* Summary */
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║                    TEST SUMMARY                       ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    printf("║ Test 1 - Realistic Measurement:  %s              ║\n",
           test1 ? "✓ PASSED" : "✗ FAILED");
    printf("║ Test 2 - Noisy Data:              %s              ║\n",
           test2 ? "✓ PASSED" : "✗ FAILED");
    printf("║ Test 3 - Edge Cases:              %s              ║\n",
           test3 ? "✓ PASSED" : "✗ FAILED");
    printf("║ Test 4 - Filter Response:         %s              ║\n",
           test4 ? "✓ PASSED" : "✗ FAILED");
    printf("╚════════════════════════════════════════════════════════╝\n");

    return test1 && test2 && test3 && test4;
}
