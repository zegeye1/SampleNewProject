/*
 * timer_test.c
 *
 *  Created on: Feb 19, 2021
 *      Author: ZAlemu
 */
#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>
#include "timer.h"

/*
 * For PWM testing/operation do the following:
 *  1. using function TMR_PwmPrdCfgForTimerBx(timer_num, period) configure
 *      the specific timer with a Period value (freq of PWM).
 *  2. configure the CCRx of the timer (x != 0), with the Output mode value
 *      and also configure the associated pin mux for timer control.
 *  3. configure duty cycle of the CCRx.
 *
 *  Note:
 *   if other CCRx for the same timer are used,
 *    steps 2 and 3.
 *
 *  All CCRx (x!=0) share the same period (CCR0 value)
 */

void timer_test()
{
    /*
     * Configure CCR1 of Timer B1 to generate PWM waveform of
     *  25000 Hz frequency, that makes use of Output Mode 7,
     *  Toggle/Reset with a 50% Duty Cycle.
     * Timer is configured in Up-Mode.
     * CCR0 holds the Period value.
     * CCR1 holds the Duty Cycle.
     */

/*
    // -----------------------------------------------------------------------
    // cfg PWM period value for all CCRs of TimerB1_3 to be 25000Hz
    TMR_PwmPrdCfgForTimerBx(TMR_B1, 25000);
    // cfg PWM output mode for TimerB1 CCR1 with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B1, TMR_CCR1, CC_CNTL_REG_OUTMOD7);
    // cfg PWM percentage
    TMR_PwmSetPercentage(TMR_B1, TMR_CCR1, 50);         // P2.0

    // NO NEED of configuring the Period; already done above
    // cfg PWM output mode for TimerB1 CCR1 with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B1, TMR_CCR2, CC_CNTL_REG_OUTMOD7);
    // cfg CCR2 pwm %
    TMR_PwmSetPercentage(TMR_B1, TMR_CCR2, 10);         // P2.1
*/

    // -----------------------------------------------------------------------
    TMR_PwmPrdCfgForTimerBx(TMR_B2, 25000);
    // cfg PWM output mode for TimerB2 CCR1 with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B2, TMR_CCR1, CC_CNTL_REG_OUTMOD7);
    // cfg PWM percentage
    TMR_PwmSetPercentage(TMR_B2, TMR_CCR1, 50);         // P5.0

    // cfg PWM output mode for TimerB2 CCR2 with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B2, TMR_CCR2, CC_CNTL_REG_OUTMOD7);
    // cfg PWM percentage
    TMR_PwmSetPercentage(TMR_B2, TMR_CCR2, 50);         // P5.1


    // -----------------------------------------------------------------------
    TMR_PwmPrdCfgForTimerBx(TMR_B3, 25000);
    // cfg PWM output mode for TimerB3 CCR1 with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR1, CC_CNTL_REG_OUTMOD7);
    // cfg PWM percentage
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR1, 50);         // P6.0

    // cfg PWM output mode for TimerB3 CCR2 with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR2, CC_CNTL_REG_OUTMOD7);
    // cfg PWM percentage
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR2, 50);         // P6.1

    // cfg PWM output mode for TimerB3 CCR3 with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR3, CC_CNTL_REG_OUTMOD7);
    // cfg PWM percentage
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR3, 50);         // P6.2

    // cfg PWM output mode for TimerB3 CCR4 with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR4, CC_CNTL_REG_OUTMOD7);
    // cfg PWM percentage
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR4, 50);         // P6.3

    // cfg PWM output mode for TimerB3 CCR5 with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR5, CC_CNTL_REG_OUTMOD7);
    // cfg PWM percentage
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR5, 50);         // P6.4

    // cfg PWM output mode for TimerB3 CCR6 with output mode 7
    TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(TMR_B3, TMR_CCR6, CC_CNTL_REG_OUTMOD7);
    // cfg PWM percentage
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR6, 50);         // P6.5

/*
    TMR_PwmSetPercentage(TMR_B1, TMR_CCR1, 75);         // P2.0
    TMR_PwmSetPercentage(TMR_B1, TMR_CCR2, 10);         // P2.1
*/
#if 0
    TMR_PwmSetPercentage(TMR_B2, TMR_CCR1, 75);         // P5.0
    TMR_PwmSetPercentage(TMR_B2, TMR_CCR2, 25);         // P5.1

    TMR_PwmSetPercentage(TMR_B3, TMR_CCR1, 75);         // P6.0
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR2, 25);         // P6.1
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR3, 90);         // P6.2
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR4, 10);         // P6.3
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR5, 99);         // P6.4
    TMR_PwmSetPercentage(TMR_B3, TMR_CCR6, 1);          // P6.5
#endif
}


