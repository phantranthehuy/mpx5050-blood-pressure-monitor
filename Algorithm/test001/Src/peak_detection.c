/**
 ******************************************************************************
 * @file           : peak_detection.c
 * @brief          : Peak detection algorithm implementation for blood pressure
 * @author         : Blood Pressure Monitor
 * @date           : 2026-03-21
 ******************************************************************************
 */

#include "peak_detection.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ==================== Private Macros ==================== */
#define MIN_PEAKS_FOR_VALID_BP          3
#define PEAK_DETECTION_ALPHA            0.2f       /* Low-pass filter coefficient */
#define PEAK_THRESHOLD_PERCENT          0.5f       /* 50% of baseline */
#define DERIVATIVE_ZERO_CROSSING_THRESH 0.01f      /* Threshold for zero crossing detection */
#define MIN_PEAK_DISTANCE_MS            300        /* Minimum distance between peaks (ms) */
#define MAX_MEASUREMENT_DURATION_MS     30000      /* Maximum measurement duration (30 seconds) */

/* ==================== Function Implementations ==================== */

/**
 * @brief Initialize peak detector
 */
bool peak_detector_init(peak_detector_t *detector, uint16_t buffer_size, uint16_t max_peaks)
{
    if (detector == NULL || buffer_size == 0 || max_peaks == 0) {
        return false;
    }

    /* Allocate memory for buffers */
    detector->pressure_buffer = (float *)malloc(buffer_size * sizeof(float));
    detector->filtered_buffer = (float *)malloc(buffer_size * sizeof(float));
    detector->peaks_buffer = (peak_data_t *)malloc(max_peaks * sizeof(peak_data_t));

    if (detector->pressure_buffer == NULL || 
        detector->filtered_buffer == NULL || 
        detector->peaks_buffer == NULL) {
        /* Free any allocated memory */
        if (detector->pressure_buffer != NULL) free(detector->pressure_buffer);
        if (detector->filtered_buffer != NULL) free(detector->filtered_buffer);
        if (detector->peaks_buffer != NULL) free(detector->peaks_buffer);
        return false;
    }

    /* Initialize structure members */
    detector->buffer_size = buffer_size;
    detector->buffer_index = 0;
    detector->peaks_count = 0;
    detector->max_peaks = max_peaks;
    detector->alpha = PEAK_DETECTION_ALPHA;
    detector->last_filtered_value = 0.0f;
    detector->in_peak = false;
    detector->last_derivative_positive = false;
    detector->sample_count = 0;
    detector->baseline_pressure = 0.0f;
    detector->detection_threshold = PEAK_THRESHOLD_PERCENT;

    /* Initialize buffers to zero */
    memset(detector->pressure_buffer, 0, buffer_size * sizeof(float));
    memset(detector->filtered_buffer, 0, buffer_size * sizeof(float));
    memset(detector->peaks_buffer, 0, max_peaks * sizeof(peak_data_t));

    return true;
}

/**
 * @brief Deinitialize peak detector
 */
void peak_detector_deinit(peak_detector_t *detector)
{
    if (detector == NULL) {
        return;
    }

    if (detector->pressure_buffer != NULL) {
        free(detector->pressure_buffer);
        detector->pressure_buffer = NULL;
    }

    if (detector->filtered_buffer != NULL) {
        free(detector->filtered_buffer);
        detector->filtered_buffer = NULL;
    }

    if (detector->peaks_buffer != NULL) {
        free(detector->peaks_buffer);
        detector->peaks_buffer = NULL;
    }

    detector->buffer_size = 0;
    detector->peaks_count = 0;
}

/**
 * @brief Reset peak detector for new measurement
 */
void peak_detector_reset(peak_detector_t *detector, float baseline_pressure)
{
    if (detector == NULL) {
        return;
    }

    detector->baseline_pressure = baseline_pressure;
    detector->buffer_index = 0;
    detector->peaks_count = 0;
    detector->sample_count = 0;
    detector->in_peak = false;
    detector->last_derivative_positive = false;
    detector->last_filtered_value = baseline_pressure;
    detector->measurement_start_time = 0;

    /* Clear buffers */
    memset(detector->pressure_buffer, 0, detector->buffer_size * sizeof(float));
    memset(detector->filtered_buffer, 0, detector->buffer_size * sizeof(float));
    memset(detector->peaks_buffer, 0, detector->max_peaks * sizeof(peak_data_t));
}

/**
 * @brief Apply low-pass filter (exponential moving average)
 */
float peak_detector_low_pass_filter(peak_detector_t *detector, float raw_value)
{
    if (detector == NULL) {
        return raw_value;
    }

    float filtered_value = detector->alpha * raw_value + 
                          (1.0f - detector->alpha) * detector->last_filtered_value;
    
    detector->last_filtered_value = filtered_value;
    return filtered_value;
}

/**
 * @brief Calculate derivative (rate of change) of pressure
 */
float peak_detector_calculate_derivative(peak_detector_t *detector)
{
    if (detector == NULL || detector->buffer_index < 2) {
        return 0.0f;
    }

    uint16_t current_idx = detector->buffer_index;
    uint16_t prev_idx = (current_idx - 1 + detector->buffer_size) % detector->buffer_size;

    float current_filtered = detector->filtered_buffer[current_idx];
    float prev_filtered = detector->filtered_buffer[prev_idx];

    float derivative = current_filtered - prev_filtered;
    return derivative;
}

/**
 * @brief Detect peak using zero-crossing of derivative
 */
bool peak_detector_detect_peak(peak_detector_t *detector, float current_derivative)
{
    if (detector == NULL) {
        return false;
    }

    bool peak_detected = false;

    /* Detect peak when derivative changes from positive to negative */
    if (detector->last_derivative_positive && 
        current_derivative < -DERIVATIVE_ZERO_CROSSING_THRESH) {
        peak_detected = true;
    }

    detector->last_derivative_positive = (current_derivative > DERIVATIVE_ZERO_CROSSING_THRESH);
    return peak_detected;
}

/**
 * @brief Process a single pressure sample
 */
peak_data_t* peak_detector_process_sample(peak_detector_t *detector, 
                                         float pressure_value, 
                                         uint32_t timestamp)
{
    if (detector == NULL) {
        return NULL;
    }

    /* Initialize measurement start time on first sample */
    if (detector->sample_count == 0) {
        detector->measurement_start_time = timestamp;
    }

    /* Check measurement duration limit */
    if ((timestamp - detector->measurement_start_time) > MAX_MEASUREMENT_DURATION_MS) {
        return NULL;
    }

    /* Update baseline on first sample */
    if (detector->sample_count == 0) {
        detector->baseline_pressure = pressure_value;
        detector->last_filtered_value = pressure_value;
    }

    /* Store raw pressure value */
    detector->pressure_buffer[detector->buffer_index] = pressure_value;

    /* Apply low-pass filter */
    float filtered_value = peak_detector_low_pass_filter(detector, pressure_value);
    detector->filtered_buffer[detector->buffer_index] = filtered_value;

    /* Calculate derivative */
    float derivative = peak_detector_calculate_derivative(detector);

    /* Detect peaks */
    bool is_peak = peak_detector_detect_peak(detector, derivative);

    /* Check for minimum distance between peaks */
    if (is_peak && detector->peaks_count > 0) {
        uint32_t last_peak_time = detector->peaks_buffer[detector->peaks_count - 1].timestamp;
        if ((timestamp - last_peak_time) < MIN_PEAK_DISTANCE_MS) {
            is_peak = false;
        }
    }

    /* Store peak if detected and buffer has space */
    if (is_peak && detector->peaks_count < detector->max_peaks) {
        peak_data_t *peak = &detector->peaks_buffer[detector->peaks_count];
        peak->pressure_value = filtered_value;
        peak->derivative = derivative;
        peak->timestamp = timestamp;
        peak->is_peak = true;
        peak->peak_magnitude = filtered_value - detector->baseline_pressure;

        detector->peaks_count++;
    }

    /* Prepare return data */
    static peak_data_t current_data;
    current_data.pressure_value = filtered_value;
    current_data.derivative = derivative;
    current_data.timestamp = timestamp;
    current_data.is_peak = is_peak;
    current_data.peak_magnitude = filtered_value - detector->baseline_pressure;

    /* Update circular buffer index */
    detector->buffer_index = (detector->buffer_index + 1) % detector->buffer_size;
    detector->sample_count++;

    return &current_data;
}

/**
 * @brief Get peaks count
 */
uint16_t peak_detector_get_peaks_count(const peak_detector_t *detector)
{
    if (detector == NULL) {
        return 0;
    }
    return detector->peaks_count;
}

/**
 * @brief Get specific peak data
 */
const peak_data_t* peak_detector_get_peak(const peak_detector_t *detector, uint16_t index)
{
    if (detector == NULL || index >= detector->peaks_count) {
        return NULL;
    }
    return &detector->peaks_buffer[index];
}

/**
 * @brief Calculate blood pressure from detected peaks
 */
bool peak_detector_calculate_bp(peak_detector_t *detector, bp_measurement_t *measurement)
{
    if (detector == NULL || measurement == NULL) {
        return false;
    }

    /* Need at least 3 peaks for valid measurement */
    if (detector->peaks_count < MIN_PEAKS_FOR_VALID_BP) {
        measurement->measurement_valid = false;
        return false;
    }

    /* Find systolic (maximum pressure) and diastolic (minimum pressure) */
    float max_pressure = detector->peaks_buffer[0].peak_magnitude;
    float min_pressure = detector->peaks_buffer[0].peak_magnitude;
    float sum_pressure = 0.0f;

    for (uint16_t i = 0; i < detector->peaks_count; i++) {
        float peak_mag = detector->peaks_buffer[i].peak_magnitude;
        if (peak_mag > max_pressure) {
            max_pressure = peak_mag;
        }
        if (peak_mag < min_pressure) {
            min_pressure = peak_mag;
        }
        sum_pressure += peak_mag;
    }

    /* Convert from magnitude to absolute pressure values */
    float systolic = detector->baseline_pressure + max_pressure;
    float diastolic = detector->baseline_pressure + min_pressure;
    float mean_arterial = detector->baseline_pressure + (sum_pressure / detector->peaks_count);

    /* Validate pressure ranges */
    if (systolic < 60 || systolic > 250 || diastolic < 30 || diastolic > 200) {
        measurement->measurement_valid = false;
        return false;
    }

    if (systolic <= diastolic) {
        measurement->measurement_valid = false;
        return false;
    }

    /* Calculate pulse rate (beats per minute) */
    uint32_t total_time = detector->measurement_start_time;
    if (detector->sample_count > 1) {
        /* Time span is from first to last sample */
        float time_minutes = 60.0f / 1000.0f; /* Convert ms to minutes for 1 second */
        float pulse_rate = (detector->peaks_count - 1) * time_minutes;
        measurement->pulse_rate = (uint16_t)pulse_rate;
    } else {
        measurement->pulse_rate = 0;
    }

    /* Store measurements */
    measurement->systolic = (uint16_t)systolic;
    measurement->diastolic = (uint16_t)diastolic;
    measurement->mean_arterial = (uint16_t)mean_arterial;
    measurement->measurement_valid = true;

    return true;
}

/**
 * @brief Set detection threshold
 */
void peak_detector_set_threshold(peak_detector_t *detector, float threshold)
{
    if (detector != NULL && threshold > 0.0f && threshold < 1.0f) {
        detector->detection_threshold = threshold;
    }
}

/**
 * @brief Set filter coefficient
 */
void peak_detector_set_filter_coefficient(peak_detector_t *detector, float alpha)
{
    if (detector != NULL && alpha >= 0.0f && alpha <= 1.0f) {
        detector->alpha = alpha;
    }
}

/**
 * @brief Get measurement statistics
 */
void peak_detector_get_statistics(const peak_detector_t *detector,
                                 float *min_pressure,
                                 float *max_pressure,
                                 float *mean_pressure)
{
    if (detector == NULL) {
        return;
    }

    if (detector->sample_count == 0) {
        if (min_pressure) *min_pressure = 0.0f;
        if (max_pressure) *max_pressure = 0.0f;
        if (mean_pressure) *mean_pressure = 0.0f;
        return;
    }

    float min_val = detector->filtered_buffer[0];
    float max_val = detector->filtered_buffer[0];
    float sum_val = 0.0f;

    uint16_t count = (detector->sample_count < detector->buffer_size) ? 
                     detector->sample_count : detector->buffer_size;

    for (uint16_t i = 0; i < count; i++) {
        float val = detector->filtered_buffer[i];
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
        sum_val += val;
    }

    if (min_pressure) *min_pressure = min_val;
    if (max_pressure) *max_pressure = max_val;
    if (mean_pressure) *mean_pressure = sum_val / count;
}
