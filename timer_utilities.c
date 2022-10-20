#include <msp430.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <intrinsics.h>
#include "clocks.h"
#include "timer.h"
#include "main.h"
#include "timer_utilities.h"

bool    bTickDelayMet = false;

stTimerStruct_t delayTickCounterTmr =
{
    .prevTimer        = NULL,
    .timeoutTickCnt   = 1000,
    .counter          = 0,
    .recurrence       = TIMER_SINGLE,
    .status           = TIMER_RUNNING,
    .callback         = delayTickCountCb,
    .nextTimer        = NULL
};

/*
 * delayMilliSecCount()
 *
 * input: count value in milli-sec resolution
 *
 * output: returns 'true' when set amount exhausts

 * delayMilliSecCount roughly gives you a set amount of milli-sec delay.
 * the actual delay is more than what is requested due to the cycle counts
 *  incurred for processing the count.
 *
 * use case:
 *      while(!delayMilliSecCount(10)); // stay here for 10 milli-seconds
 */
bool delayMilliSecCount(uint16_t u16MsCount)
{
uint16_t u16CpuCntr;
    while(u16MsCount)
    {
        for(u16CpuCntr=0; u16CpuCntr<gu16MilliSecCpuClkCycleCount; u16CpuCntr++);
        u16MsCount--;
    }
    return true;
}

uint16_t delayTickCountCb(stTimerStruct_t* newTimer)
{
    bTickDelayMet = true;
    return 1;
}

uint16_t registerTimer(stTimerStruct_t* newTimer)
{
    stTimerStruct_t* iter = &sTimerQueueHead; // start from the head
    uint16_t  uiTimerModeSave;

    // check if new time we are desiring to register is already
    //  in the link. walk down entire chain, end of chain is marked
    //  with a NULL, or until timer is found in chain
    while(iter != newTimer && iter->nextTimer != NULL)
    {
        iter = iter->nextTimer;
    }

    // if timer is found, nothing to do, exit
    // user needs to call the next task, like enabling timer, etc
    if(iter == newTimer)
    {
        return 1;
    }

    // if timer we are adding is not found, need to add
    //  it at the tail of the chain. stop timer before adding to
    //  avoid race condition.
    // get current mode and stop timer
    uiTimerModeSave = TMR_GetTimerOperatingMode(stTickTimerRegsAddress);
    TMR_SetOperatingMode(stTickTimerRegsAddress, TMR_STOP_MODE);


    iter->nextTimer = newTimer;
    newTimer->prevTimer = iter;
    newTimer->nextTimer = NULL;
    newTimer->counter = 0;
    newTimer->status = TIMER_DISABLED;

    // resume timer
    TMR_SetOperatingMode(stTickTimerRegsAddress, uiTimerModeSave);

    return 0;
}


uint16_t enableDisableTimer(stTimerStruct_t* myTimer, bool bEnableDisable)
{
    stTimerStruct_t* iter = &sTimerQueueHead; // start from the head
    uint16_t  uiTimerModeSave;

    if(myTimer == NULL)
    {
        return 1;
    }

    while(iter != myTimer && iter != NULL)
    {
        iter = iter->nextTimer;
    }

    if(iter == NULL)
    {
        return 1;
    }

    if (bEnableDisable)
    {
        if(myTimer->status == TIMER_RUNNING || myTimer->status == TIMER_DONE)
        {
            return 1;
        }
    }
    // get current mode and stop timer
    uiTimerModeSave = TMR_GetTimerOperatingMode(stTickTimerRegsAddress);
    TMR_SetOperatingMode(stTickTimerRegsAddress, TMR_STOP_MODE);

    if (bEnableDisable)
    {
        myTimer->status  = TIMER_RUNNING;
    }
    else
    {
        myTimer->status  = TIMER_DISABLED;
    }

    myTimer->counter = 0;

    // resume timer
    TMR_SetOperatingMode(stTickTimerRegsAddress, uiTimerModeSave);

    return 0;
}


uint16_t deregisterTimer(stTimerStruct_t* oldTimer)
{
    stTimerStruct_t* iter = &sTimerQueueHead; // start from the head
    uint16_t  uiTimerModeSave;

    if(oldTimer == NULL)
    {
        return 1;
    }

    // search down the chain for the timer to remove
    // if found the timer or if reach end of chain stop
    while(iter != oldTimer && iter != NULL)
    {
        iter = iter->nextTimer;
    }

    // if reached end of chain, sw timer does not exist
    //  so abort/return
    if(iter == NULL)
    {
        return 1;
    }

    // iter == oldTimer (if reached here)
    // remove the timer found from the chain
    //  de-associate the forward and backward pointers

    // get current time mode and stop timer
    uiTimerModeSave = TMR_GetTimerOperatingMode(stTickTimerRegsAddress);
    TMR_SetOperatingMode(stTickTimerRegsAddress, TMR_STOP_MODE);

    // update previous timer pointing to oldTimer to point to
    //  where the oldTimer nextTimer points to.
    iter->prevTimer->nextTimer = iter->nextTimer;

    // if timer to be removed is in a middle of the chain
    // modify the nextTimer prviousTimer from pointing to oldTimer
    //  to previousTimer
    if(iter->nextTimer != NULL)
    {
        iter->nextTimer->prevTimer = iter->prevTimer;
    }

    // modify oldTimer next and previous Timer pointes with a NULL
    iter->nextTimer = NULL;
    iter->prevTimer = NULL;

    // change oldTimer status to DISABLED.
    oldTimer->status = TIMER_DISABLED;

    // resume timer
    TMR_SetOperatingMode(stTickTimerRegsAddress, uiTimerModeSave);

    return 0;
}


/*
 * serviceTimers(): set to execute end of every tick count
 */
uint16_t serviceTimers()
{
    stTimerStruct_t* iter = &sTimerQueueHead; // start from the head
    uint16_t uiTimerModeSave;
    uint16_t returnVal = 0;

    gstMainEvts.bits.svcTicker = false;

    // get current mode and stop ticker timer
    uiTimerModeSave = TMR_GetTimerOperatingMode(stTickTimerRegsAddress);
    TMR_SetOperatingMode(stTickTimerRegsAddress, TMR_STOP_MODE);


    while(iter != NULL)
    {
        if(iter->status == TIMER_DONE)
        {
            // service the DONE timer (launch call back)
            if(iter->callback != NULL)
            {
                returnVal |= iter->callback(iter);
            }

            /*
             * re-enable timer if it is recurring;
             * status is changed when a timeout is reached to DONE;
             *  so need to change status back to RUNNING
             */
            if(iter->recurrence == TIMER_RECURRING)
            {
                iter->status = TIMER_RUNNING;
            }
            else
            {   // if it is a single shot, change status from DONE to DISABLED
                iter->status = TIMER_DISABLED;
            }

            iter->counter = 0;
        }
        iter = iter->nextTimer;
    }

    // resume timer to the previous saved mode
    TMR_SetOperatingMode(stTickTimerRegsAddress, uiTimerModeSave);

    return returnVal;
}
