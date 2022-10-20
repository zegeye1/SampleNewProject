/*
 * tmp1075.c
 *
 *  Created on: Mar 9, 2021
 *      Author: ZAlemu
 */

#include <msp430.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <intrinsics.h>
#include "config.h"
#include "i2c.h"
#include "timer_utilities.h"
#include "clocks.h"
#include "main.h"
#include "tmp1075.h"
#include "thermalcontrol.h"

float gfI2cSnsrTempAvg = -1;

const float    fTempSnsrLsb     = 0.0625f; /* Temp Snsr lsb val   */
float afTempSnsrRds[MAX_I2C_TMP1075_MESSAGES];
bool  bTestTzPwm = false;

// I2C Transmit and Receive Buffers
volatile uint8_t abyTmp1075I2cTxBuff[MAX_I2C_TMP1075_MESSAGES][MAX_I2C_TMP1075_MSG_BYTE_CNT];
volatile uint8_t abyTmp1075I2cRxBuff[MAX_I2C_TMP1075_MESSAGES][MAX_I2C_TMP1075_MSG_BYTE_CNT];

// Table holding I2C messages.
// Last entry should be a NULL to signify end of message.
stI2cTrasaction_t* pastTmp1075I2cMsgTable[MAX_I2C_TMP1075_MESSAGES + 1];

// process i2c message indicator
uint8_t gbyProcessI2cTmp1075MsgNum = 0;    // start with message 0
volatile float   fTempAvgSum;
volatile float   fTempAverage;

// count # of i2c messages serviced to identify when an i2c hung is detected
// a fraction of serviced i2c messages is expected and if this does not happen
//  perform an i2c reset; i2cWatchdog
uint8_t gbyI2cTmp1075MsgProcessedCnt = 0;
// note how many i2c reset invoked; to get an idea as to how robust the i2c
//  transactions are.
uint32_t gi32I2cTmp1075MsgSwResetCnt = 0;


// partial initialization while declaring variable
stI2cTrasaction_t stTmp1075I2cMessage0 =
{
    .eI2cSnsrType     = I2C_TEMP_SNSR_TMP1075,
    .pbyTxBuff        = abyTmp1075I2cTxBuff[TMP1075_MSG0],
    .pbyRxBuff        = abyTmp1075I2cRxBuff[TMP1075_MSG0],
    .eI2cInUse        = I2C_0,
    .ubyAddress       = 0x48,
    .ubyRxByteCount   = 2,
    .ubyTxByteCount   = 1,
    .ubyRxByteCounter = 0,
    .ubyTxByteCounter = 0,
    .callback         = xformTmp1075Adc2Temp
};


// partial initialization while declaring variable
stI2cTrasaction_t stTmp1075I2cMessage1 =
{
    .eI2cSnsrType     = I2C_TEMP_SNSR_TMP1075,
    .pbyTxBuff        = abyTmp1075I2cTxBuff[TMP1075_MSG1],
    .pbyRxBuff        = abyTmp1075I2cRxBuff[TMP1075_MSG1],
    .eI2cInUse        = I2C_0,
    .ubyAddress       = 0x4A,
    .ubyRxByteCount   = 2,
    .ubyTxByteCount   = 1,
    .ubyRxByteCounter = 0,
    .ubyTxByteCounter = 0,
    .callback         = xformTmp1075Adc2Temp
};


// partial initialization while declaring variable
stI2cTrasaction_t stTmp1075I2cMessage2 =
{
    .eI2cSnsrType     = I2C_TEMP_SNSR_TMP1075,
    .pbyTxBuff        = abyTmp1075I2cTxBuff[TMP1075_MSG2],
    .pbyRxBuff        = abyTmp1075I2cRxBuff[TMP1075_MSG2],
    .eI2cInUse        = I2C_0,
    .ubyAddress       = 0x48,
    .ubyRxByteCount   = 2,
    .ubyTxByteCount   = 1,
    .ubyRxByteCounter = 0,
    .ubyTxByteCounter = 0,
    .callback         = xformTmp1075Adc2Temp
};


// partial initialization while declaring variable
stI2cTrasaction_t stTmp1075I2cMessage3 =
{
    .eI2cSnsrType     = I2C_TEMP_SNSR_TMP1075,
    .pbyTxBuff        = abyTmp1075I2cTxBuff[TMP1075_MSG3],
    .pbyRxBuff        = abyTmp1075I2cRxBuff[TMP1075_MSG3],
    .eI2cInUse        = I2C_0,
    .ubyAddress       = 0x48,
    .ubyRxByteCount   = 0,
    .ubyTxByteCount   = 1,
    .ubyRxByteCounter = 0,
    .ubyTxByteCounter = 0,
    .callback         = xformTmp1075Adc2Temp
};



stTimerStruct_t stI2cTmp1075PeriodicStartTmr =
{
    .prevTimer        = NULL,
    .timeoutTickCnt   = I2C_TMP1075_PERIODIC_STRT_TICK,
    .counter          = 0,
    .recurrence       = TIMER_RECURRING,
    .status           = TIMER_RUNNING,
    .callback         = loadAndStrtFirstTmp1075I2cMsgCb,
    .nextTimer        = NULL
};


stTimerStruct_t stSwWatchDogTmp1075Tmr =
{
    .prevTimer        = NULL,
    .timeoutTickCnt   = SW_WATCHDOG_TICK_CNT,
    .counter          = 0,
    .recurrence       = TIMER_RECURRING,
    .status           = TIMER_RUNNING,
    /*
     * launchboard is using port P1.0. so handler
     *  is configured to toggle P1.0.
     * insure that the correct port is replaced
     *  when using a different platform
     */
    .callback       = tmp1075I2cWatchDog,
    .nextTimer      = NULL
};


void initI2cTmp1075TransactionsParams()
{
    loadTmp1075I2cMsgTable();
    pstI2cActiveMessage = pastTmp1075I2cMsgTable[0];
    registerTimer(&stI2cTmp1075PeriodicStartTmr);
    enableDisableTimer(&stI2cTmp1075PeriodicStartTmr, TMR_ENABLE);
}

/*
 * a full transaction could be a read, a write or write and read.
 * the structure declaration of data type, stI2cTrasaction_t, has elements
 *  that captures the # of byte writes and byte reads to perform.
 * i2c transactions are based on TMP1075 register access protocol
 *  where a pointer register is written with a value of a Register
 *  address followed by a two byte read of the register.
 */
void loadTmp1075I2cMsgTable()
{
    uint8_t u8Index = 0;
    // load byte data to send
    // Note: if you update the pointer, need to reset it
    *stTmp1075I2cMessage0.pbyTxBuff   = TMP1075_PTR_VAL_4_TEMPERATURE;
    pastTmp1075I2cMsgTable[u8Index++] = &stTmp1075I2cMessage0;

    *stTmp1075I2cMessage1.pbyTxBuff   = TMP1075_PTR_VAL_4_TEMPERATURE;
    pastTmp1075I2cMsgTable[u8Index++] = &stTmp1075I2cMessage1;

    *stTmp1075I2cMessage2.pbyTxBuff   = TMP1075_PTR_VAL_4_DEVICE_ID;
    pastTmp1075I2cMsgTable[u8Index++] = &stTmp1075I2cMessage2;

    *stTmp1075I2cMessage3.pbyTxBuff   = TMP1075_PTR_VAL_4_DEVICE_ID;
    pastTmp1075I2cMsgTable[u8Index++] = &stTmp1075I2cMessage3;

    // signify that an end of table entry
    pastTmp1075I2cMsgTable[u8Index]   = NULL;
}


uint16_t loadAndStrtFirstTmp1075I2cMsgCb(stTimerStruct_t* myTimer)
{
    pstI2cActiveMessage = pastTmp1075I2cMsgTable[0];
    startI2cMsg(pstI2cActiveMessage);
    return 0;
}


/*
 * msp430 is little endian (low byte of data is stored in low address)
 *  16-bit data, 0x1890, is stored at offset n(0x90) and offset n+1(0x18)
 *  TempSnsr (TMP1075) sends temp reading in 2 bytes, with first byte
 *   being the high byte and 2nd byte being the low byte.
 *  Since array is configured in byte format, first byte data received (0x18)
 *   is stored at the first location 'n' and second byte received (0x90) at the
 *   next address, 'n+1'. When accessing this byte array as intger data type
 *   then, msp430 is going to consider the lower byte to be at address 'n'
 *   since it is little endian centric. This makes the data read as integer
 *   to be swapped.
 *  Referring to the above example, msp430 device, will present the byte
 *   array data swapped as 0x9018 since 0x18 is stored at lower address.
 *  For this reason, need to swap out the bytes position and right shift the
 *   data by 4 bits, to extract the useful temp data bits (12 bits).
 *  To compute the temperature, we will need to multiply the shifted value
 *   with the lsb value given in the datasheet, which is 0.0625 oC.
 *
 *  For debug: view memory 16-Bit Hex - C Style and enter 0x3018 for Rx Data.
 */
// #pragma CODE_SECTION(processTempSnsrRds, ".ram_code")

void xformTmp1075Adc2Temp()
{
    static uint8_t ubyTrackNumSnsrs = 0;
    uint16_t ui16Data;
    int16_t  i16Data1st;
    float    fCurrentTempValue;
    float    fTempAvg;

    gstMainEvts.bits.xformI2cMsg = false;

    // swap bytes of received data
    // do not use pointer inc since the actual msg initial value will be modified
    //  unless you plan to re-adjust it again before exiting function
    ui16Data = (*(pastTmp1075I2cMsgTable[gbyProcessI2cTmp1075MsgNum]->pbyRxBuff) << 8) |
               *(pastTmp1075I2cMsgTable[gbyProcessI2cTmp1075MsgNum]->pbyRxBuff+1);
    // high 12-bits of 16-bit data is needed
    i16Data1st = (int16_t)ui16Data >> 4;
    pastTmp1075I2cMsgTable[gbyProcessI2cTmp1075MsgNum]->fI2cRead1stValue = i16Data1st * fTempSnsrLsb;

    fCurrentTempValue  = pastTmp1075I2cMsgTable[gbyProcessI2cTmp1075MsgNum]->fI2cRead1stValue;
    pastTmp1075I2cMsgTable[gbyProcessI2cTmp1075MsgNum]->fI2cRead1stValueSave = fCurrentTempValue;
//
//    gfTempAvg = updateAverageTemp();
//    processThermalControl();
//
    if(bTestTzPwm)
    {
        testTzAndPwm();
    }
    else
    {
        if (++ubyTrackNumSnsrs == MAX_I2C_TMP1075_MESSAGES)
        {
            ubyTrackNumSnsrs = 0;   // reset track indicator

            fTempAvg = 0;
            // use the track indicator for indexing while in the for loop
            for (ubyTrackNumSnsrs=0; ubyTrackNumSnsrs<MAX_I2C_TMP1075_MESSAGES; ubyTrackNumSnsrs++)
            {
                fTempAvg += pastTmp1075I2cMsgTable[ubyTrackNumSnsrs]->fI2cRead1stValueSave;
            }

            //gfTempAvg = fTempAvg/4;
            gfI2cSnsrTempAvg = fTempAvg/MAX_I2C_TMP1075_MESSAGES;

 //           processThermalControl();
        }

        // just in case; this was being violated when going to testmode and coming back
        if(ubyTrackNumSnsrs > MAX_I2C_TMP1075_MESSAGES)
        {
            ubyTrackNumSnsrs = 0;
        }

    }

    __no_operation();
    __no_operation();
}

float updateAverageTemp()
{

    uint8_t ubyIndx;
    uint8_t ubyTempIndx;

    fTempAvgSum = 0;
    ubyTempIndx = 0;

    for(ubyIndx=0; ubyIndx<MAX_I2C_TMP1075_MESSAGES; ubyIndx++)
    {   // some of the sensors might not be temperature sensors
        if(pastTmp1075I2cMsgTable[ubyIndx]->eI2cSnsrType == I2C_TEMP_SNSR_TMP1075)
        {
            ubyTempIndx++;       // track # of temp sensors
            fTempAvgSum += pastTmp1075I2cMsgTable[ubyIndx]->fI2cRead1stValueSave;
        }
    }

    if (ubyTempIndx)
    {
        fTempAverage = fTempAvgSum/ubyTempIndx;
    }

    return fTempAverage;

}

void testTzAndPwm()
{
    static int8_t i8AvgTemp = -41;
    static bool   bTempRampUp = true;

    if(bTempRampUp)
    {
        i8AvgTemp++;
        if (i8AvgTemp >= 60)
        {
            bTempRampUp = false;
        }
    }
    else
    {
        i8AvgTemp--;
        if (i8AvgTemp <= -40)
        {
            bTempRampUp = true;
        }
    }

    gfI2cSnsrTempAvg = i8AvgTemp;
    processThermalControl();
}

uint16_t tmp1075I2cWatchDog(stTimerStruct_t* myTimer)
{
    // expect to process at least RST_I2C_TRANSACTION_CNT amount of I2C msg
    //  if not, most likely i2C is not running
    if(gbyI2cTmp1075MsgProcessedCnt <= RST_I2C_TRANSACTION_CNT)
    {
        // reset both I2c; don't bother figuring out which i2c to use
        UCB0CTLW0 |= UCSWRST;
        UCB1CTLW1 |= UCSWRST;

        gi32I2cTmp1075MsgSwResetCnt++;

        // init will register and enable load timer;
        enableDisableTimer(&stI2cTmp1075PeriodicStartTmr, TMR_DISABLE);
        deregisterTimer(&stI2cTmp1075PeriodicStartTmr);

        // got this from main
        initI2c(I2C_0, I2C_BAUD_IN_USE);

        initI2cTmp1075TransactionsParams();
    }

    gbyI2cTmp1075MsgProcessedCnt = 0;

    return 0;
}


void processTmp1075I2cMsg()
{
    static uint8_t ubyMsgIndex = 0;
    static bool bRptStrtInvoked = false;

    gstMainEvts.bits.svcI2c = false;

    if(stI2cMessageActive.eStatus == I2C_COMPLETE)
    {
        // wait till bus is idle
        if(stI2cMessageActive.eI2cInUse == I2C_0)
        {
            while(UCB0STATW & UCBBUSY);
        }
        else
        {
            while(UCB1STATW & UCBBUSY);
        }

        // indicate Message Number to be processed
        gbyProcessI2cTmp1075MsgNum = ubyMsgIndex;
        gstMainEvts.bits.xformI2cMsg = true;

        pstI2cActiveMessage->eStatus = I2C_IDLE;
    }
    else if (stI2cMessageActive.eStatus == I2C_NAK)
    {
        if(bRptStrtInvoked == false)
        {
            bRptStrtInvoked = true;
            invokeStartCondition();     // attempt to recover
            return;
        }
        else
        {
            bRptStrtInvoked = false;
            invokeStopCondition();      // done with attempting/shut it down
        }
    }

    // load next message and start transaction if not reached end of table
    if(pastTmp1075I2cMsgTable[++ubyMsgIndex] != NULL)
    {
        pstI2cActiveMessage = pastTmp1075I2cMsgTable[ubyMsgIndex];
        startI2cMsg(pstI2cActiveMessage);
    }
    else
    {
        // reset message table index
        ubyMsgIndex = 0;
    }

    // sw watchdog will clear this value periodically
    // used to trigger a reset on I2C in case it goes off line
    gbyI2cTmp1075MsgProcessedCnt++;
}



