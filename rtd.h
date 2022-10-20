/*
 * rtd.h
 *
 *  Created on: Mar 16, 2021
 *      Author: ZAlemu
 */

#ifndef RTD_H_
#define RTD_H_

/*
 * The principle of operation is to measure the resistance of
 *  a platinum element.
 */
#define RTD_VOLTAGE_LEVEL           (3.3f)      // BIAS Voltage
#define RTD_VOLTAGE_LSB             (RTD_VOLTAGE_LEVEL/4096.0f) // 12-bit ADC
#define RTD_REF_RESISTOR_OHMS       (1000.0f)   // RTD resistance @ 0oC
#define RTD_REF_TEMP_C              (0.0f)      // Ref Resistance @ temp = 0oC
/*
 * For a PT1000 sensor, a 1 °C temperature change will cause a 0.384 ohm
 *  change in resistance,
 */
#define RTD_OHMS_PER_C              (3.85f)
#define RTD_C_PER_OHMS              (1.0f/RTD_OHMS_PER_C)
#define RTD_ALPHA_TEMP_COEFFICIENT  (0.00385f)
#define RTD_ONE_OVER_ALPHA          (1/RTD_ALPHA_TEMP_COEFFICIENT)

extern float gfRtdTempAvg;

void initRtd();
void transformRtdAdcToTmp();

#endif /* RTD_H_ */
