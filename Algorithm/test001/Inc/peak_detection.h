/**
 ******************************************************************************
 * @file           : peak_detection.h
 * @brief          : Peak detection algorithm for blood pressure measurement
 * @author         : Blood Pressure Monitor
 * @date           : 2026-03-21
 ******************************************************************************
 */

#ifndef PEAK_DETECTION_H
#define PEAK_DETECTION_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== Data Types ==================== */

/**
 * @struct peak_data_t
 * @brief Structure to hold peak detection data
 */
typedef struct {
    float pressure_value;      /**< Current pressure value (mmHg) */
    float derivative;          /**< Pressure derivative (rate of change) */
    uint32_t timestamp;        /**< Timestamp of the sample (ms) */
    bool is_peak;              /**< Flag indicating if this sample is a peak */
    float peak_magnitude;      /**< Magnitude of the detected peak */
} peak_data_t;

/**
 * @struct bp_measurement_t
 * @brief Structure to hold blood pressure measurement results
 */
typedef struct {
    uint16_t systolic;         /**< Systolic pressure (mmHg) */
    uint16_t diastolic;        /**< Diastolic pressure (mmHg) */
    uint16_t mean_arterial;    /**< Mean arterial pressure (mmHg) */
    uint16_t pulse_rate;       /**< Heart rate (bpm) */
    bool measurement_valid;    /**< Flag indicating if measurement is valid */
} bp_measurement_t;

/**
 * @struct peak_detector_t
 * @brief Peak detector state machine structure
 */
typedef struct {
    float baseline_pressure;           /**< Reference baseline pressure */
    float detection_threshold;         /**< Threshold for peak detection */
    uint16_t buffer_size;              /**< Size of the circular buffer */
    uint16_t buffer_index;             /**< Current index in circular buffer */
    float *pressure_buffer;            /**< Circular buffer for pressure samples */
    float *filtered_buffer;            /**< Filtered pressure samples */
    peak_data_t *peaks_buffer;         /**< Buffer to store detected peaks */
    uint16_t peaks_count;              /**< Number of peaks detected */
    uint16_t max_peaks;                /**< Maximum number of peaks to store */
    
    /* Filter coefficients */
    float alpha;                       /**< Low-pass filter coefficient */
    float last_filtered_value;         /**< Previous filtered value */
    
    /* Peak detection state */
    bool in_peak;                      /**< Flag indicating if currently in a peak */
    float peak_start_value;            /**< Pressure value at peak start */
    uint32_t peak_start_time;          /**< Time at peak start */
    bool last_derivative_positive;     /**< Previous derivative sign */
    
    /* Measurement parameters */
    uint32_t measurement_start_time;   /**< Start time of measurement */
    uint16_t sample_count;             /**< Total samples collected */
} peak_detector_t;

/* ==================== Function Prototypes ==================== */

/**
 * @brief Initialize peak detector
 * @param detector Pointer to peak detector structure
 * @param buffer_size Size of the circular buffer
 * @param max_peaks Maximum number of peaks to track
 * @return true if initialization successful, false otherwise
 */
bool peak_detector_init(peak_detector_t *detector, uint16_t buffer_size, uint16_t max_peaks);

/**
 * @brief Deinitialize peak detector and free memory
 * @param detector Pointer to peak detector structure
 */
void peak_detector_deinit(peak_detector_t *detector);

/**
 * @brief Reset peak detector state for new measurement
 * @param detector Pointer to peak detector structure
 * @param baseline_pressure Initial baseline pressure (mmHg)
 */
void peak_detector_reset(peak_detector_t *detector, float baseline_pressure);

/**
 * @brief Process a single pressure sample
 * @param detector Pointer to peak detector structure
 * @param pressure_value Raw pressure value (mmHg)
 * @param timestamp Current timestamp (ms)
 * @return Pointer to peak_data_t containing analysis, or NULL if no peak detected
 */
peak_data_t* peak_detector_process_sample(peak_detector_t *detector, 
                                         float pressure_value, 
                                         uint32_t timestamp);

/**
 * @brief Apply low-pass filter to smooth pressure signal
 * @param detector Pointer to peak detector structure
 * @param raw_value Raw pressure value
 * @return Filtered pressure value
 */
float peak_detector_low_pass_filter(peak_detector_t *detector, float raw_value);

/**
 * @brief Calculate derivative (rate of change) of pressure signal
 * @param detector Pointer to peak detector structure
 * @return Derivative value
 */
float peak_detector_calculate_derivative(peak_detector_t *detector);

/**
 * @brief Detect peaks in the filtered signal using derivative method
 * @param detector Pointer to peak detector structure
 * @param current_derivative Current derivative value
 * @return true if peak detected, false otherwise
 */
bool peak_detector_detect_peak(peak_detector_t *detector, float current_derivative);

/**
 * @brief Get detected peaks count
 * @param detector Pointer to peak detector structure
 * @return Number of peaks detected
 */
uint16_t peak_detector_get_peaks_count(const peak_detector_t *detector);

/**
 * @brief Get specific peak data
 * @param detector Pointer to peak detector structure
 * @param index Peak index
 * @return Pointer to peak_data_t or NULL if index out of range
 */
const peak_data_t* peak_detector_get_peak(const peak_detector_t *detector, uint16_t index);

/**
 * @brief Calculate blood pressure measurements from detected peaks
 * @param detector Pointer to peak detector structure
 * @param measurement Pointer to bp_measurement_t structure to store results
 * @return true if measurement is valid, false otherwise
 */
bool peak_detector_calculate_bp(peak_detector_t *detector, bp_measurement_t *measurement);

/**
 * @brief Set detection threshold
 * @param detector Pointer to peak detector structure
 * @param threshold Threshold value (percentage of baseline)
 */
void peak_detector_set_threshold(peak_detector_t *detector, float threshold);

/**
 * @brief Set low-pass filter coefficient
 * @param detector Pointer to peak detector structure
 * @param alpha Filter coefficient (0.0 to 1.0)
 */
void peak_detector_set_filter_coefficient(peak_detector_t *detector, float alpha);

/**
 * @brief Get current measurement statistics
 * @param detector Pointer to peak detector structure
 * @param min_pressure Pointer to store minimum pressure
 * @param max_pressure Pointer to store maximum pressure
 * @param mean_pressure Pointer to store mean pressure
 */
void peak_detector_get_statistics(const peak_detector_t *detector,
                                 float *min_pressure,
                                 float *max_pressure,
                                 float *mean_pressure);

#endif /* PEAK_DETECTION_H */
