/**
 ******************************************************************************
 * @file           : sensor_interface.c
 * @brief          : Sensor interface implementation
 * @author         : Blood Pressure Monitor
 * @date           : 2026-03-21
 ******************************************************************************
 */

#include "sensor_interface.h"

/**
 * @brief Convert ADC value to voltage
 */
float sensor_adc_to_voltage(uint16_t adc_value)
{
    /* Voltage = (ADC_value / ADC_MAX_VALUE) * Vref */
    return ((float)adc_value / ADC_MAX_VALUE) * ADC_VREF;
}

/**
 * @brief Convert voltage to pressure (kPa)
 */
float sensor_voltage_to_pressure_kpa(float voltage)
{
    /* MPX5050: Vout = (Vs / 5) * (0.2 + 40 * P) for P in kPa
     * Solving for P: P = (Vout * 5 / Vs - 0.2) / 40
     * With Vref = 3.3V: P = (Vout - 0.2) / (5/3.3) / 40 * 5
     * Simplified: P = (Vout - 0.2) * (3.3 / 5) * (1 / 40) * 5
     * Further: P = (Vout - 0.2) / 242.424  [simplified multiplier]
     */
    
    if (voltage < MPX5050_VOUT_MIN) {
        return MPX5050_PRESSURE_MIN;
    }
    
    float pressure = (voltage - MPX5050_VOUT_MIN) / 
                    (MPX5050_VOUT_MAX - MPX5050_VOUT_MIN) * 
                    (MPX5050_PRESSURE_MAX - MPX5050_PRESSURE_MIN) + 
                    MPX5050_PRESSURE_MIN;
    
    if (pressure < MPX5050_PRESSURE_MIN) {
        return MPX5050_PRESSURE_MIN;
    }
    if (pressure > MPX5050_PRESSURE_MAX) {
        return MPX5050_PRESSURE_MAX;
    }
    
    return pressure;
}

/**
 * @brief Convert kPa to mmHg
 */
float sensor_kpa_to_mmhg(float pressure_kpa)
{
    return pressure_kpa * KPA_TO_MMHG;
}

/**
 * @brief Direct conversion from ADC to pressure in mmHg
 */
float sensor_adc_to_pressure_mmhg(uint16_t adc_value)
{
    float voltage = sensor_adc_to_voltage(adc_value);
    float pressure_kpa = sensor_voltage_to_pressure_kpa(voltage);
    return sensor_kpa_to_mmhg(pressure_kpa);
}

/**
 * @brief Initialize sensor interface
 */
bool sensor_interface_init(void)
{
    /* Add any sensor initialization code here */
    return true;
}
