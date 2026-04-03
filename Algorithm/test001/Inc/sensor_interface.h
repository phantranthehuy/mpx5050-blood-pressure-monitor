/**
 ******************************************************************************
 * @file           : sensor_interface.h
 * @brief          : Sensor interface for MPX5050 pressure sensor
 * @author         : Blood Pressure Monitor
 * @date           : 2026-03-21
 ******************************************************************************
 */

#ifndef SENSOR_INTERFACE_H
#define SENSOR_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== ADC Configuration ==================== */
#define ADC_RESOLUTION          12          /* 12-bit ADC */
#define ADC_MAX_VALUE           4095        /* 2^12 - 1 */
#define ADC_VREF                3.3f        /* Reference voltage 3.3V */

/* ==================== MPX5050 Sensor Configuration ==================== */
#define MPX5050_VOUT_MIN        0.2f        /* Minimum output voltage (V) at 0 kPa */
#define MPX5050_VOUT_MAX        4.7f        /* Maximum output voltage (V) at 350 kPa */
#define MPX5050_PRESSURE_MIN    0.0f        /* Minimum pressure (kPa) */
#define MPX5050_PRESSURE_MAX    350.0f      /* Maximum pressure (kPa) */

/* ==================== Pressure Conversion ==================== */
#define KPA_TO_MMHG             7.50061f    /* 1 kPa = 7.50061 mmHg */
#define PSI_TO_MMHG             51.7151f    /* 1 PSI = 51.7151 mmHg */

/* ==================== Function Prototypes ==================== */

/**
 * @brief Convert ADC value to voltage
 * @param adc_value 12-bit ADC value (0-4095)
 * @return Voltage in volts
 */
float sensor_adc_to_voltage(uint16_t adc_value);

/**
 * @brief Convert voltage to pressure (kPa)
 * @param voltage Sensor output voltage (V)
 * @return Pressure in kPa
 */
float sensor_voltage_to_pressure_kpa(float voltage);

/**
 * @brief Convert kPa to mmHg
 * @param pressure_kpa Pressure in kPa
 * @return Pressure in mmHg
 */
float sensor_kpa_to_mmhg(float pressure_kpa);

/**
 * @brief Direct conversion from ADC to pressure in mmHg
 * @param adc_value 12-bit ADC value
 * @return Pressure in mmHg
 */
float sensor_adc_to_pressure_mmhg(uint16_t adc_value);

/**
 * @brief Initialize sensor interface
 * @return true if initialization successful
 */
bool sensor_interface_init(void);

#endif /* SENSOR_INTERFACE_H */
