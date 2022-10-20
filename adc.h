/*
 * adc.h
 *
 *  Created on: Mar 2, 2021
 *      Author: ZAlemu
 */

#ifndef ADC_H_
#define ADC_H_

#include <msp430.h>
#include <stdint.h>
#include "timer.h"

/*
 * if sensor in use is a temperature sensor
 *  then fAdcXformVal = temperature value.
 */
typedef struct ADC_CH_DATA
{
    uint8_t  ubyChNum;
    uint16_t u16AdcChVal;       // isr places read data here
    float    fAdcXformVal;      // transformed data is stored here
    uint8_t  ubyPwmNum;         // PWM# is not CCR#. Schematics assigned
    uint8_t  ubyFanIndex;
    bool     bSelfTemp;         // 1/0 => SelfMeasured/OutOfRange = Int Temp
}stAdcSnsrData_t;

/*
 * range of temperature expected to read from an active sensor.
 * if transformed value is out of range, internal micro temperature
 *  will replace presumptuously bad data.
 */
#define MAX_TMP_VALUE_EXPECTED      (105)
#define MIN_TMP_VALUE_EXPECTED      (-50)

// ADC_moduleEnableDisable()
#define ADC_MODULE_ENABLE           (true)
#define ADC_MODULE_DISABLE          (false)

// ADC Sample and Hold Time values for ADC_cfgSampleHoldTime()
#define ADC_SHT_4ADCLK              (ADCSHT_0)
#define ADC_SHT_8ADCLK              (ADCSHT_1)
#define ADC_SHT_16ADCLK             (ADCSHT_2)
#define ADC_SHT_32ADCLK             (ADCSHT_3)
#define ADC_SHT_64ADCLK             (ADCSHT_4)
#define ADC_SHT_96ADCLK             (ADCSHT_5)
#define ADC_SHT_128ADCLK            (ADCSHT_6)
#define ADC_SHT_192ADCLK            (ADCSHT_7)
#define ADC_SHT_256ADCLK            (ADCSHT_8)
#define ADC_SHT_384ADCLK            (ADCSHT_9)
#define ADC_SHT_512ADCLK            (ADCSHT_10)
#define ADC_SHT_768ADCLK            (ADCSHT_11)
#define ADC_SHT_1024ADCLK           (ADCSHT_12)
#define ADC_SHT_1024ADCLK_13        (ADCSHT_13)
#define ADC_SHT_1024ADCLK_14        (ADCSHT_14)
#define ADC_SHT_1024ADCLK_15        (ADCSHT_15)


// ADC Enable Conversion (aka unlock/lock ADC registers)
// core is automatically disabled/unlocked when idle
// to be used as input parameter for ADC_enableDisableConversion()
#define ADC_CONVERSION_ENABLE           (true)  // lock
#define ADC_CONVERSION_DISABLE          (false) // unlock

// ADC_startStopSampleAndConversion() input params
#define ADC_START_CONVERSION            (true)
#define ADC_END_CONVERSION              (false)


// ADC channel numbers
#define ADC_CHA0                        (0)     // P1.0
#define ADC_CHA1                        (1)     // P1.1
#define ADC_CHA2                        (2)     // P1.2
#define ADC_CHA3                        (3)     // P1.3
#define ADC_CHA4                        (4)     // P1.4
#define ADC_CHA5                        (5)     // P1.5
#define ADC_CHA6                        (6)     // P1.6
#define ADC_CHA7                        (7)     // P1.7
#define ADC_CHA8                        (8)     // P5.0
#define ADC_CHA9                        (9)     // P5.1
#define ADC_CHA10                       (10)    // P5.2
#define ADC_CHA11                       (11)    // P5.3
#define ADC_CHA12                       (12)    // Internal Ch; Temp Sensor
#define ADC_ON_CHIP_TMP_SNSR            ADC_CHA12

// on-chip temperature calibration data
// see section 1.13.3.3 of user's manual, slau445i.pdf
#define ADC_30C_AT_1_5V_REF             *((unsigned int *)0x1A1A)
#define ADC_105C_AT_1_5V_REF            *((unsigned int *)0x1A1C)

/*
 * we will need to use these two formulas to generate a transformation
 *  equation for internal temperature.
 * Both of these equations are given in User's manual slau445i.pdf
 *  they are located @ sec 21.2.1 and sec 2.2.9 respectively.
 *
 * Formula 1:
 *   T = 0.00355 × (VT – V30ºC) + 30ºC
 *    where VT = Voltage at the sampled data point and
 *         V30oC = Voltage at the 30C for the ref voltage used.
 *
 *    We will need to transform the ADC count to Votlage to make
 *     use of this formula.
 *    To do this, we will use Formula 2 to find VT and V30ºC
 *
 * Formula 2:
 *   Nadc = 4096 * (Vin - Vr-)/(Vr+ - Vr-)
 *          Vr+=Vref=1.5V, Vr-=0,
 *          Vin=Voltage and Nadc=ADC count of sample point respectively
 *   Rearranging Formula 2 and substituting some of the values
 *   >>>> Vin = (1.5V/4096)*Nadc  <<<<<<<<<<<
 *
 *   For V30ºC = Nadc = content of memory @ 0x1A1A (ADC_30C_AT_1_5V_REF)
 *                    = 0x0850 = 2128d
 *   => V30ºC = (1.5/4096)*2128 = 0.7993V
 *
 *  Formula 1 now reduces to:
 *   T = 0.00355 × (VT – V30ºC)  + 30ºC
 *     = 0.00355 * (VT - 0.7993) + 30
 *     = 0.00355 * ([(1.5*Nadc)/4096] - 0.7993)) + 30
 *
 */
// Nadc = 4096 * (Vin - Vr-)/(Vr+ - Vr-) - formula @ slau445i.pdf sec 21.2.1
// Vr+ = Vref = 1.5V, Vr- = 0
// Nadc = 4096 * Vin/1.5
// Vin = 1.5/4096 * Nadc
// Nadc @ 30C is the content of memory @ 0x1A1A (ADC_30C_AT_1_5V_REF)
//            = 0x0850 = 2128d

#define TMP_CALC_CONST1                 (0.00355)
#define V1_5_AT_30C_REF                 (0.7993)

#define CALADC_15V_30C  *((unsigned int *)0x1A1A)                 // Temperature Sensor Calibration-30 C
#define CALADC_15V_85C  *((unsigned int *)0x1A1C)// Temperature Sensor Calibration-High Temperature (85 for Industrial, 105 for Extended)


// channel conversion type
#define ADC_CH_CONV_SINGLE              (0)
#define ADC_CH_CONV_MULTIPLE            (1)
#define ADC_CH_CONV_RPT_SINGLE          (2)
#define ADC_CH_CONV_RPT_MULTIPLE        (3)

// clock source for ADC
#define ADC_CLK_SRC_MODCLK              (0)
#define ADC_CLK_SRC_ACLK                (1)
#define ADC_CLK_SRC_SMCLK               (2)

// input data for ADC_samplingPulseGenBy()
#define ADC_SAMPLE_START_PULSE_FROM_CH  (0)
#define ADC_SAMPLE_START_PULSE_FROM_TMR (ADCSHP)

// ADC_samplingCfiguredToStartdBy() input parameters
#define ADC_ADCST_STRT_ACQ              (ADCSHS_0)  // fw/user controlled start
#define ADC_TMR_TRIGG0_STRT_ACQ         (ADCSHS_1)
#define ADC_TMR_TRIGG1_STRT_ACQ         (ADCSHS_2)
#define ADC_TMR_TRIGG2_STRT_ACQ         (ADCSHS_3)

// ADC_resolutionSetting() input parameters
#define ADC_SAMPLE_RESOLUTION_8BITS     (ADCRES_0)
#define ADC_SAMPLE_RESOLUTION_10BITS    (ADCRES_1)
#define ADC_SAMPLE_RESOLUTION_12BITS    (ADCRES_2)

// ADC_preDivider() input parameters
#define ADC_PRE_DIVIDER_1               (ADCPDIV_0)
#define ADC_PRE_DIVIDER_4               (ADCPDIV_1)
#define ADC_PRE_DIVIDER_64              (ADCPDIV_2)

// ADC_refVoltagesSelect() input parameters
#define ADC_REF_AVCC_AVSS               (ADCSREF_0)
#define ADC_REF_VREF_AVSS               (ADCSREF_1)
#define ADC_REF_VEREF_P_BUFF_AVSS       (ADCSREF_2)
#define ADC_REF_VEREF_P_AVSS            (ADCSREF_3)
#define ADC_REF_AVCC_VEREF_M            (ADCSREF_4)
#define ADC_REF_VREF_VEREF_M            (ADCSREF_5)
#define ADC_REF_VEREF_P_BUFF_VEREF_M    (ADCSREF_6)
#define ADC_REF_VEREF_P_VEREF_M         (ADCSREF_7)

// ADCMEM0 Int
#define ADC_MEM0_INT_ENABLE             (true)
#define ADC_MEM0_INT_DISABLE            (false)

extern int8_t    gbyTmpRangeMax;
extern int8_t    gbyTmpRangeMin;
extern uint16_t  gu16AdcSample;
extern uint16_t  gau16AdcTempVal[];    // holds ADCMEM0 val for each snsr
extern uint16_t* gpu16AdcTempVal;      // point to one of temp snsr data
extern uint16_t  gau16TempVal[];
extern stTimerStruct_t stAdcAcquistionTmr;
extern stAdcSnsrData_t* gastAdcChServiceTbl[];
extern stAdcSnsrData_t* pgstAdcChActive;
extern stAdcSnsrData_t gstAdcChAx;

extern stAdcSnsrData_t stAdcChA4;
extern stAdcSnsrData_t stAdcChA5;
extern stAdcSnsrData_t stAdcChA8;
extern stAdcSnsrData_t stAdcChA9;
extern stAdcSnsrData_t stAdcChA10;
extern stAdcSnsrData_t stAdcChA11;
extern stAdcSnsrData_t stAdcChA12;      // on-chip temp sensor; internal

void ADC_moduleEnableDisable(bool bEnableDisable);
void ADC_cfgSampleHoldTime(uint16_t ui16Sht);
void ADC_enableDisableConversion(bool bEnableAdc);
void ADC_startStopSampleAndConversion(bool bConversion);
void ADC_cfgPort4AdcUse(uint8_t ubyAdcChNum);
bool ADC_isBusy();
void ADC_setConversionMode(uint8_t ubyCmode);
void ADC_clockSelect(uint8_t ubyAdcClk);
void ADC_clkAdcDiv(uint8_t ubyClkDiv);
void ADC_samplingPulseGenBy(uint16_t ui16Pstarter);
void ADC_samplingCfiguredToStartdBy(uint16_t ui16Sstarter);
void ADC_resolutionSetting(uint8_t ubyResolution);
void ADC_preDivider(uint16_t ui16Divider);
void ADC_refVoltagesSelect(uint8_t ubyRefSelect);
void ADC_inputChSelect(uint8_t ubyChSelect);
void ADC_init();
void ADC_cfgChannelForAdc(uint8_t ubyAdcCh);
void ADC_enableAdcmem0Int(bool bInt);
void ADC_pinMuxVerefP();
void turnOnOnChipTmpSnsr();

uint16_t readSingleAdcChInt(uint8_t ubyChnNum);
uint16_t readSingleAdcChPolling(uint8_t ubyChnNum);
void readSingleAdcChIntUsingStructureInput();
uint16_t adcReadFromChsCb(stTimerStruct_t* myTimer);
void transformOnChipAdcToTmp();
void adc_test();
void deInitAdc();

#endif /* ADC_H_ */
