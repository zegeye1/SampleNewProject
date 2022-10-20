/*
 * rtd.c
 *
 *  Created on: Mar 16, 2021
 *      Author: ZAlemu
 */
#include <stdint.h>
#include "rtd.h"
#include "adc.h"
#include "main.h"
#include "config.h"
#include "thermalcontrol.h"

float gfRtdTempAvg = -1;    // since this is a float, float '0' not eq to Int '0'

void initRtd()
{
    ADC_init();

    // perform a periodical multiple reads
    /*
     * adcReadFromChs() call back function will be called periodically
     *  to handle enabled channels one at a time.
     *
     * a structure data type for each channel to be read/enabled need to
     *  be present within the table gastAdcChServiceTbl[].
     * the total number of channels enabled need be also set within the
     *  #define ADC_NUM_OF_CHS_ENABLED
     * gastAdcChServiceTbl[ADC_NUM_OF_CHS_ENABLED] =
     *   {&stAdcChA4, &stAdcChA5, &stAdcChA8, &stAdcChA9, &stAdcChA10, &stAdcChA11};
     */
    ADC_cfgChannelForAdc(ADC_CHA4);   // p1.4
    ADC_cfgChannelForAdc(ADC_CHA5);   // p1.5
//    ADC_cfgChannelForAdc(ADC_CHA8);   // p5.0
//    ADC_cfgChannelForAdc(ADC_CHA9);   // p5.1
//    ADC_cfgChannelForAdc(ADC_CHA10);  // p5.2
//    ADC_cfgChannelForAdc(ADC_CHA11);  // p5.3
    ADC_enableAdcmem0Int(ADC_MEM0_INT_ENABLE);

    // enable sw watchdog timer; will trigger periodically by setting ADCCT0:ADCSC
    registerTimer(&stAdcAcquistionTmr);
    enableDisableTimer(&stAdcAcquistionTmr, TMR_ENABLE);
}


/*
 *      +---3.3V
 *      |
 *      \
 *      / 1000 Ohms
 *      \
 *      /
 *      |
 *      +------------[ADC]
 *      |
 *      \
 *      /  PT1000 RTC
 *      \
 *      /
 *      |
 *    -----
 *     ---
 *      -
 *
 *    Rmeasured = Rreference * [1 + @*(Tmeasured - Treference)]
 *      where @ is temperature coefficient
 *    =>
 *     Tmeasured - Treference = (1/@) * [(Rmeasured/Rrerference) - 1]
 *
 *    Rreference = 1000 Ohms
 *    @ = 0.00385
 *    Treference = 0oC
 *
 *    =>
 *     Tmeasured = (1/@) * [(Rmeasured/1000) - 1]
 *               = 259.75 * [(Rmeasured/1000) - 1]
 *
 *    In order to calculate Rmeasured; need to transform the read ADC value.
 *    Read ADC value is a Voltage value.
 *    => VoltageMeasured = (3.3V/2^12 AdcCnt)*(ReadAdcValue)
 *
 *    Get the current going through the top 1000K reference resistor
 *     using Ohm's law
 *    => Current_1K = (3.3V - VoltageMeasured)/1000;
 *
 *    Since RTC 1000 is in series, the same current will flow through it.
 *    so, using ohm's law again, we can figure out RTC Resistance as follows:
 *    => Rmeasured = VoltageMeasured/Current_1K
 *
 *    e.g.
 *     ADC Value = 4095
 *     => MeasuredVoltage = (3.3/4096)*4095 ~= 3.3V
 *     => Current_1k      = (3.3V - VoltageMeasured)/1000
 *                        = 0
 *
 *     Rmeasured = VoltageMeasured/Current_1K
 *                =    3.3V / 0 = INFINITE
 *
 *     Possible Solution is to Increase Reference Resistance by magnitude
 *      of FOUR or so.
 *
 */
void transformRtdAdcToTmp()
{
    static uint8_t ubyTrackNumSnsrs = 0;
    float fTempAvg;

    float fRtdVolt;
    float fRtdCurrent;
    float fRtdOhms;
    float fXformVal;

    gstMainEvts.bits.svcRtdAdc = false;
    if(pgstAdcChActive->ubyChNum == ADC_ON_CHIP_TMP_SNSR)
    {
        transformOnChipAdcToTmp();
    }
    else
    {
        // read value is rtd resistance in adc count: change it to Voltage
        fRtdVolt    = RTD_VOLTAGE_LSB * pgstAdcChActive->u16AdcChVal;
        // compute current flowing through the reference resistor (ohms law)
        fRtdCurrent = (RTD_VOLTAGE_LEVEL - fRtdVolt) / RTD_REF_RESISTOR_OHMS;
        // compute updated/current resistance
        fRtdOhms    = fRtdVolt/fRtdCurrent;
        fXformVal   = RTD_ONE_OVER_ALPHA * ((fRtdOhms/RTD_REF_RESISTOR_OHMS) - 1);

        // do not update temperature data if pwm is under test
        // be careful here. if the first sensor is the internal sensor, want to skip test
        // => valid gubyPwmInTest values will be [1 - NUM_FANS]
        if(!(gbEnableTempCycleTest && (pgstAdcChActive->ubyFanIndex ==  gubyPwmInTest-1)))
        {
            /*
             * if transformed temperature data is NOT within defined range or damaged
             *  unrealistic temperature value will be computed.
             * mitigate this by replacing data with internal temperature reading.
             */
            if((fXformVal > gbyTmpRangeMax) || (fXformVal < gbyTmpRangeMin))
            {
                pgstAdcChActive->fAdcXformVal   = stAdcChA12.fAdcXformVal;
    //                    pgstAdcChActive->fAdcXformVal   = pgstAdcChActive->ubyPwmNum;   // for debug push pwm #
                pgstAdcChActive->bSelfTemp                         = false; // indicate that this is Int Temp
            }
            else
            {
                pgstAdcChActive->fAdcXformVal   = fXformVal;
    //                    pgstAdcChActive->fAdcXformVal   = pgstAdcChActive->ubyChNum;    // for debug push ch #
                pgstAdcChActive->bSelfTemp                         = true;  // indicate that this is measured temp
            }
        }
    }

    // calculate average after a full channels read completion; Average includes internal + External temp measurements
    if (++ubyTrackNumSnsrs == ADC_NUM_OF_CHS_ENABLED)
    {
        fTempAvg = 0;
        // use the track indicator for indexing while in the for loop
        for (ubyTrackNumSnsrs=0; ubyTrackNumSnsrs<ADC_NUM_OF_CHS_ENABLED; ubyTrackNumSnsrs++)
        {
            fTempAvg += gastAdcChServiceTbl[ubyTrackNumSnsrs]->fAdcXformVal;
        }

        gfRtdTempAvg     = fTempAvg/ADC_NUM_OF_CHS_ENABLED;
        ubyTrackNumSnsrs = 0;   // reset track indicator
    }

    if(pgstAdcChActive->ubyChNum != ADC_ON_CHIP_TMP_SNSR)
    {
        processThermalControl();
    }
}
