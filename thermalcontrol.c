/*
 * thermalcontrol.c
 *
 *  Created on: Jul 9, 2021
 *      Author: ZAlemu
 */

#include <stdbool.h>
#include <string.h>
#include "config.h"
#include "main.h"
#include "thermalcontrol.h"
#include "fans.h"
#include "timer.h"
#include "adc.h"
#include "rtd.h"

/*
 * Since DEMEC7040SYS-02 only has cooling capability ONLY, no heat,
 *  it is fair to assume that environmental temperature is > 0.
 *
 * For statement of work consult ESR: 04.326.035.
 *
 */
// ---------------------- vars in flash
#pragma PERSISTENT(persistentMemoryInitialized)
    uint16_t persistentMemoryInitialized = 0;

#pragma PERSISTENT (fTz)
    float fTz [NUM_TZONES][2] =
                                {-99.0, 15.0,
                                  15.0, 25.0,
                                  25.0, 30.0,
                                  30.0, 35.0,
                                  35.0, 40.0,
                                  40.0, 45.0,
                                  45.0, 50.0,
                                  50.0, 999.9};

#pragma PERSISTENT(fFanPwm)
     uint8_t fFanPwm[NUM_FANS][NUM_TZONES] =
                                 {20, 30, 35, 45, 50, 55, 60, 100,  // CPU fan
                                  20, 30, 35, 45, 50, 55, 60, 100}; // GPU fan

#pragma PERSISTENT(ubyTempHysteresis)
     uint8_t ubyTempHysteresis = FAN_HYSTERISIS_TEMP;

#pragma PERSISTENT(gbIsHtrOn)
     bool gbIsHtrOn = true;

#pragma PERSISTENT(gbyHtrOnSetPt)
     int8_t gbyHtrOnSetPt = HEATER_ON_THRESHOLD;

// ---------------------- end of vars in flash

bool   gbEnableTempCycleTest = false;
bool   bTempSetAsscending    = true;
int8_t gbyTestTemperatureVal;          // this is for debugging PWM Cycling
static uint8_t ubyHeaterNum  = 0;

// ---------------------- Heater On Timer
/*
 * this timer is used to stagger or sequence heater turning on action.
 * idea is to minimize in rush current.
 */
stTimerStruct_t stHeaterOntTmr =
{
    .prevTimer        = NULL,
    .timeoutTickCnt   = HEATER_ON_INTERVAL_MS,
    .counter          = 0,
    .recurrence       = TIMER_RECURRING,
    .status           = TIMER_DISABLED,
    .callback         = htrOnCb,
    .nextTimer        = NULL
};


bool    bAllHeatersOn = false;
bool    bHeaterTmrOnStatus = false;
bool    bIsThermalControlled;
// gubyCurrentTz[Fan0-TZ, Fan1-Tz, Fan2-Tz, Fan3-Tz, Fan4-Tz, Fan5-Tz]
uint8_t gubyCurrentTz[NUM_FANS];
//uint8_t ubyPreviousTz[NUM_FANS];
uint8_t gubyPwmInTest;           // used for selecting PWM under test when testing


void initThermalControl()
{
    bIsThermalControlled = false;

    // heater is not used by DEMEC7040SYS so need of configuring the port
    // however, the voltage divider used for simulation requires port 2.1
    P2DIR |= BIT1;

    if (persistentMemoryInitialized != 0xBEEF)
    {
        persistentMemoryInitialized = 0xBEEF;
        // temperature zones allocation
        float fTzInit [NUM_TZONES][2] =
                                    {-99.0, 15.0,
                                      15.0, 25.0,
                                      25.0, 30.0,
                                      30.0, 35.0,
                                      35.0, 40.0,
                                      40.0, 45.0,
                                      45.0, 50.0,
                                      50.0, 999.9};
        /*
         * ROW 0 <=> Fan5(PWM5) <=> ACH5 <=> CPU (Tandem two fans): PWM5 - CCR1, TACH5 - P4.0
         * ROW 1 <=> Fan4(PWM4) <=> ACH4 <=> GPU (single fan)     : PWM4 - CCR2, TACH4 - P4.1
         */
        // fan pwm allocations
        uint8_t fFanPwmInit[NUM_FANS][NUM_TZONES] =
                                    {20, 30, 35, 45, 50, 55, 60, 100,  // CPU fan
                                     20, 30, 35, 45, 50, 55, 60, 100}; // GPU fan

        ubyTempHysteresis = FAN_HYSTERISIS_TEMP;
        gbyTmpRangeMax    = MAX_TMP_VALUE_EXPECTED;
        gbyTmpRangeMin    = MIN_TMP_VALUE_EXPECTED;
        memcpy (&fFanPwm, &fFanPwmInit, sizeof(fFanPwm));
        memcpy (&fTz,     &fTzInit,     sizeof(fTz));
    }
}


void startThermalControl()
{
    findTz();
    setPwmFromTz();

    bIsThermalControlled = true;
}


void processThermalControl()
{
    /*
     * gbEnableTempCycleTest is controlled from command line interface.
     * setting this flag to 'true' will cycle through temperatures
     *  between -40 and 90 affecting fan, power control, atx enable
     *  control, ...
     */
    if(gbEnableTempCycleTest)
    {
        forceTempVal();
    }


    if(bIsThermalControlled)
    {
        updateTz();
        __no_operation();
    }
    else
    {
        startThermalControl();
    }
}


void forceTempVal()
{
    static bool bAssendOnce = false;
    static bool bDesendOnce = false;
    static uint8_t ubyDelayCnt;


    /*
     * cycle temp values from MIN_TEST_TEMPERATURE to MIN_TEST_TEMPERATURE
     *  used for debugging/testing
     */

    // since this is a post increment, it will be true every other cycle
    if(ubyDelayCnt++)       // delay force temperature update
    {
        if (bTempSetAsscending)
        {
            gbyTestTemperatureVal++;
        }
        else
        {
            gbyTestTemperatureVal--;
        }
        ubyDelayCnt = 0;
    }


    // clip temperature when outside range
    if(gbyTestTemperatureVal > MAX_TEST_TEMPERATURE)
    {
        gbyTestTemperatureVal = MAX_TEST_TEMPERATURE;
    }
    else if(gbyTestTemperatureVal < MIN_TEST_TEMPERATURE)
    {
        gbyTestTemperatureVal = MIN_TEST_TEMPERATURE;
    }

    gfRtdTempAvg                                     = gbyTestTemperatureVal;
    gastAdcChServiceTbl[gubyPwmInTest]->fAdcXformVal = gbyTestTemperatureVal;

    if((gbyTestTemperatureVal >= MAX_TEST_TEMPERATURE) && (bTempSetAsscending == true))      // increment temp until reaching Max temp
    {
        bTempSetAsscending = false;     // reverse direction
        bAssendOnce        = true;
    }
    // caution, bDescendOnce will be set to true at the start of test if '<=' comparison is used
    else if((gbyTestTemperatureVal  <= MIN_TEST_TEMPERATURE) && (bTempSetAsscending == false)) // decrement temp until reaching Min temp
    {
        bTempSetAsscending = true;      // reverse direction; start counting up
        bDesendOnce        = true;
    }

    if (bAssendOnce && bDesendOnce)     // check if need to test next PWM
    {
        gubyPwmInTest++;
        bAssendOnce = false;
        bDesendOnce = false;
        if (gubyPwmInTest > NUM_FANS)
        {
            /*
             * for development, run test continuously.
             * when release, in case used for production testing, halt after 1 round of test.
             * comment/UNcomment #define ONE_ROUND_TEST to enable/DISable one time test respectively
             * for a release version, UNcomment the #define below
             */
#define ONE_ROUND_TEST  // for release firmware this #define should be UNcommented
#ifdef  ONE_ROUND_TEST
            gbEnableTempCycleTest = false;
            gubyPwmInTest = 1;          // re-init if test is re-invoked
#else
            gubyPwmInTest = 1;
#endif
        }
    }

    __no_operation();
}


void findTz()
{
    unsigned char ubyFanIndex;
    unsigned char ubyZoneIndex;

    // Fan Index pertains only to external fans
    // if internal temperature sensor is read first, take care of the fan indexing
    // every where gastAdcChServiceTbl is referenced, increment fan index value
    for(ubyFanIndex=0; ubyFanIndex<NUM_FANS; ubyFanIndex++)
    {
        for(ubyZoneIndex=0; ubyZoneIndex<NUM_TZONES; ubyZoneIndex++)
        {
            if((gastAdcChServiceTbl[ubyFanIndex+1]->fAdcXformVal > fTz[ubyZoneIndex][0] && (gastAdcChServiceTbl[ubyFanIndex+1]->fAdcXformVal <= fTz[ubyZoneIndex][1])))
            {
                gubyCurrentTz[ubyFanIndex] = ubyZoneIndex;
                break;
            }
        }
     }
}


#if 1
unsigned char ubyFanIndex;
// this version of updateTz(), updates one fan at a time
void updateTz()
{

    ubyFanIndex = pgstAdcChActive->ubyFanIndex;

    if (pgstAdcChActive->fAdcXformVal > fTz[gubyCurrentTz[ubyFanIndex]][TZX_HIGH] + ubyTempHysteresis)
    {
        if(gubyCurrentTz[ubyFanIndex] < (NUM_TZONES-1))
        {
            gubyCurrentTz[ubyFanIndex]++;
        }
    }
    else if (pgstAdcChActive->fAdcXformVal < fTz[gubyCurrentTz[ubyFanIndex]][TZX_LOW] - ubyTempHysteresis)
    {
        if(gubyCurrentTz[ubyFanIndex])
        {
            gubyCurrentTz[ubyFanIndex]--;
        }
    }

    setSinglePwmFromTz(pgstAdcChActive->ubyPwmNum);
}

#else

// this version of updateTz(), updates all fans when for every sample
void updateTz()
{
    unsigned char ubyFanIndex;

    for(ubyFanIndex=0; ubyFanIndex<NUM_FANS; ubyFanIndex++)
    {
        if (gastAdcChServiceTbl[ubyFanIndex]->fAdcXformVal > fTz[gubyCurrentTz[ubyFanIndex]][TZX_HIGH] + ubyTempHysteresis)
        {
            if(gubyCurrentTz[ubyFanIndex] < (NUM_TZONES-1))
            {
                gubyCurrentTz[ubyFanIndex]++;
            }
        }
        else if (gastAdcChServiceTbl[ubyFanIndex]->fAdcXformVal < fTz[gubyCurrentTz[ubyFanIndex]][TZX_LOW] - ubyTempHysteresis)
        {
            if(gubyCurrentTz[ubyFanIndex])
            {
                gubyCurrentTz[ubyFanIndex]--;
            }
        }
    }

    setPwmFromTz();
}
#endif


void setPwmFromTz()
{
    unsigned char ubyFanIndx;
    unsigned char ubyCcrIndx;

    // DEMEC7040SYS uses 2 PWMs (PWM5:CCR1 and PWM4:CCR2)
    // In this case, CCR Index = Fan Index + 1
    // based on gastAdcChServiceTbl initialization; Ch5, Ch4 are index 1 and 2
    // ubyFanIndex = 0, 1 => PWM5 (CCR1 and TACH5=P4.0), PWM4 (CCR2 and TACH4=P4.1)
    for(ubyFanIndx=0; ubyFanIndx<NUM_FANS; ubyFanIndx++)
    {
        ubyCcrIndx = ubyFanIndx + 1;
        // uint8_t ubyTmrNum, uint8_t ubyCcrNum, uint8_t ubyPercent
        TMR_PwmSetPercentage(TMR_B3, ubyCcrIndx, fFanPwm[ubyFanIndx][gubyCurrentTz[ubyFanIndx]]);
    }
}


void setSinglePwmFromTz(uint8_t ubyPwmNum)
{
    uint8_t ubyCcrNum = 6 - ubyPwmNum;
    uint8_t ubyFanIndex = pgstAdcChActive->ubyFanIndex;

    //                   (TmrNum,   CcrNum,          Percent)
    TMR_PwmSetPercentage(TMR_B3, ubyCcrNum, fFanPwm[ubyFanIndex][gubyCurrentTz[ubyFanIndex]]);
}


uint16_t htrOnCb(stTimerStruct_t* myTimer)
{
    if (gbIsHtrOn)
    {
        turnHeaterOnOff(ubyHeaterNum, ON);

        if (++ubyHeaterNum > 5)
        {
             ubyHeaterNum = 0;
             gstMainEvts.bits.svcHtrTmr = true;
        }
    }
    return 0;
}


void turnHeaterOnOff(uint8_t ubyHtrNum, bool bOnOff)
{
    switch(ubyHtrNum)
    {
    case 0:
        HTR_CTRL0(bOnOff);
        break;

    case 1:
        HTR_CTRL1(bOnOff);
        break;

    case 2:
        HTR_CTRL2(bOnOff);
        break;

    case 3:
        HTR_CTRL3(bOnOff);
        break;

    case 4:
        HTR_CTRL4(bOnOff);
        break;

    case 5:
        HTR_CTRL5(bOnOff);
        if(bOnOff)
        {
            bAllHeatersOn = true;
        }
        else
        {
            bAllHeatersOn = false;
        }
        break;

    case HTR_ALL_ON_OFF:
        HTR_CTRLALL(bOnOff);
        if(bOnOff)
        {
            bAllHeatersOn = true;
        }
        else
        {
            ubyHeaterNum  = 0;      // reset heater on index
            bAllHeatersOn = false;
        }
        break;

    default:
        break;
    }
}

void disableHtrTmr()
{
    gstMainEvts.bits.svcHtrTmr = false;
    enableDisableTimer(&stHeaterOntTmr, TMR_DISABLE);
    deregisterTimer(&stHeaterOntTmr);
    bHeaterTmrOnStatus         = false;
}

void turnOnHeater()
{
    if (gbIsHtrOn && (bHeaterTmrOnStatus==false))
    {
        // enable heater turn-on timer (to insert delay in between heater on actions)
        registerTimer(&stHeaterOntTmr);
        enableDisableTimer(&stHeaterOntTmr, TMR_ENABLE);
        bHeaterTmrOnStatus = true;
    }
}

void turnOffHeater()
{
    turnHeaterOnOff(HTR_ALL_ON_OFF, OFF);
}

void updateHeater()
{
    if (gfRtdTempAvg < gbyHtrOnSetPt - ubyTempHysteresis)
    {
        if((bAllHeatersOn==false) && (bHeaterTmrOnStatus == false))
        {
            turnOnHeater();
        }
    }
    else if (gfRtdTempAvg > gbyHtrOnSetPt + ubyTempHysteresis)
    {
        if (bAllHeatersOn)
        {
            turnOffHeater();
        }
    }

}

