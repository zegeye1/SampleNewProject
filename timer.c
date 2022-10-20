/*
 * timer.c
 *
 * msp430fr2355 has 4 timers (all timer B's)
 * TimerB0_3, TimerB1_3, TimerB2_3 and TimerB3_7
 *
 * No Timer A
 */

#include <msp430.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <intrinsics.h>
#include "timer_utilities.h"
#include "clocks.h"
#include "timer.h"
#include "main.h"

uint16_t gu16MilliSecCpuClkCycleCount;

TMR_GrpRegsAddress_t stTimerRegsAddress;
TMR_GrpRegsAddress_t stTickTimerRegsAddress;  // save address; init once
/*
 * due to dynamic addressing, generating specific address of a timer
 *  is costly when needing to modify PWMs. so, need to stash some timer
 *  parameters when creating the timer in use.
 */
//
stTimerXPwmParams_t  stPwmTmrsParams[4];


/*
 * this is the head timer; this is the only timer that
 *  do not require registering; enabling; and disabling
 * it is used to
 */
stTimerStruct_t sTimerQueueHead =
{
    .prevTimer      = NULL,
    .timeoutTickCnt = HEARTBEAT_TIME,
    .counter        = 0,
    .recurrence     = TIMER_RECURRING,
    .status         = TIMER_RUNNING,
    /*
     * launchboard is using port P1.0. so handler
     *  is configured to toggle P1.0.
     * insure that the correct port is replaced
     *  when using a different platform
     */
 //   .callback       = launchPadHeartBeatToggle,
    .callback       = fanControllerHeartBeatToggle,
    .nextTimer      = NULL
};


// blink LED
// port 1.0 is configured for port usage using: cfgLnchPadHrtBeatP1pin0()
uint16_t launchPadHeartBeatToggle(stTimerStruct_t* myTimer)
{
    P1OUT ^= BIT0;
    return 0;
}


// blink LED
// port 1.1 is configured for port usage using: fanControllerHeartBeatToggle()
uint16_t fanControllerHeartBeatToggle(stTimerStruct_t* myTimer)
{
    P1OUT ^= BIT1;
    return 0;
}

/*
 * ***************************************************************************
 */

/*
 * TMR_CfgTimerBxTick():    generate ticker
 * input: Timer, Ticker Period in second
 * output: none
 *
 * Assumed/Implied: SMCLK is used to clock the timer.
 *
 * Note:
 *  tickers can only be generated using timer CCR0 register. If any non-CCR0
 *   register is used, timer will not be running.
 *  if using continuous mode, CCR0 is not needed and can be used as any other
 *   CCR register.
 *
 *  For this reason, ticker clock choice is restricted/or equal to the # of timers
 *   supported, which is 4 for the MSP430FR2355.
 *
 * Use Cases: Following demonstrate how to generate a ticker for a specific
 *  timer. since we've 4 timers, all 4 use cases are shown here.
 *       TMR_CfgTimerBxTick(TMR_B0, 0.001);
 *       TMR_CfgTimerBxTick(TMR_B1, 0.001);
 *       TMR_CfgTimerBxTick(TMR_B2, 0.001);
 *       TMR_CfgTimerBxTick(TMR_B3, 0.001);
 *
 * NOTE:
 *   the selection of the ticker timer depends on the PWMs that are in use.
 *   use a timer that is NOT going to be used for any kind of PWM action.
 *   reason is because CCR0 register holds the Duty Cycle value and unless
 *    the pwm you desire to configure has the same count value as the ticker,
 *    i.e., you only have one CCR0 register per timer.
 *
 */
void TMR_CfgTimerBxTick(uint8_t ubyTmrNum, float fTickValue)
{
    uint16_t    u16TickCounter;
    stDevClks_t stClkFreq;
    float       fMclkClkPeriod;

    stClkFreq      = getClockFreq();
    fMclkClkPeriod = (1.0f/(float)stClkFreq.ui32MclkHz);

    // use this for delay ms services
    gu16MilliSecCpuClkCycleCount = (uint16_t)(0.001f/fMclkClkPeriod);

    // tick counter = MCLK_freq/TICK_freq or TICK_period/MCLK_period=1/MCLK_freq
    u16TickCounter                = (uint16_t)(fTickValue/fMclkClkPeriod );

    TMR_GetTmrRegsAddress(ubyTmrNum);
    stTickTimerRegsAddress = stTimerRegsAddress;    // save for later use

    // configure the selected Compare Register with a tick count
    *stTimerRegsAddress.pTmrCapCompReg = u16TickCounter;             // TBxCCR0
    *stTimerRegsAddress.pTmrCntrl      = (CTRL_REG_DATA16_BIT|
                                          CTRL_REG_CLK_SMCLK |
                                          CTRL_REG_ID_DIV1   |
                                          CTRL_REG_MODE_UP   |
                                          CTRL_REG_CLR_FIELDS);

    // configure Compare counter register match interrupt
    // set int field corresponding to the CCR Register the Timer Counter
    //  is comparing with
    *stTimerRegsAddress.pTmrCapCompCntl = CC_CNTL_REG_INT_ENABLE;
}


/*
 * TMR_PwmPrdCfgForTimerBx():  configures specific timer CCR0 for PWM generation
 * input: Timer, Period Freq(Hz), CCR #, Output Mode,  and % for DutyCycle
 * output: none
 *
 * Assumed/Implied: SMCLK is used to clock the timer.
 *
 * Note:
 *  tickers can only be generated using timer CCR0 register. If any non-CCR0
 *   register is used, timer will not be running.
 *  if using continuous mode, CCR0 is not needed and can be used as any other
 *   CCR register.
 *
 */

/*
 * CCR0 of the selected timer, TB0/TB1/TB2/TB3, holds the period
 *  of the PWM.
 * It will be assumed that the timer will be running in UP mode.
 *
 * WARNING:
 *  The timer to be selected for PWM generation should be the timer
 *   OTHER than the ticker timer. ticker timer can only be used
 *   if PWM period equals to ticker period.
 *
 * Use cases:
 *
 *  // cfg PWM period value for all CCRs of TimerB1_3 to be 25000Hz
 *  TMR_PwmPrdCfgForTimerBx(TMR_B1, 25000);
 *  // cfg PWM output mode for TimerB1 CCR1 with output mode 7
 *  TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B1, TMR_CCR1, CC_CNTL_REG_OUTMOD7);
 *  // cfg PWM percentage
 *  TMR_PwmSetPercentage(TMR_B1, TMR_CCR1, 50);         // P2.0
 *
 *  // NO NEED of configuring the Period; already done above
 *  // cfg PWM output mode for TimerB1 CCR1 with output mode 7
 *  TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B1, TMR_CCR2, CC_CNTL_REG_OUTMOD7);
 *  // cfg CCR2 pwm %
 *  TMR_PwmSetPercentage(TMR_B1, TMR_CCR2, 10);         // P2.1
 *
 *  TMR_PwmPrdCfgForTimerBx(TMR_B2, 25000);
 *  // cfg PWM output mode for TimerB2 CCR1 with output mode 7
 *  TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B2, TMR_CCR1, CC_CNTL_REG_OUTMOD7);
 *  // cfg PWM percentage
 *  TMR_PwmSetPercentage(TMR_B2, TMR_CCR1, 50);         // P5.0
 *
 *  // cfg PWM output mode for TimerB2 CCR2 with output mode 7
 *  TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B2, TMR_CCR2, CC_CNTL_REG_OUTMOD7);
 *  // cfg PWM percentage
 *  TMR_PwmSetPercentage(TMR_B2, TMR_CCR2, 50);         // P5.1
 *
 *  TMR_PwmPrdCfgForTimerBx(TMR_B3, 25000);
 *  // cfg PWM output mode for TimerB3 CCR1 with output mode 7
 *  TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR1, CC_CNTL_REG_OUTMOD7);
 *  // cfg PWM percentage
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR1, 50);         // P6.0
 *
 *  // cfg PWM output mode for TimerB3 CCR2 with output mode 7
 *  TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR2, CC_CNTL_REG_OUTMOD7);
 *  // cfg PWM percentage
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR2, 50);         // P6.1
 *
 *
 *  // cfg PWM output mode for TimerB3 CCR3 with output mode 7
 *  TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR3, CC_CNTL_REG_OUTMOD7);
 *  // cfg PWM percentage
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR3, 50);         // P6.2
 *
 *
 *  // cfg PWM output mode for TimerB3 CCR4 with output mode 7
 *  TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR4, CC_CNTL_REG_OUTMOD7);
 *  // cfg PWM percentage
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR4, 50);         // P6.3
 *
 *
 *  // cfg PWM output mode for TimerB3 CCR5 with output mode 7
 *  TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR5, CC_CNTL_REG_OUTMOD7);
 *  // cfg PWM percentage
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR5, 50);         // P6.4
 *
 *  // cfg PWM output mode for TimerB3 CCR6 with output mode 7
 *  TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR6, CC_CNTL_REG_OUTMOD7);
 *  // cfg PWM percentage
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR6, 50);         // P6.5
 */
void TMR_PwmPrdCfgForTimerBx(uint8_t ubyTmrNum, uint16_t ui16PwmPrdFreq)
{
    uint16_t uiPeriodCounter;
    stDevClks_t stClkFreq;
    float fTimerClkPeriod;
    float fPwmPeriod;

    // retrieve clock structure (MCLK, SMCLK, ACLK, and REFCLK)
    stClkFreq       = getClockFreq();
    fTimerClkPeriod = 1.0f/(float)stClkFreq.ui32SmclkHz;            // compute clock period
    fPwmPeriod      = 1.0f/(float)ui16PwmPrdFreq;                   // compute pwm period
    uiPeriodCounter = (uint16_t)(fPwmPeriod/fTimerClkPeriod);       // compute pwm period in clock count val
                                                                    //  = PWM_PERIOD / CLOCK_PERIOD

    // get the base address of the timer
    TMR_GetTmrRegsAddress(ubyTmrNum);

    // configure the selected Compare Register with a tick count
    *stTimerRegsAddress.pTmrCapCompReg = uiPeriodCounter;           // config CCR0  - Period
    *stTimerRegsAddress.pTmrCntrl      = (CTRL_REG_DATA16_BIT|      // TBxCTL CNTL   - Counter Len = 16bits
                                          CTRL_REG_CLK_SMCLK |      // TBxCTL TBSSEL - Clock Source = SMCLK
                                          CTRL_REG_ID_DIV1   |      // TBxCTL ID     - Divider = 1
                                          CTRL_REG_MODE_UP   |      // TBxCTL MC     - Mode = Up
                                          CTRL_REG_CLR_FIELDS);     // TBxCTL TBCLR  - Clear all Timer Regs (counter, divider, & count direction)

    // store PWM parameters for future PWM value changes; Update PWM function.
    // want to avoid heavy computation when using multiple timers/pwms. store
    //  some parameters into a structure object.
    stPwmTmrsParams[ubyTmrNum].ubyTimerNum                = ubyTmrNum;
    stPwmTmrsParams[ubyTmrNum].stPwmTimerRegsAddress      = stTimerRegsAddress;
    stPwmTmrsParams[ubyTmrNum].ui16PwmPrdCnt              = uiPeriodCounter;
    stPwmTmrsParams[ubyTmrNum].ui32InvPrdCntScaled        = (uint32_t)((1.0f/uiPeriodCounter)*10000);
}


void TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(uint8_t ubyTmrNum,
                                                uint8_t ubyCcrNum, uint16_t ui16OutMode)
{

    // since we needed to configure CCR0 with a Period value, above, the
    //  dynamic memory structure is pointing to CCR0 control and compare regs.
    // in order to point to the CCR reg address, without calling,
    //  TMR_GetTmrRegsAddress(), we can add the offset (ccr #).
    *(stPwmTmrsParams[ubyTmrNum].stPwmTimerRegsAddress.pTmrCapCompCntl+ubyCcrNum) |= ui16OutMode;

    // configure CCR# in use with the desired Duty Cycle percetage
//    TMR_PwmSetPercentage (ubyTmrNum, ubyCcrNum, ubyPercent);

    // configure pin muxing for the specific Timer and CCR register in use
    TMR_PwmPinMuxCfg(ubyTmrNum, ubyCcrNum);
}


/*
 * TMR_PwmSetPercentage():  Modify PWM Waveform Duty Cycle
 * input: Timer, CCR #, and % for DutyCycle
 * output: -1 if error (attempted to modify a pwm that's not configured)
 *          0 if good
 *
 * Prerequisite:
 *  PWM waveform should have already been created before attempting to modify
 *   using TMR_PwmPrdCfgForTimerBx(...).
 *
 *
 * TBxCCR0 is configured with Period value = 80 counts
 * TBxCCRs is configured with a Duty Cycle count <= 80
 * calculate duty cycle using integer math since processor is fixed pt
 *  and floating pt calculation is extremely costly.
 *
 * (make sure pwm is configured before calling this function)
 *   if you call without configuring, it will do nothing, returns -1
 * see timer_test()
 *
 * example of usage:
 *  TMR_PwmSetPercentage(TMR_B1, TMR_CCR1, 75); // P2.0
 *  TMR_PwmSetPercentage(TMR_B1, TMR_CCR2, 10);         // P2.1
 *
 *  TMR_PwmSetPercentage(TMR_B2, TMR_CCR1, 75);         // P5.0
 *  TMR_PwmSetPercentage(TMR_B2, TMR_CCR2, 25);         // P5.1
 *
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR1, 75);         // P6.0
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR2, 25);         // P6.1
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR3, 90);         // P6.2
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR4, 10);         // P6.3
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR5, 99);         // P6.4
 *  TMR_PwmSetPercentage(TMR_B3, TMR_CCR6, 1);          // P6.5
 *
 */
/*

 *
 * formula example:
 *
 * DC = % * Period Count
 * e.g. 50% = 0.5 * 80 = 40
 * Pb with this is 50/100, using int math will be 50/100 = 0 not .5
 *  so need to re-arrange the computation by multiplying it by 1
 *  multiply by 10,000/10,000
 *
 * DC = dc/100 * 80;
 *               80 = 1/(1/80)
 *               80 = 1/0.0125
 *                  = (1/0.0125)*(1)
 *                  = (1/0.0125)*(10000/10000)
 *               80 = 10000/125
 * Substituting this fractional value to the above formula
 * DC = %        * Period Count
 *    = (dc/100) * 80
 *    = (dc/100) * (10000/125)
 *    = (dc*10000)/(100*125)
 *    = (dc*100)/125
 * DC =  dc*100/125  <<<<==== this is the formula to use for a prd val of 80
 *
 * Summary calculating Duty Cycle of 50%
 *  floating pt math: 0.5 * Period = 0.5 * 80 = 40.
 *  fixed pt math: 0.5 * 80 = 0.
 *  fixed pt math: replace 0.5 * 80 with 50 * 100/125
 *
 * Duty Cycle(DC) pertains to the length of time waveform is ON within
 *  a single period.
 * the OUTMODE selected dictates the ON time length
 *
 *              ACTIVE LOW Control
 * Fan is active low => low time is the ON duty cycle
 * ------+      +
 *       |      |
 *       +------+
 *      /\      /\
 *      ||      ||
 *    =TBxCCRx  =TBxCCR0
 * e.g for 90% duty cycle you want 90%*80 = 72 cnt to be Low
 *     for an Active Low device need to make ON time the LOW time
 *      so subtract value from Period value
 *     so,TBxCCRx should be programmed with 80 - 72 = 8.
 *     90% of Period(80) = 72
 *     90 * 100/125 = 72
 *
 *     For Active Low configuration:
 *      Period - DC count
 *       80    -   72
 *      TBxCCRx = 8 counts
 *
 *               ACTIVE HIGH Control
 * For an active High PWM control, need to use the straight
 *  percentile conversion. For the above eg. TBxCCRx = 72.
 *
 */
int16_t TMR_PwmSetPercentage (uint8_t ubyTmrNum, uint8_t ubyCcrNum, uint8_t ubyPercent)
{
    int32_t i32DutyCycleCnt;
    uint32_t ui16PeriodMinusDutyCycle;

    uint32_t ui32PwmPrdInvScaled;

    TMR_GrpRegsAddress_t stTimerAddresses;

    // get stored inverted and scaled value when pwm was being configured.
    ui32PwmPrdInvScaled = stPwmTmrsParams[ubyTmrNum].ui32InvPrdCntScaled;

    // since this value is the reciprocal of the PWM period, scaled, it should be none 0
    if (ui32PwmPrdInvScaled == 0)
    {   // the timer referenced is not configured for pwm function
        return -1;
    }

    // get data for ease of reducing variable length
    stTimerAddresses = stPwmTmrsParams[ubyTmrNum].stPwmTimerRegsAddress;

    // compute counter value to load for the requested percentile
    i32DutyCycleCnt = (int32_t)((ubyPercent * 100) / ui32PwmPrdInvScaled);

    // due to round off, value might go a bit beyond boundaries
    if (i32DutyCycleCnt > (int32_t)stPwmTmrsParams[ubyTmrNum].ui16PwmPrdCnt)
    {
        i32DutyCycleCnt = (int32_t)stPwmTmrsParams[ubyTmrNum].ui16PwmPrdCnt;
    }

    ui16PeriodMinusDutyCycle = (uint32_t)(stPwmTmrsParams[ubyTmrNum].ui16PwmPrdCnt - i32DutyCycleCnt);


    *(stTimerAddresses.pTmrCapCompReg+ubyCcrNum) = ui16PeriodMinusDutyCycle;

    stPwmTmrsParams[ubyTmrNum].ubyPercentageDC[ubyCcrNum] = ubyPercent;

    return 0;
}


/*
 * getter for cli pwm % request
 */
int8_t TMR_PwmGetDcPercenatage(uint8_t ubyTmrNum, uint8_t ubyCcrNum)
{
    if (stPwmTmrsParams[ubyTmrNum].ui32InvPrdCntScaled == 0)
    {   // the timer referenced is not configured for pwm function
        return -1;
    }
    else
    {
        return stPwmTmrsParams[ubyTmrNum].ubyPercentageDC[ubyCcrNum];
    }
}


/*
 * TMR_PwmPinMuxCfg():  Configures multiplexed port pin for timer usage.
 * input: Timer and CCR #
 * output: none
 *
 * Note:
 *  configures pinmux for:
 *    TB0.1, TB0.2
 *    TB1.1, TB1.2
 *    TB2.1, TB2.2
 *    TB3.1, TB3.2, TB3.3, TB3.4, TB3.5, TB3.6
 */
void TMR_PwmPinMuxCfg(uint8_t ubyTmrNum, uint8_t ubyCcrNum)
{
    uint8_t ubyCcrBitField;

    // configure pin mux and
    switch(ubyTmrNum)
    {
    case TMR_B0:
        // for TB0.1 or TB1.2 cfg P1.6 or P1.7
        if ((ubyCcrNum > 0) && (ubyCcrNum < 3))
        {
            // P1DIR = x1xx_xxxx or 1xxx_xxx; P1SEL[1:0]=10b
            ubyCcrBitField = 5 + ubyCcrNum;
            P1DIR         |= (1 << ubyCcrBitField);
            P1SEL0        &= ~(1 << ubyCcrBitField);
            P1SEL1        |= (1 << ubyCcrBitField);
        }
        break;

    case TMR_B1:
        // for TB1.1 or TB1.2, cfg P2.0 or P2.1
        if ((ubyCcrNum > 0) && (ubyCcrNum < 3))
        {
            // P2DIR = xxxx_xxx1 or xxxx_xx1x; P2SEL[1:0]=01b
            ubyCcrBitField = ubyCcrNum - 1;
            P2DIR         |= (1 << ubyCcrBitField);
            P2SEL0        |= (1 << ubyCcrBitField);
            P2SEL1        &= ~(1 << ubyCcrBitField);
        }
        break;

    case TMR_B2:
        // for TB2.1 or TB2.2, cfg P5.0 or P5.1
        if ((ubyCcrNum > 0) && (ubyCcrNum < 3))
        {
            // P5DIR = xxxx_xxx1 or xxxx_xx1x; P5SELx[1:0]=01b
            ubyCcrBitField = ubyCcrNum - 1;
            P5DIR         |= (1 << ubyCcrBitField);
            P5SEL0        |= (1 << ubyCcrBitField);
            P5SEL1        &= ~(1 << ubyCcrBitField);
        }
        break;

    case TMR_B3:
        // for TB3.1, TB3.2, TB3.3, TB3.4, TB3.5 or TB3.6
        // cfg  P6.0,  P6.1,  P6.2,  P6.3,  P6.4 or  P6.5
        if ((ubyCcrNum > 0) && (ubyCcrNum < 7))
        {
            // P6DIR = xxxx_xxx1 to x1xx_xxxx or P6SELx[1:0]=01b
            ubyCcrBitField = ubyCcrNum - 1;
            P6DIR         |= (1 << ubyCcrBitField);
            P6SEL0        |= (1 << ubyCcrBitField);
            P6SEL1        &= ~(1 << ubyCcrBitField);
        }
        break;
    }
}


/*
 * Usage:
 *       TMR_SectTmrCntrLength(TMR_B0, TMR_CCR0, CTRL_REG_DATA12_BIT);
 *       TMR_SectTmrCntrLength(TMR_B1, TMR_CCR0, CTRL_REG_DATA12_BIT);
 *       TMR_SectTmrCntrLength(TMR_B2, TMR_CCR0, CTRL_REG_DATA12_BIT);
 *       TMR_SectTmrCntrLength(TMR_B3, TMR_CCR0, CTRL_REG_DATA12_BIT);
 *
 */
void TMR_SectTmrCntrLength(uint8_t ubyTmrNum, uint16_t ui16CntrlLen)
{
    TMR_GetTmrRegsAddress(ubyTmrNum);
    *stTimerRegsAddress.pTmrCntrl &= ~CNTL;
    *stTimerRegsAddress.pTmrCntrl |= ui16CntrlLen;
}


//  *    TBxCTL.TBSSEL[b9:b8] =
//  *        [00b, 01b, 10b, 11b] => [TBxCLK, ACLK, SMCLK, INCLK]
/*
 * Usage:
 *       TMR_SelectInClk(TMR_B0, TMR_CCR0, INPUT_CLK_SMCLK);
 *       TMR_SelectInClk(TMR_B1, TMR_CCR0, INPUT_CLK_SMCLK);
 *       TMR_SelectInClk(TMR_B2, TMR_CCR0, INPUT_CLK_SMCLK);
 *       TMR_SelectInClk(TMR_B3, TMR_CCR0, INPUT_CLK_SMCLK);
 *
 */
void TMR_TmrInputClk(uint8_t ubyTmrNum, uint16_t uiTmrClk)
{
    TMR_GetTmrRegsAddress(ubyTmrNum);
    *stTimerRegsAddress.pTmrCntrl &= ~TBSSEL;
    *stTimerRegsAddress.pTmrCntrl |= uiTmrClk;
}


// *    TBxCTL.MC[b5:b4] =
// *        [00b, 01b, 10b, 11b] => [STOP, UP, CONTINUOUS, UP-DOWN]
/*
 * Usage:
 *       TMR_SetOperatingMode(TMR_B0, TMR_CCR0, TMR_UP_MODE);
 *       TMR_SetOperatingMode(TMR_B1, TMR_CCR0, TMR_UP_MODE);
 *       TMR_SetOperatingMode(TMR_B2, TMR_CCR0, TMR_UP_MODE);
 *       TMR_SetOperatingMode(TMR_B3, TMR_CCR0, TMR_UP_MODE);
 *
 *
 */
void TMR_SetOperatingMode(TMR_GrpRegsAddress_t stTimerRegAddress, uint16_t uiOpMode)
{
    *stTimerRegAddress.pTmrCntrl &= ~MC;
    *stTimerRegAddress.pTmrCntrl |= uiOpMode;
}


uint16_t TMR_GetTimerOperatingMode(TMR_GrpRegsAddress_t stTimerRegAddress)
{
    return *stTimerRegAddress.pTmrCntrl & MC;
}


// *    TBxCTL.ID[b7:b6] =
// *        [00b, 01b, 10b, 11b] => [/1, /2, /4, /8]
//                               = INPUT_CLK_DIV1/2/4/8
/*
 * Usage:
 *        TMR_SetClkDivider(TMR_B0, TMR_CCR0, INPUT_CLK_DIV2);
 *        TMR_SetClkDivider(TMR_B1, TMR_CCR0, INPUT_CLK_DIV2);
 *        TMR_SetClkDivider(TMR_B2, TMR_CCR0, INPUT_CLK_DIV2);
 *        TMR_SetClkDivider(TMR_B3, TMR_CCR0, INPUT_CLK_DIV2);
 *
 */
void TMR_SetClkDivider(uint8_t ubyTmrNum, uint16_t uiInClkDivider)
{
    TMR_GetTmrRegsAddress(ubyTmrNum);
    *stTimerRegsAddress.pTmrCntrl &= ~ID;
    *stTimerRegsAddress.pTmrCntrl |= uiInClkDivider;
}


// *    TBxCTL.TBCLR[b2] = 1 => Clears Clock Divider, Counter and Count Direction
// *         Divider values remain unchanged
/*
 * Usage:
 *        TMR_ClearClkCtrDivDirection(TMR_B0, TMR_CCR0);
 *        TMR_ClearClkCtrDivDirection(TMR_B1, TMR_CCR0);
 *        TMR_ClearClkCtrDivDirection(TMR_B2, TMR_CCR0);
 *        TMR_ClearClkCtrDivDirection(TMR_B3, TMR_CCR0);
 *
 */
void TMR_ClearClkCtrDivDirection(uint8_t ubyTmrNum)
{
    TMR_GetTmrRegsAddress(ubyTmrNum);
    *stTimerRegsAddress.pTmrCntrl |= TMR_BX_CLEAR;
}


// *    TBxCTL.TBIE[b1] = 1/0 => Interrupt Enabled/Disabled
// *
/*
 * Usage:
 *       TMR_IntEnableDisable(TMR_B0, TMR_CCR0, TMR_BX_INT_ENABLE);
 *       TMR_IntEnableDisable(TMR_B1, TMR_CCR0, TMR_BX_INT_ENABLE);
 *       TMR_IntEnableDisable(TMR_B2, TMR_CCR0, TMR_BX_INT_ENABLE);
 *       TMR_IntEnableDisable(TMR_B3, TMR_CCR0, TMR_BX_INT_ENABLE);
 *
 */
void TMR_IntEnableDisable(uint8_t ubyTmrNum, bool bEnableInt)
{
    TMR_GetTmrRegsAddress(ubyTmrNum);
    if (bEnableInt)
    {
        *stTimerRegsAddress.pTmrCntrl |= TMR_BX_INT_ENABLE;
    }
    else
    {
        *stTimerRegsAddress.pTmrCntrl &= ~TMR_BX_INT_ENABLE;
    }
}


// Get Timer Register Addresses for either Control or Capture Compare ZERO
void TMR_GetTmrRegsAddress(uint8_t ubyTmrNum)
{
    switch(ubyTmrNum)
    {
        case(0):
            stTimerRegsAddress.pTmrCntrl       = &TB0CTL;
            stTimerRegsAddress.pTmrCapCompCntl = &TB0CCTL0;
            stTimerRegsAddress.pTmrCounter     = &TB0R;
            stTimerRegsAddress.pTmrCapCompReg  = &TB0CCR0;
            break;

        case(1):
            stTimerRegsAddress.pTmrCntrl       = &TB1CTL;
            stTimerRegsAddress.pTmrCapCompCntl = &TB1CCTL0;
            stTimerRegsAddress.pTmrCounter     = &TB1R;
            stTimerRegsAddress.pTmrCapCompReg  = &TB1CCR0;
            break;

        case(2):
            stTimerRegsAddress.pTmrCntrl       = &TB2CTL;
            stTimerRegsAddress.pTmrCapCompCntl = &TB2CCTL0;
            stTimerRegsAddress.pTmrCounter     = &TB2R;
            stTimerRegsAddress.pTmrCapCompReg  = &TB2CCR0;
            break;

        case(3):
            stTimerRegsAddress.pTmrCntrl       = &TB3CTL;
            stTimerRegsAddress.pTmrCapCompCntl = &TB3CCTL0;
            stTimerRegsAddress.pTmrCounter     = &TB3R;
            stTimerRegsAddress.pTmrCapCompReg  = &TB3CCR0;
            break;
    }
}


void cfgTickClkTestPort()
{
    // use SMCLK clock out p3.4 to measure tick frequency
    P3DIR  |= BIT4;
    //P3SELx=01b
    P3SEL0 &= ~BIT4;
    P3SEL1 &= ~BIT4;
}


void deInitTickTimer()
{
    // disable ticker interrupt
    *stTimerRegsAddress.pTmrCapCompCntl &= ~CC_CNTL_REG_INT_ENABLE;
    // MC_STOP = 00b and MC_UP_DWN = 11b
    // isolate MC field for clearing using Up/Down setting
    *stTimerRegsAddress.pTmrCntrl &= ~CTRL_REG_MODE_UP_DWN;
}


void deInitPwmTimerB3()
{
    // stop timer
    TB3CTL &=  ~CTRL_REG_MODE_UP_DWN;
    // configure port for gpio usage
    P6SEL0        &= ~(BIT5 | BIT4 | BIT3 | BIT2 | BIT1 | BIT0);
    P6SEL1        &= ~(BIT5 | BIT4 | BIT3 | BIT2 | BIT1 | BIT0);
}


/*
 * this function is used to minimize the code duplicate necessary
 *  to keep the same handler for all timer ISRs.
 * usually not recommended for ISR; preferred serialized very short
 *  code. In this case, since system is not very loaded, should
 *  be OK to violate the rule a bit.
 */
bool tickTmrIsrHandler()
{
    stTimerStruct_t* iter = &sTimerQueueHead;
    bool bWakeProcessor = false;

#ifdef ___DEBUG___
    /*
     * on launch board, port 3.4 was used to validate the tick
     *  frequency.
     */
    P3OUT ^= BIT4;
#endif

    while(iter != NULL)
    {
        if(iter->status == TIMER_RUNNING)
        {
            if(iter->counter >= iter->timeoutTickCnt)
            {
                iter->status = TIMER_DONE;
                gstMainEvts.bits.svcTicker = true;
//                __bic_SR_register_on_exit(LPM0_bits);
                bWakeProcessor = true;
            }
            else
            {
                iter->counter += 1;
            }
        }
        iter = iter->nextTimer;
    }
    return bWakeProcessor;
}

// Timer B0 interrupt service routine
/*
 * Two interrupt vectors are associated with the 16-bit Timer_B module:
 *  1. TBxCCR0 interrupt vector for TBxCCR0 CCIFG
 *  2. TBIV interrupt vector for all other CCIFG flags and TBIFG
 *
 * The below interrupt vector is with case 1 because interrupt is set
 *  to be generated when TB03 counter value matches TBxCCR0 value.
 *
 * Based on Interrupt Vector, Table 6.2 of datasheet
 *  INTERRUPT SOURCE: Timer0_B3 (TimerB0_3)
 *  INTERRUPT FLAG:   TB0CCR0 CCIFG0
 *  SYSTEM INTERRUPT: Maskable
 *  WORD ADDRESS:     FFF8h
 *  PRIORITY:         60
 *
 */
// Timer0_B3 interrupt service routine TB0CCR0 CCIFG0; Single Interrupt
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER0_B0_VECTOR
__interrupt void TimerB0_3_ISR1 (void)      // Timer0_B3 TB0CCR0 CCIFG0 @FFF8
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_B0_VECTOR))) TimerB0_3_ISR1 (void)
#else
#error Compiler not supported!
#endif
{
    if(tickTmrIsrHandler())
    {
        __bic_SR_register_on_exit(LPM0_bits);
    }
}


// Timer1_B3 interrupt service routine TB1CCR0 CCIFG0; Single Interrupt
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER1_B0_VECTOR
__interrupt void Timer1_B3_CCR0_ISR(void)   // Timer1_B3 TB1CCR0 CCIFG0 @FFF4
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER1_B0_VECTOR))) TimerB1_3_ISR (void)
#else
#error Compiler not supported!
#endif
{
    if(tickTmrIsrHandler())
    {
        __bic_SR_register_on_exit(LPM0_bits);
    }

#if 0
        /*
         * p3.4 is used during development to observe tick output.
         * this output may be used by an actual system.
         * ****** USE WITH CARE ******
         */
    #ifdef ___DEBUG___
        // toggle P3.4 port to validate tick period
        // one period is 2 tick count
        P3OUT ^= BIT4;
    #endif
        __bic_SR_register_on_exit(LPM0_bits);
#endif
}


// Timer2_B3 interrupt service routine TB2CCR0 CCIFG0; Single Interrupt
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER2_B0_VECTOR       // Timer2_B3 TB2CCR0 CCIFG0 @FFF0
__interrupt void Timer2_B3_CCR0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER2_B0_VECTOR))) TimerB2_3_ISR (void)
#else
#error Compiler not supported!
#endif
{
    if(tickTmrIsrHandler())
    {
        __bic_SR_register_on_exit(LPM0_bits);
    }

#if 0
        /*
         * p3.4 is used during development to observe tick output.
         * this output may be used by an actual system.
         * ****** USE WITH CARE ******
         */
    #ifdef ___DEBUG___
        // toggle P3.4 port to validate tick period
        // one period is 2 tick count
        P3OUT ^= BIT4;
    #endif
        __bic_SR_register_on_exit(LPM0_bits);
#endif
}


// Timer3_B7 interrupt service routine TB3CCR0 CCIFG0; Single Interrupt
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER3_B0_VECTOR           // Timer3_B7 TB3CCR0 CCIFG0 @FFEC
__interrupt void Timer3_B7_CCR0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER2_B1_VECTOR))) TimerB3_7_ISR (void)
#else
#error Compiler not supported!
#endif
{
    if(tickTmrIsrHandler())
    {
        __bic_SR_register_on_exit(LPM0_bits);
    }

#if 0
        /*
         * p3.4 is used during development to observe tick output.
         * this output may be used by an actual system.
         * ****** USE WITH CARE ******
         */
    #ifdef ___DEBUG___
        // toggle P3.4 port to validate tick period
        // one period is 2 tick count
        P3OUT ^= BIT4;
    #endif
        __bic_SR_register_on_exit(LPM0_bits);
#endif
}


// Second Timer B0 interrupt service routine
/*
 * Two interrupt vectors are associated with the 16-bit Timer_B module:
 *  1. TBxCCR0 interrupt vector for TBxCCR0 CCIFG
 *  2. TBIV interrupt vector for all other CCIFG flags and TBIFG
 *
 * The below interrupt vector is with case 2 because interrupt is set
 *  to be generated when TB03 counter value matches one of the
 *  CCR Registers that is NOT TBxCCR0
 *
 * Based on Interrupt Vector, Table 6.2 of datasheet
 *  INTERRUPT SOURCE: Timer0_B3 (TimerB0_3)
 *  INTERRUPT FLAG:   TB0CCR1 CCIFG1,
 *                    TB0CCR2 CCIFG2,
 *                    TB0IFG  (which is CCR0)
 *  SYSTEM INTERRUPT: Maskable
 *  WORD ADDRESS:     FFF6h
 *  PRIORITY:         59
 *
 */
// Timer0_B3 Interrupt Vector (TBIV) handler: Group Interrupt Handler
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_B1_VECTOR                 // Timer0_B3 @FFF6
__interrupt void Timer0_B3_TB3IV_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_B1_VECTOR))) TIMERB0_B3_TB0IV_ISR (void)
#else
#error Compiler not supported!
#endif
{
    switch(__even_in_range(TB0IV,TB0IV_TBIFG))
    {
        case TB0IV_NONE:
            // toggle P3.4 port to validate tick period
            // one period is 2 tick count
            P3OUT ^= BIT4;
            break;                               // No interrupt
        case TB0IV_TBCCR1:
            // toggle P3.4 port to validate tick period
            // one period is 2 tick count
            P3OUT ^= BIT4;
            break;                               // CCR1 not used
        case TB0IV_TBCCR2:
            // toggle P3.4 port to validate tick period
            // one period is 2 tick count
            P3OUT ^= BIT4;
            break;                               // CCR2 not used
        case TB0IV_TBIFG:
            // toggle P3.4 port to validate tick period
            // one period is 2 tick count
            P3OUT ^= BIT4;
             break;
        default:
            break;
    }
    __bic_SR_register_on_exit(LPM0_bits);
}


