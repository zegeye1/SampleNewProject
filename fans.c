/*
 * fans.c
 *
 *  Created on: Jul 8, 2021
 *      Author: ZAlemu
 */

#include <stdint.h>
#include "config.h"
#include "timer.h"
#include "fans.h"
#include "thermalcontrol.h"

stTimerStruct_t stFanRpmComputeTmr =
{
    .prevTimer      = NULL,
    .timeoutTickCnt = FAN_RPM_CALC_COUNT_TICK,
    .counter        = 0,
    .recurrence     = TIMER_RECURRING,
    .status         = TIMER_RUNNING,
    .callback       = fanRpmComputeCb,
    .nextTimer      = NULL
};


// stFanTach[TACHforCh4, TACHforCh5] = [FAN_TACH5_P4_0, FAN_TACH4_P4_1]
stFanTach_t stFanTach[NUM_FANS];
uint8_t ubyRpmCalcSecPrd;


/*
 * configure:
 * - FAN Controller Timer (PWM)
 *   -- Configure Period     (CCR0)
 *   -- Configure Duty Cycle (CCRx, Port For timer Use, Output Mode)
 *  - Fan Tach GPIO Direction and Internal Pull Resistor
 *  - Fan Tach Interrupt Direction
 */
void initFans()
{
    // in the acquistion table, , if first sensor is the internal sensor, need to skip forcing its value.
    // => valid gubyPwmInTest values will be [1 - NUM_FANS]
    gubyPwmInTest = 1;
    gbyTestTemperatureVal = MIN_TEST_TEMPERATURE;   // init the starting temperature for testing

    /*
     * Note: must not interchange the following two functions.
     * cfgPwm..() should always follow TMR_PwmPrd...()
     */
    // configure timer for PWM operation, TMR_B3 in this case, with PWM period counter,
    //  configuring the selection of clock, mode of operation, and other config etc
    // also store computed expensive float operations for future use when updating
    //  PWM percentage
    TMR_PwmPrdCfgForTimerBx(TMR_B3, TMR_PWM_PRD_FREQ_IN_USE);

    // for each pwm in use, configure
    //  - Pin Mux and
    //  - PWM Waveform (Output Mode)
    cfgPwmDutyCyclesForFanCtrl(0);  // cfg pinmux, duty cycles(init 0) for TB3.1 (PWM5) to TB3.6 (PWM0)

    // configure ports used for Tachometers for all fans running; set all as input & with int pull-up
    cfgGpio4DirPullRes();

    // configure gpio for fan tach interrupt be generated when transition high to low
    cfgGpio4IntTachCount();

    // convert RPM calculate wait period ticks into seconds
    ubyRpmCalcSecPrd = FAN_RPM_CALC_COUNT_TICK * TMR_TICKER_PERIOD;

    registerTimer(&stFanRpmComputeTmr);
    enableDisableTimer(&stFanRpmComputeTmr, TMR_ENABLE);
}


void cfgGpioP4ForTachCount(uint8_t ubyGp4Num, uint8_t ubyIsOutput, eGpioIntPullResState_t eResistor)
{
    /*
     *    FAN Controller Board; 810405; Hawk Strike
     * P4.5 <= TACH0 (Fan controlled by PWM3 TB3.6/P6.5
     * P4.4 <= TACH1 (Fan controlled by PWM3 TB3.5/P6.4
     * P4.3 <= TACH2 (Fan controlled by PWM3 TB3.4/P6.3
     * P4.2 <= TACH3 (Fan controlled by PWM2 TB3.3/P6.2
     * P4.1 <= TACH4 (Fan controlled by PWM1 TB3.2/P6.1
     * P4.0 <= TACH5 (Fan controlled by PWM0 TB3.1/P6.0
     */

    if(ubyIsOutput)   // => Is output
    {
        P4DIR |= 1<<ubyGp4Num;
    }
    else
    {               // => Is Input
        P4DIR &= ~(1<<ubyGp4Num);

        if(eResistor == GPIO_INTERNAL_RES_DISABLED)
        {
            P4REN     &= ~(1<<ubyGp4Num);
        }
        else
        {
            P4REN     |= 1<<ubyGp4Num;
            if (eResistor == GPIO_INTERNAL_RES_PULLED_UP)
            {
                P4OUT |= 1<<ubyGp4Num;
            }
            else
            {
                P4OUT &= ~(1<<ubyGp4Num);
            }
        }
    }

    P4SEL0 &= ~(1<<ubyGp4Num);
    P4SEL1 &= ~(1<<ubyGp4Num);
}


void cfgPwmDutyCyclesForFanCtrl(uint8_t ubyDCpercentage)
{
    // cfg PWM output mode for TimerB3 CCRx with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, PWM4_CCR2, CC_CNTL_REG_OUTMOD7);
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, PWM5_CCR1, CC_CNTL_REG_OUTMOD7);

    // cfg PWM percentage
    TMR_PwmSetPercentage(TMR_B3, PWM4_CCR2, ubyDCpercentage);            // P6.1 TMR_CCR2
    TMR_PwmSetPercentage(TMR_B3, PWM5_CCR1, ubyDCpercentage);            // P6.0 TMR_CCR1
}


/*
 * Calculate RPM periodically
 * Period in ticks is converted into seconds during fan initialization fanInit()
 *  i.e.    ubyRpmCalcSecPrd = FAN_RPM_CALC_COUNT_TICK * TMR_TICKER_PERIOD;
 *
 * 1st det #of revolution per rpm calculating period
 * - knowing that it takes 2 pulses per rev then
 *   # of rev per period = # of pulses counted / 2 = u16TotalRevPerPrd
 *   u16TotalRevPerPrd = stFanTach[ubyIndx].u16TachCount / 2;
 *
 * 2nd det RPM value for one sec
 * - # of rev per period / ubyRpmCalcSecPrd
 *   u16TotalRevPerPrd/ubyRpmCalcSecPrd
 *
 * 3rd det RPM, value for 1 minute == 60 seconds
 * - multiply result from 2nd step by 60
 *   (u16TotalRevPerPrd/ubyRpmCalcSecPrd) * 60
 *
 *   for integer math, it's better to multiply 1st before dividing
 *   (u16TotalRevPerPrd * 60) / ubyRpmCalcSecPrd
 */
uint16_t fanRpmComputeCb(stTimerStruct_t* myTimer)
{
    uint8_t ubyIndx;
    uint16_t u16TotalRevPerPrd;

    for(ubyIndx=0; ubyIndx<NUM_FANS; ubyIndx++)
    {
        // det Rev per Period
        u16TotalRevPerPrd = stFanTach[ubyIndx].u16TachCount / 2;

        // transpose rev/period to rev/min = RPM
        stFanTach[ubyIndx].u16Rpm = (u16TotalRevPerPrd * 60) / ubyRpmCalcSecPrd;

        // store previous RPM value
        stFanTach[ubyIndx].u16RpmPrevious = stFanTach[ubyIndx].u16Rpm;

        // store tach count before clearing
        stFanTach[ubyIndx].u16TachCountPrevious = stFanTach[ubyIndx].u16TachCount;

        stFanTach[ubyIndx].u16TachCount = 0;
    }

    return 0;
}

void cfgGpio4DirPullRes()
{
    // ----------------------------- Config GPIO Direction for Tach Count
    // TACH4=P4.1, TACH5=P4.0
    cfgGpioP4ForTachCount(FAN_TACH4_P4_1, GPIO_DIR_INPUT, GPIO_INTERNAL_RES_PULLED_UP);
    cfgGpioP4ForTachCount(FAN_TACH5_P4_0, GPIO_DIR_INPUT, GPIO_INTERNAL_RES_PULLED_UP);
}

void cfgGpio4IntTachCount()
{
    // ----------------------------- Config GPIO Int Gen Direction & enable Int
    // TACH4=P4.1, TACH5=P4.0
    cfgGpioP4Int(GPIO_PORT4_CH1, GPIO_INT_ENABLED, GPIO_INT_GEN_BYHIGH2LOW);
    cfgGpioP4Int(GPIO_PORT4_CH0, GPIO_INT_ENABLED, GPIO_INT_GEN_BYHIGH2LOW);
}

void cfgGpioP4Int(uint8_t ubyGp4Num, uint8_t ubyIsIntEnabled, uint8_t ubyEdgeDir)
{
    if(ubyIsIntEnabled)
    {
        if(ubyEdgeDir)
        {
            P4IES |= 1 << ubyGp4Num;    // cfg
        }
        else
        {
            P4IES &= ~(1 << ubyGp4Num);
        }

        P4IE  |= 1 << ubyGp4Num;
    }
}

void deInitTachs()
{
    // tachs are implemented through gpio's configured as inputs
    // high to low transition indicates a pulse start
    // all needed is disable interrupt
    P4IE  &= ~(BIT1 | BIT0);
}


#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT4_VECTOR                 // P4IFG.0 to P4IFG.7 (P4IV) @FFCE
__interrupt void PORT4_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT4_VECTOR))) Port_4 (void)
#else
#error Compiler not supported!
#endif
{
    switch(__even_in_range(P4IV,P4IV__P4IFG7))
    {
    case P4IV__P4IFG5:
        P4IFG &= ~BIT5;
//        stFanTach[FAN_TACH0_P4_5].u16TachCount++;   // TACH0 <=> PWM0 TB3.6, P6.5, Ch8
        break;

    case P4IV__P4IFG4:
        P4IFG &= ~BIT4;
//        stFanTach[FAN_TACH1_P4_4].u16TachCount++;   // TACH1 <=> PWM1 TB3.5, P6.4, Ch9
        break;

    case P4IV__P4IFG3:
        P4IFG &= ~BIT3;
//        stFanTach[FAN_TACH2_P4_3].u16TachCount++;   // TACH2 <=> PWM2 TB3.4, P6.3, Ch10
        break;

    case P4IV__P4IFG2:
        P4IFG &= ~BIT2;
//        stFanTach[FAN_TACH3_P4_2].u16TachCount++;   // TACH3 <=> PWM3 TB3.3, P6.2, Ch11
        break;

    case P4IV__P4IFG1:
        P4IFG &= ~BIT1;
        stFanTach[FAN_TACH4_P4_1].u16TachCount++;   // TACH4 <=> PWM4 TB3.2, P6.1, Ch4
        break;

    case P4IV__P4IFG0:
        P4IFG &= ~BIT0;
        stFanTach[FAN_TACH5_P4_0].u16TachCount++;   // TACH5 <=> PWM5 TB3.1, P6.0, Ch5
        break;

    default:
        break;
    }
}
