/*
 * timer_utilities.h
 *
 *  Created on: Jan 27, 2021
 *      Author: ZAlemu
 */

#ifndef TIMER_UTILITIES_H_
#define TIMER_UTILITIES_H_
#include <stdbool.h>

#define TMR_ENABLE              (1)
#define TMR_DISABLE             (0)

typedef enum TIMER_STATUS
{
    TIMER_RUNNING,
    TIMER_DONE,
    TIMER_DISABLED,
    NUM_TIMER_STATUS,
}eTimerStatus_t;


typedef enum TIMER_RECURRENCE
{
    TIMER_RECURRING,
    TIMER_SINGLE,
    NUM_TIMER_RECURRENCE,
}eTimerRecurrence_t;


typedef struct TIMER_STRUCT
{
    struct TIMER_STRUCT* prevTimer;
    uint16_t timeoutTickCnt;
    uint16_t counter;
    eTimerRecurrence_t recurrence;
    eTimerStatus_t status;
    uint16_t (*callback)(struct TIMER_STRUCT* thisTimer);
    struct TIMER_STRUCT* nextTimer;
}stTimerStruct_t;

extern stTimerStruct_t delayTickCounterTmr;

// timer servicing functions
uint16_t serviceTimers();
uint16_t registerTimer(stTimerStruct_t* newTimer);
uint16_t enableDisableTimer(stTimerStruct_t* myTimer, bool bEnableDisable);
uint16_t deregisterTimer(stTimerStruct_t* oldTimer);

// delay ticker handler
uint16_t delayTickCountCb(stTimerStruct_t* newTimer);
bool     delayMilliSecCount(uint16_t u16TickCount);


#endif /* TIMER_UTILITIES_H_ */
