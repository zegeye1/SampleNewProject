/*
 * main.h
 *
 *  Created on: Feb 2, 2021
 *      Author: ZAlemu
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdint.h>
#include "timer_utilities.h"



typedef union MAIN_EVENTS
{
    uint16_t wAll;

    struct
    {
        uint16_t svcTicker:1;       // bit0
        uint16_t svcDiag:1;         // bit1
        uint16_t bit2:1;
        uint16_t bit3:1;

        uint16_t svcUartRx:1;       // bit4
        uint16_t svcUartTx:1;       // bit5
        uint16_t svcTestUartTx:1;   // bit6
        uint16_t bit7:1;

        uint16_t svcI2c:1;          // bit8
        uint16_t xformI2cMsg:1;     // bit9
        uint16_t bit10:1;
        uint16_t bit11:1;

        uint16_t svcRtdAdc:1;       // bit12;
        uint16_t svcHtrTmr:1;       // bit13;
        uint16_t bit14:1;
        uint16_t bit15:1;

    }bits;

} stMainEvts_t;

// global variables
extern volatile stMainEvts_t gstMainEvts;
// global timers
extern stTimerStruct_t stDispBannerAtStrtUpTmr;

// timer callback functions (handlers)
uint16_t displayBannerCb(stTimerStruct_t* myTimer);

void main_events();

#endif /* MAIN_H_ */
