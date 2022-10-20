/*
 * adc.c
 *
 *  Created on: Mar 2, 2021
 *      Author: ZAlemu
 */

#include <msp430.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "config.h"
#include "adc.h"
#include "main.h"

#pragma PERSISTENT(gbyTmpRangeMax)
    int8_t  gbyTmpRangeMax = 0;

#pragma PERSISTENT(gbyTmpRangeMin)
    int8_t  gbyTmpRangeMin = 0;

uint16_t  gu16AdcSample;

/*
 * a structure data type that holds the ADC channel numbers (ubyChNum)
 *  the ADC value read by the ISR (u16AdcChVal) and the ADC to temperature
 *  transformation (fAdcXformVal) computed by back ground task for
 *  each channel is defined. Compiler will throw away the channels that
 *  are not exercised.
 *
 */
stAdcSnsrData_t stAdcChA4 =
{
     .ubyChNum     = ADC_CHA4,
     .u16AdcChVal  = NULL,
     .fAdcXformVal = NULL,
     .ubyPwmNum    = 4,             // RTD4 controls fan driven by CCR2
     .ubyFanIndex  = 1,
     .bSelfTemp    = false
};

stAdcSnsrData_t stAdcChA5 =
{
     .ubyChNum     = ADC_CHA5,
     .u16AdcChVal  = NULL,
     .fAdcXformVal = NULL,
     .ubyPwmNum    = 5,             // RTD5 controls fan driven by CCR1
     .ubyFanIndex  = 0,
     .bSelfTemp    = false
};

stAdcSnsrData_t stAdcChA8 =
{
     .ubyChNum     = ADC_CHA8,
     .u16AdcChVal  = NULL,
     .fAdcXformVal = NULL,
     .ubyPwmNum    = 0,
     .bSelfTemp    = false
};

stAdcSnsrData_t stAdcChA9 =
{
     .ubyChNum     = ADC_CHA9,
     .u16AdcChVal  = NULL,
     .fAdcXformVal = NULL,
     .ubyPwmNum    = 1,
     .bSelfTemp    = false
};

stAdcSnsrData_t stAdcChA10 =
{
     .ubyChNum     = ADC_CHA10,
     .u16AdcChVal  = NULL,
     .fAdcXformVal = NULL,
     .ubyPwmNum    = 2,
     .bSelfTemp    = false
};

stAdcSnsrData_t stAdcChA11 =
{
     .ubyChNum     = ADC_CHA11,
     .u16AdcChVal  = NULL,
     .fAdcXformVal = NULL,
     .ubyPwmNum    = 3,
     .bSelfTemp    = false
};

stAdcSnsrData_t stAdcChA12 =
{
     .ubyChNum     = ADC_ON_CHIP_TMP_SNSR,
     .u16AdcChVal  = NULL,
     .fAdcXformVal = NULL
};


// this is for single channel read (similar to a group chs implementation)
stAdcSnsrData_t gstAdcChAx =
{
     .ubyChNum     = 0,
     .u16AdcChVal  = NULL,
     .fAdcXformVal = NULL
};

// make sure ADC_NUM_OF_CHS_ENABLED is equal to the initialization elements.
/*
 * For Hawk Strike; following is the Association
 *    CCR6 <=> PWM0 <=> ACH8  <=> TACH0 = P4.5
 *    CCR5 <=> PWM1 <=> ACH9  <=> TACH1 = P4.4
 *    CCR4 <=> PWM2 <=> ACH10 <=> TACH2 = P4.3
 *    CCR3 <=> PWM3 <=> ACH11 <=> TACH3 = P4.2
 *    CCR2 <=> PWM4 <=> ACH4  <=> TACH4 = P4.1  -> used by DEMEC740SYS
 *    CCR1 <=> PWM5 <=> ACH5  <=> TACH5 = P4.0  -> used by DEMEC740SYS
 */
/*
 * for DEMEC7040SYS-02 the following are the resources used for heating and cooling
 * # of Fans/PWMs = 2
 *   => PWM5/CCR1 w/ TACH5/P4.0 driven by RTD5-CPU & PWM4/CCR2; TACH4/P4.1 driven by RTD4-GPU
 * # of Temperature Sensors 3 => = 1 Int Sensor + 2 RTDs  =>Int Snsr, Chs 5 and 4
 */
// read internal temp first since out of range ext temp measurement will be over-ridden
stAdcSnsrData_t* gastAdcChServiceTbl[ADC_NUM_OF_CHS_ENABLED] =
                            {&stAdcChA12, &stAdcChA5, &stAdcChA4};

stAdcSnsrData_t* pgstAdcChActive;

stTimerStruct_t stAdcAcquistionTmr =
{
    .prevTimer      = NULL,
    .timeoutTickCnt = ADC_ACQ_PERIOD,
    .counter        = 0,
    .recurrence     = TIMER_RECURRING,
    .status         = TIMER_RUNNING,
    .callback       = adcReadFromChsCb,
    .nextTimer      = NULL
};

/*
 * select input clock: MDCLK
 * select sample and hold clocks; 16
 * select pulse generator; timer
 * select resolution; 12-bits
 * enable ADC
 */
void ADC_init()
{
    ADC_clockSelect(ADC_CLK_SRC_MODCLK);            // ADCCTL1 |= MODCLK
    /*
     * NOTE: When using 16 clock cycles for sample and hold time
     *       the temperature reading was off. had to increase
     *       to higher number.
     *
     * At the time of this comment writing, I had 7 channels enabled
     *  (including the internal temperature sensor).
     * Performing ADC sample every 500 ms. This implies that for 7 channels
     *  it will take 7 * 500ms which is 3500ms = 3.5 sec
     *  for a 500ms period, it will take 2 samples / sec. very, very slow,
     *  meaning that we  have all the time we need and for this reason,
     *  using the maximum sample hold time of 1024 cycles/sample.
     *
     *  Based on this, the maximum # of clock cycles per channel
     *   is 1024 clks for sample and hold and 13 clks for conversion
     *   resulting with 1037 clks/sample.
     *  approximate total amount of clock cycles required for a single
     *   sample acquisition is sample and hold cycles (1024 clks) plus
     *   conversion time (13 clks) resulting with 1037 total clks for
     *   a single acquisition. there are few more clock cycles but are
     *   ignored here.

     *  the sampling frequency is shared amongst all enabled channels, i.e.,
     *   the more channels enabled the lower sampling rate available per ch.
     *   how much of an ADC clock frequency is required for this system?
     *   MODCLK seems to give us close to the 200ksps.
     *
     *  2ch * 1037clk/ch = 2074 clocks/2samples each second
     *
     *  For a MODCLK of 5Mhz Maximum # of samples for a 1024 Sample
     *   and hold time is: => 5M/1037 = 4821samples/sec
     *
     *  that is, we can schedule a single acquisition every 1sec/4821 samples
     *   = 0.2milli-sec which is even less than our ticker which is 1ms
     *
     *  So, theoretically, we can configure acquisition to take place every 1ms
     *   but that is an overkill, especially for an RTD.
     *
     */
    ADC_cfgSampleHoldTime(ADC_SHT_1024ADCLK);                   // ADCCTL0 |= ADCSHT_8
    // based on the above, default divider is a 1. anyway
    ADC_preDivider(ADC_PRE_DIVIDER_1);
    ADC_clkAdcDiv(1);
    // choose for ADC clock pulse to start transaction when ready
    ADC_samplingPulseGenBy(ADC_SAMPLE_START_PULSE_FROM_TMR);    // ADCCTL1 |= ADCSHP_1;
    // set highest resolution; need to reduce the clk anyway
    ADC_resolutionSetting(ADC_SAMPLE_RESOLUTION_12BITS);        // ADCCTL2 |= ADCRES_2;
    // turn on on-chip temp sensor
    turnOnOnChipTmpSnsr();
    // turn on ADC
    ADC_moduleEnableDisable(ADC_MODULE_ENABLE);                 // ADCCTL0 |= ADCON;
}


/*
 * configure input channel #
 * configure port for given channel
 *
 */
void ADC_cfgChannelForAdc(uint8_t ubyAdcCh)
{
    ADC_cfgPort4AdcUse(ubyAdcCh);
    ADC_inputChSelect(ubyAdcCh);
}


/*
 * input: bool (enable/disable)
 *
 * output: none
 *
 * use case:
 *  ADC_moduleEnableDisable(ADC_MODULE_ENABLE);
 *
 * input parameters:
 *  ADC_MODULE_ENABLE or
 *  ADC_MODULE_DISABLE
 */


void ADC_moduleEnableDisable(bool bEnableDisable)
{
    if(bEnableDisable)
    {
        ADCCTL0 |= ADCON;
    }
    else
    {
        ADCCTL0 &= ~ADCON;
    }

}


/*
 * These bits define the number of ADCCLK cycles in the sampling period for the ADC.
 * input: uint8_t ubySht
 *
 * output: none
 *
 * use case:
 *  ADC_cfgSampleHoldTime(ADC_SHT_16ADCLK);
 *
 * Note:
 *   sample and hold time ADC will use ranges from 0 to 1024 clock cycles
 *   ubySht[0-15] => Sample Time in ADC Clocks[4-1024]
 */


void ADC_cfgSampleHoldTime(uint16_t ui16Sht)
{
    ADCCTL0 &= ~ADCSHT;     // clear value in field
    ADCCTL0 |= ui16Sht;
}


/* input: boolean
 *
 * output: none
 *
 * use case:
 *  ADC_enableDisableConversion(ADC_CONVERSION_ENABLE);
 *
 * Note:
 *  ADCENC when disabled unlock ADC registers to be modified.
 *  this bit/field does not start conversion. however, need to be enabled
 *   so that registers are locked prior to starting sampling and conversion
 *   process by setting ADCSC bit field.
 *
 *  after conversion, core disables ADC by clearing ADCENC field for power
 *   saving. so need to enable it back when ready to perform an acquisition.
 */
void ADC_enableDisableConversion(bool bEnableAdc)
{
    if(bEnableAdc)
    {
        ADCCTL0 |= ADCENC;
    }
    else
    {
        ADCCTL0 &= ~ADCENC;
    }

}


/*
 * ADCSC is reset automatically
 *
 * use case:
 *  ADC_startStopSampleAndConversion(ADC_START_CONVERSION);
 *
 * input parameters:
 *  ADC_START_CONVERSION or
 *  ADC_END_CONVERSION
 */
void ADC_startStopSampleAndConversion(bool bConversion)
{
    if(bConversion)
    {
        ADCCTL0 |= ADCSC;
    }
    else
    {
        ADCCTL0 &= ~ADCSC;
    }
}


/* input: ADC Channel #
 *
 * output: none
 *
 * Use case:
 *  ADC_cfgPort4AdcUse(ADC_CHA0);
 *
 * input parameters:
 *  ADC_CHA0,
 *  ADC_CHA1,
 *  ADC_CHA2,
 *   ...
 *  ADC_CHA10, and
 *  ADC_CHA11
 *
 *
 * Table 6-70. Device Descriptors (data sheet slasec4d)
 * Table 6-63. Port P1 Function (data sheet slasec4d)
 *  P1SELx = 11 for x=0,1,2,3,4,5,6,7 => ADC A0,A1,A2,A3,A4,A5,A6,A7
 *  P1DIRx = X
 * Table 6-67. Port P5 Function (data sheet slasec4d)
 *  P5SELx = 11 for x=0,1,2,3 => ADC A8,A9,A10,A11
 *  P5DIRx = X
 *
 */
void ADC_cfgPort4AdcUse(uint8_t ubyAdcChNum)
{
    // cfg port ussage for ADC Ch0 to Ch7 => Port1[7:0]
    if(ubyAdcChNum < 8)
    {
        P1SEL0 |= (1 << ubyAdcChNum);
        P1SEL1 |= (1 << ubyAdcChNum);
    }
    // cfg port usage for ADC ch8 to ch11 => Port5[3:0]
    else if(ubyAdcChNum < 12)
    {
        P5SEL0 |= (1 << (ubyAdcChNum - 8));
        P5SEL1 |= (1 << (ubyAdcChNum - 8));
    }
    else
    {
        for(;;);
    }
}


bool ADC_isBusy()
{
    return !!(ADCCTL1 & ADCBUSY);
}


/*
 * use case:
 *  ADC_setConversionMode(ADC_CH_CONV_SINGLE)
 */
void ADC_setConversionMode(uint8_t ubyCmode)
{
    ubyCmode &= 0x3;
    ADCCTL1  &= ~ADCCONSEQ;  // clear field
    ADCCTL1  |= (ubyCmode << 1);
}


/*
 * use case:
 *  ADC_clockSelect(ADC_CLK_SRC_MODCLK);
 *
 */
void ADC_clockSelect(uint8_t ubyAdcClk)
{
    ubyAdcClk &= 0x3;
    ADCCTL1   &= ~ADCSSEL;
    ADCCTL1   |= (ubyAdcClk << 3);
}


/*
 * input: divider value 1 to 8
 *
 * output: none
 *
 * use case
 *  ADC_clkAdcDiv(8);
 */
void ADC_clkAdcDiv(uint8_t ubyClkDiv)
{
    // only values 1 to 8 allowed
    if(ubyClkDiv && (ubyClkDiv < 9))
    {
        ADCCTL1 &= ~ADCDIV;
        ADCCTL1 |= (ubyClkDiv - 1) << 5;
    }
}


/*
 * input: 0 or (0x200)
 *
 * output: none
 *
 * use case
 *  ADC_samplingPulseGenBy(ADC_SAMPLE_START_PULSE_FROM_TMR);
 *
 * input parameters:
 *  ADC_SAMPLE_START_PULSE_FROM_CH
 *  ADC_SAMPLE_START_PULSE_FROM_TMR
 *
 */
void ADC_samplingPulseGenBy(uint16_t ui16Pstarter)
{
    ADCCTL1 &= ~ADCSHP;
    ADCCTL1 |= ui16Pstarter;
}


/*
 * input: a 2bit value
 *
 * output: none
 *
 * use case
 *  ADC_samplingCfiguredToStartdBy(ADC_ADCST_STRT_ACQ);
 *
 * input parameters:
 *  ADC_ADCST_STRT_ACQ,
 *  ADC_TMR_TRIGG0_STRT_ACQ,
 *  ADC_TMR_TRIGG1_STRT_ACQ or
 *  ADC_TMR_TRIGG2_STRT_ACQ
 */
void ADC_samplingCfiguredToStartdBy(uint16_t ui16Sstarter)
{
    ADCCTL1 &= ~ADCSHS;
    ADCCTL1 |= ui16Sstarter;
}


/*
 * input:  a 2bit value
 *
 * output: none
 *
 * use case
 *  ADC_resolutionSetting(ADC_SAMPLE_RESOLUTION_12BITS);
 *
 * input parameters:
 *  ADC_SAMPLE_RESOLUTION_8BITS
 *  ADC_SAMPLE_RESOLUTION_10BITS
 *  ADC_SAMPLE_RESOLUTION_12BITS
 *
 */
void ADC_resolutionSetting(uint8_t ubyResolution)
{
    ADCCTL2 &= ~ADCRES;
    ADCCTL2 |= ubyResolution;
}


/*
 * input:  a 2bit value
 *
 * output: none
 *
 * use case
 *  ADC_preDivider(ADC_PRE_DIVIDER_1)
 *
 * input parameters:
 *  ADC_PRE_DIVIDER_1,
 *  ADC_PRE_DIVIDER_4, or
 *  ADC_PRE_DIVIDER_64
 *
 */
void ADC_preDivider(uint16_t ui16Divider)
{
    ADCCTL2 &= ~ADCPDIV;
    ADCCTL2 |= ui16Divider;
}


/*
 * input: Ref Voltages
 *
 * outupt: none
 *
 * use case:
 *  ADC_refVoltagesSelect(ADC_REF_AVCC_AVSS);
 *
 * input parameters:
 *  ADC_REF_AVCC_AVSS,
 *  ADC_REF_VREF_AVSS,
 *  ADC_REF_VEREF_P_BUFF_AVSS,
 *  ADC_REF_VEREF_P_AVSS,
 *  ADC_REF_AVCC_VEREF_M,
 *  ADC_REF_VREF_VEREF_M,
 *  ADC_REF_VEREF_P_BUFF_VEREF_M, or
 *  ADC_REF_VEREF_P_VEREF_M,
 *
 */
void ADC_refVoltagesSelect(uint8_t ubyRefSelect)
{
    ADCMCTL0 &= ~ADCSREF;       // clear field
    ADCMCTL0 |= ubyRefSelect;
}

/*
 * input: channel # (0 to 11)
 *
 * output: none
 *
 * use case:
 *  ADC_inputChSelect(ADC_CHA0);
 *
 * input parameters:
 *  ADC_CHA0,
 *  ADC_CHA1,
 *  ADC_CHA2,
 *   ...
 *  ADC_CHA10, and
 *  ADC_CHA11
 *
 */
void ADC_inputChSelect(uint8_t ubyChSelect)
{
    ubyChSelect &= ADCINCH;     // make sure input data is in range
    ADCMCTL0    &= ~ADCINCH;
    ADCMCTL0    |= ubyChSelect;
}


void ADC_enableAdcmem0Int(bool bInt)
{
    if(bInt)
    {
        ADCIE |= ADCIE0;
    }
    else
    {
        ADCIE &= ~ADCIE0;
    }

}

uint16_t readSingleAdcChPolling(uint8_t ubyChnNum)
{
    // disable conversion unit to unlock and allow conversion related
    //  parameter like updating input channel
    ADC_enableDisableConversion(ADC_CONVERSION_DISABLE);     // ADCCTL0:ADCENC;
    ADC_cfgChannelForAdc(ubyChnNum);
    // configure ADC the channel to sample
    ADC_inputChSelect(ubyChnNum);
    // enable conversion; lock conversion parameters
    ADC_enableDisableConversion(ADC_CONVERSION_ENABLE);     // ADCCTL0 |= ADCENC;
    // signal ADC to start sampling
    ADC_startStopSampleAndConversion(ADC_START_CONVERSION);

    // wait until conversion is done
    while(!(ADCIFG & ADCIFG0));
    return (gu16AdcSample = ADCMEM0);
}


uint16_t readSingleAdcChInt(uint8_t ubyChnNum)
{
    gu16AdcSample = 0xFFFF;             // adc-isr writes sample data to this var

    // enable interrupt
    ADC_enableAdcmem0Int(ADC_MEM0_INT_ENABLE);
    // disable conversion unit to unlock and allow conversion related
    //  parameter like updating input channel
    ADC_enableDisableConversion(ADC_CONVERSION_DISABLE);     // ADCCTL0:ADCENC;
    ADC_cfgChannelForAdc(ubyChnNum);
    // configure ADC the channel to sample
    ADC_inputChSelect(ubyChnNum);
    // enable conversion; lock conversion parameters
    ADC_enableDisableConversion(ADC_CONVERSION_ENABLE);     // ADCCTL0 |= ADCENC;
    __enable_interrupt();
    // signal ADC to start sampling
    ADC_startStopSampleAndConversion(ADC_START_CONVERSION);
    while(gu16AdcSample == 0xFFFF);
    return gu16AdcSample;
}

void ADC_pinMuxVerefP()
{
    // P1.0 mux'd with VEREF+
    P1SEL0  |=  BIT0;
    P1SEL1  |=  BIT0;
}

// this method will populate the structure with read value
void readSingleAdcChIntUsingStructureInput()
{
    // enable interrupt
    ADC_enableAdcmem0Int(ADC_MEM0_INT_ENABLE);
    // disable conversion unit to unlock and allow conversion related
    //  parameter like updating input channel
    ADC_enableDisableConversion(ADC_CONVERSION_DISABLE);     // ADCCTL0:ADCENC;
    // configure device port for analog ch usage
    ADC_cfgChannelForAdc(gstAdcChAx.ubyChNum);
    // get pointer for next adc channel parameter from the table
    pgstAdcChActive = &gstAdcChAx;
   // configure ADC the channel to sample
    ADC_inputChSelect(gstAdcChAx.ubyChNum);
    // enable conversion; lock conversion parameters
    ADC_enableDisableConversion(ADC_CONVERSION_ENABLE);     // ADCCTL0 |= ADCENC;
    __enable_interrupt();
    // signal ADC to start sampling
    ADC_startStopSampleAndConversion(ADC_START_CONVERSION);
}


uint16_t getAdcReadSample()
{
    return gu16AdcSample;
}


uint16_t adcReadFromChsCb(stTimerStruct_t* myTimer)
{
    static uint8_t  ubyIndex=0; // index used to access ADC Channels

    if (ubyIndex >= ADC_NUM_OF_CHS_ENABLED)    // index circular fashion
    {
        ubyIndex = 0;
    }

    // disable conversion unit to unlock and allow conversion related
    //  parameter like updating input channel
    ADC_enableDisableConversion(ADC_CONVERSION_DISABLE);     // ADCCTL0:ADCENC;

    // get pointer for next adc channel parameter from the table
    pgstAdcChActive = gastAdcChServiceTbl[ubyIndex++];

    /*
     * On-chip temp sensor is required to use one of the available Internal
     *  ref Voltages: 1.5V, 2.0V, or 2.5V. Default is 1.5V.
     *  if need to use 2.0V or 2.5V; need to modify PMMCTL2[REVSEL] field.
     * For extenal sensors, we are using input voltages
     */
    //
    if(pgstAdcChActive->ubyChNum == ADC_ON_CHIP_TMP_SNSR)
    {
        ADC_refVoltagesSelect(ADC_REF_VREF_AVSS);
    }
    else
    {
        ADC_refVoltagesSelect(ADC_REF_AVCC_AVSS);
    }

    // configure ADC the channel to sample
    ADC_inputChSelect(pgstAdcChActive->ubyChNum);    // P1.5, P5.0, P5.1, P5.2, P5.3

    // enable conversion; lock conversion parameters
    ADC_enableDisableConversion(ADC_CONVERSION_ENABLE);     // ADCCTL0 |= ADCENC;

    // signal ADC to start sampling
    ADC_startStopSampleAndConversion(ADC_START_CONVERSION);

    return 0;
}


//
void transformOnChipAdcToTmp()
{
    /*  FIRST Method : Seems to work and kind of matches THIRD Method
     *  --------------
     *
     * Temperature = (ADC_row - ADC_30C_at_1.5Vref) *
     *               {  75C / (ADC_105C_at_1.5Vref - ADC_30C_at_1.5Vref } + 30C
     *
     * Transformation formula and Calibration data:
     * - See section 1.13.3.3, Temperature Sensor Calibration, of User's
     *    Manual (slau445i.pdf)
     * - See Table 6-70, Device Descriptors, of Datasheet (slasec4d.pdf)
     */
    stAdcChA12.fAdcXformVal =
            ((float)(int16_t)stAdcChA12.u16AdcChVal - ADC_30C_AT_1_5V_REF) *
                 ((105.0f - 30.0f) / (ADC_105C_AT_1_5V_REF - ADC_30C_AT_1_5V_REF)) + 30;

    _no_operation();
}


void turnOnOnChipTmpSnsr()
{
    /*
     * PMMCTL0[b15:b8|PMMPW] is PMM Password field.
     *  Always reads as 096h. Write with 0A5h to unlock the PMM CTL registers
     *  for write
     * The TSENSOREN bit in the PMMCTL2 register must be set to turn on the
     *  sensor before it is used. Internal temperature sensor uses only
     *  internal reference voltages (1.5V, 2.0V or 2.5V)
     *
     * see section 2.2.9, Temperature Sensor, on User's Manual slau445i.pdf
     */
    PMMCTL0  = PMMPW;                   // do not use OR operation
    PMMCTL2 |= TSENSOREN | INTREFEN;    // Enable Int Tmp sensor & Ref V use
}


void deInitAdc()
{
    ADC_moduleEnableDisable(ADC_MODULE_DISABLE);

    // configure port for gpio usage
    // CH[7:0]
    P1SEL0 &= ~0xFF;
    P1SEL1 &= ~0xFF;

    // CH[11:8]
    P5SEL0 &= ~0x0F;
    P5SEL1 &= ~0x0F;
}


// ADC interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC_VECTOR))) ADC_ISR (void)
#else
#error Compiler not supported!
#endif
{
    switch(__even_in_range(ADCIV,ADCIV_ADCIFG))
    {
        case ADCIV_NONE:
            break;

        case ADCIV_ADCOVIFG:
            break;

        case ADCIV_ADCTOVIFG:
            break;

        case ADCIV_ADCHIIFG:
            break;

        case ADCIV_ADCLOIFG:
            break;

        case ADCIV_ADCINIFG:
            break;

        case ADCIV_ADCIFG:
            // copy sampled data
            gu16AdcSample                 = ADCMEM0;
            // store read adc data into respective channel parameter structure
            pgstAdcChActive->u16AdcChVal = gu16AdcSample;
            gstMainEvts.bits.svcRtdAdc = true;     // transform RTD adc count to temp
            __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
            break;
        default:
            break;
    }
}

