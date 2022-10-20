/*
 * cli.c
 *
 *  Created on: Feb 1, 2021
 *      Author: ZAlemu
 */

#include <msp430.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "main.h"
#include "timer.h"
#include "uart.h"
#include "cli.h"
#include "i2c.h"
#include "config.h"
#include "adc.h"
#include "tmp1075.h"
#include "rtd.h"
#include "thermalcontrol.h"
#include "fans.h"
#include "bsl.h"


#define DIAG_TMR_TO_VAL         10      // default timeout val
/*
 * DIAG_UPDATED_TO_PERIOD renamed to DIAG_DISPLAY_INTERVAL_PERIOD
 * and moved it to cofig.h
 *
 */
//#define DIAG_DISPLAY_INTERVAL_PERIOD  1000    // timeout val when diag's running

char achCmdLineBuff[SERIAL_BUFFER_SZ];
static uint8_t ubyCmdLineBuffIndx;
bool gbDiagTimeoutChanged;
bool gbResetDiagTmr = false;
bool bIsDiagActive = false;

eDiagTmrState_t geDiagTmrUpdateState;

float    fDummyTestNum1    = 25.6;
float    fDummyTestNum2    = 0.5;
uint16_t ui16DummyTestNum1 = 29;

int16_t  getFract(float fNum);
uint16_t getInt(float fNum);
int16_t  i16NumOfZeros;

stTimerStruct_t gstCopySeralCmdToCmdBuffTmr =
{
    .prevTimer  = NULL,
    .timeoutTickCnt  = 5,
    .counter    = 0,
    .recurrence = TIMER_SINGLE,
    .status     = TIMER_DISABLED,
    .callback   = cpySerialCmd2CmdBuf,
    .nextTimer  = NULL
};

stTimerStruct_t stSerialCmdMakeTokensTmr =
{
    .prevTimer  = NULL,
    .timeoutTickCnt  = 10,
    .counter    = 0,
    .recurrence = TIMER_SINGLE,
    .status     = TIMER_DISABLED,
    .callback   = makeTokens,
    .nextTimer  = NULL
};

stTimerStruct_t stSerialCmdTokenExecuteTmr =
{
    .prevTimer  = NULL,
    .timeoutTickCnt  = 10,
    .counter    = 0,
    .recurrence = TIMER_SINGLE,
    .status     = TIMER_DISABLED,
    .callback   = executeUartCmd,
    .nextTimer  = NULL
};


stTimerStruct_t stDiagnosticsDataTmr =
{
    .prevTimer  = NULL,
    .timeoutTickCnt  = DIAG_TMR_TO_VAL,
    .counter    = 0,
    .recurrence = TIMER_SINGLE,
    .status     = TIMER_DISABLED,
    .callback   = outputDiagData,
    .nextTimer  = NULL
};

stTimerStruct_t stUpdateDiagTimoutTmr =
{
    .prevTimer  = NULL,
    .timeoutTickCnt  = 50,
    .counter    = 0,
    .recurrence = TIMER_SINGLE,
    .status     = TIMER_DISABLED,
    .callback   = updateDiagTimer,
    .nextTimer  = NULL
};


stTimerStruct_t stBslLaunchTmr =
{
    .prevTimer        = NULL,
    .timeoutTickCnt   = 10,
    .counter          = 0,
    .recurrence       = TIMER_SINGLE,
    .status           = TIMER_DISABLED,
    .callback         = updateCmdCb,
    .nextTimer        = NULL
};

uint16_t updateCmdCb(stTimerStruct_t* myTimer)
{
    char c;

    while(gstMainEvts.bits.svcUartRx == false);
    c= achUartInputBuffer[0];
    c = tolower(c);

    if (c == 'y')
    {
        invokeBsl();
    }
    else
    {
        UART_putStringSerial("Firmware updated aborted\n\r");
        UART_printNewLineAndPrompt();
    }

    return 0;
}


typedef void (*pFuncCmdCb)();

typedef struct CLI_CMDS
{
    char*       pchCmdString;
    pFuncCmdCb  pCbUartCmdHdlr;
}stCliCmds_t;

stCliCmds_t astCliCmds[NUM_CMDS];
char* achTokenArray[MAX_CMD_LENGTH];
uint8_t ubyTokenIndex;

uint16_t cpySerialCmd2CmdBuf(stTimerStruct_t* myTimer)
{
    ubyCmdLineBuffIndx = 0;

    do
    {
        achCmdLineBuff[ubyCmdLineBuffIndx++] = achUartInputBuffer[ubyUrtInBuffUnLdIndx];
    }while(achUartInputBuffer[ubyUrtInBuffUnLdIndx++] != NULL);

    ubyUrtInBuffUnLdIndx = 0;
    ubyUrtInBuffLdrIndx  = 0;

    UART_EnableDisableRxInt(UART_ENABLE_RXINT);
    enableDisableTimer(&stSerialCmdMakeTokensTmr, TMR_ENABLE);

    return 0;
}


// CR LF    0D 0A   13 10   \r\n
/*
 * command table, cliCMDS, is initialized with the commands supported
 * each command can have many more variations
 *  e.g. get temp exhaust
 *       get temp ssd
 *       get vin
 *       etc
 *  what is loadded in cliCMDS table is the main command.
 *
 *  lineBuff[] buffer can be considered as a command line buffer.
 *  characters stored @ InBuff by UART-RxISR, are copied in lineBuff
 *   array. The length of characters copied is upto reaching a <CR>
 *   control character.
 *
 *  cliStat and prevCliState are states used by processCli() function
 *   to copy chars from InBuff to lineBuff, parse command line and
 *   launch a callback function for execution.
 *
 *  eUSCI0 is the UART used for this comms and it's baud rate is
 *   configured for 115,200 baud with no parity, 8-bits data,
 *   1 start bit and 1 stop bit.
 */
void initCli()
{
    // Initialize our command structure
    astCliCmds[0].pchCmdString = "get"      ; astCliCmds[0].pCbUartCmdHdlr = getCMD;
    astCliCmds[1].pchCmdString = "set"      ; astCliCmds[1].pCbUartCmdHdlr = setCMD;
    astCliCmds[2].pchCmdString = "update"   ; astCliCmds[2].pCbUartCmdHdlr = updateCMD;
    astCliCmds[3].pchCmdString = "man"      ; astCliCmds[3].pCbUartCmdHdlr = manCMD;


    registerTimer(&gstCopySeralCmdToCmdBuffTmr);
    registerTimer(&stSerialCmdMakeTokensTmr);
    registerTimer(&stSerialCmdTokenExecuteTmr);
}


uint16_t executeUartCmd(stTimerStruct_t* myTimer)
{
    uint16_t i;

    for (i=0; i<NUM_CMDS; i++)
    {
        if (strcmp((const char*)achTokenArray[0],(const char*)astCliCmds[i].pchCmdString) == 0)
        {
            astCliCmds[i].pCbUartCmdHdlr();
            return 0;
        }
    }
    UART_putStringSerial("Unrecognized Command!");
    UART_printNewLineAndPrompt();
    return 1;
}

void getCMD()
{
    char    achStringBuff[256];
    bool    bIsCmdGood = false;
    volatile uint8_t ubyIndexFan;
    volatile uint8_t ubyIndexZone;

    // %f has been deprecated from sprinf()
    if((strcmp((const char*)achTokenArray[1],"pwm") == 0) && (ubyTokenIndex == 2))
    {
        UART_putStringSerial("\r\nCurrent Temp Zone & PWM settings\r\nFan#   TZ   PWM%");

        for(ubyIndexFan=0; ubyIndexFan<NUM_FANS; ubyIndexFan++)
        {
            sprintf (achStringBuff, "\r\n %d      %d     %d",ubyIndexFan, gubyCurrentTz[ubyIndexFan], fFanPwm[ubyIndexFan][gubyCurrentTz[ubyIndexFan]]);
            UART_putStringSerial(achStringBuff);
        }
        sprintf (achStringBuff, "\r\n");
        UART_putStringSerial(achStringBuff);
        UART_printNewLineAndPrompt();
        bIsCmdGood = true;
    }
    else if((strcmp((const char*)achTokenArray[1],"hyst") == 0)&& (ubyTokenIndex == 2))
    {
        sprintf(achStringBuff, "current set temperature hysteresis = %d, ", ubyTempHysteresis);
        UART_putStringSerial(achStringBuff);
        UART_printNewLineAndPrompt();
        bIsCmdGood = true;
    }

    // get pwms
    // set pwm pwm# zone# val
    else if((strcmp((const char*)achTokenArray[1],"pwms") == 0) && (ubyTokenIndex == 2))
    {
        sprintf (achStringBuff, "Fan# Zones [0-%d]\r\n", NUM_TZONES-1);
        UART_putStringSerial(achStringBuff);
        for(ubyIndexFan=0; ubyIndexFan<NUM_FANS; ubyIndexFan++)
        {
            sprintf (achStringBuff, "  %d:[", ubyIndexFan);
            UART_putStringSerial(achStringBuff);
            for(ubyIndexZone=0; ubyIndexZone<NUM_TZONES; ubyIndexZone++)
            {
                if(ubyIndexZone == NUM_TZONES-1)
                {
                    sprintf (achStringBuff, "%d",fFanPwm[ubyIndexFan][ubyIndexZone]);
                }
                else
                {
                    sprintf (achStringBuff, "%d,",fFanPwm[ubyIndexFan][ubyIndexZone]);
                }
                UART_putStringSerial(achStringBuff);
            }
            sprintf (achStringBuff, "]\r\n");
            UART_putStringSerial(achStringBuff);
        }
        UART_printNewLineAndPrompt();
        bIsCmdGood = true;
    }
    // Example: 'get thresholds'
    else if((strcmp((const char*)achTokenArray[1],"tempthresh") == 0)&& (ubyTokenIndex == 2))
    {

        sprintf (achStringBuff, "Z#0 L=%d  H=%d \r\n", getInt(fTz[0][0]), getInt(fTz[0][1]));    // cold zone
        UART_putStringSerial(achStringBuff);
        sprintf (achStringBuff, "Z#1 L=%d  H=%d \r\n", getInt(fTz[1][0]), getInt(fTz[1][1]));    // normal zone 1
        UART_putStringSerial(achStringBuff);
        sprintf (achStringBuff, "Z#2 L=%d   H=%d \r\n", getInt(fTz[2][0]), getInt(fTz[2][1]));    // normal zone 2
        UART_putStringSerial(achStringBuff);
        sprintf (achStringBuff, "Z#3 L=%d   H=%d \r\n", getInt(fTz[3][0]), getInt(fTz[3][1]));    // normal zone 2
        UART_putStringSerial(achStringBuff);
        sprintf (achStringBuff, "Z#4 L=%d   H=%d \r\n", getInt(fTz[4][0]), getInt(fTz[4][1]));    // normal zone 2
        UART_putStringSerial(achStringBuff);
        sprintf (achStringBuff, "Z#5 L=%d   H=%d \r\n", getInt(fTz[5][0]), getInt(fTz[5][1]));    // normal zone 2
        UART_putStringSerial(achStringBuff);
        sprintf (achStringBuff, "Z#6 L=%d   H=%d \r\n", getInt(fTz[6][0]), getInt(fTz[6][1]));    // high zone
        UART_putStringSerial(achStringBuff);
        sprintf (achStringBuff, "Z#7 L=%d   H=%d \r\n\n", getInt(fTz[7][0]), getInt(fTz[7][1]));    // extreme/max zone
        UART_putStringSerial(achStringBuff);
        sprintf(achStringBuff, "hysteresis = %d, ", ubyTempHysteresis);
        UART_putStringSerial(achStringBuff);
        UART_printNewLineAndPrompt();
        bIsCmdGood = true;
    }
    else if((strcmp((const char*)achTokenArray[1],"range") == 0) && (ubyTokenIndex == 3))
    {
        if(strcmp((const char*)achTokenArray[2],"max") == 0)
        {
            sprintf (achStringBuff, "Max Temperature Range is: %d", gbyTmpRangeMax);
        }
        else if(strcmp((const char*)achTokenArray[2],"min") == 0)
        {
            sprintf (achStringBuff, "Min Temperature Range is: %d", gbyTmpRangeMin);
        }
        UART_putStringSerial(achStringBuff);
        UART_printNewLineAndPrompt();
        bIsCmdGood = true;
    }
    else if ((strcmp((const char*)achTokenArray[1],"version") == 0) && (ubyTokenIndex == 2))
    {
        stTimerStruct_t dummyTimerStructure;
        displayBannerCb(&dummyTimerStructure);
        bIsCmdGood = true;
    }

    if (bIsCmdGood == false)
    {
        UART_putStringSerial("Unrecognized Command!");
        UART_printNewLineAndPrompt();
    }
}


uint16_t getInt(float fNum)
{
    return (int)fNum;
}

int16_t getFract(float fNum)
{
    int16_t i16fNum;
    float fFrac;

    i16fNum = (int)fNum;
    fFrac = (fNum - i16fNum)*100;

    if(fFrac < 0)
    {
        fFrac *= -1;    // remove the sign
    }
    i16NumOfZeros = (int)fFrac;
    if(i16NumOfZeros)
    {
        if(i16NumOfZeros < 9)
        {
            i16NumOfZeros = 1;
        }
        else
        {
            i16NumOfZeros = 0;
        }
    }
    return (int)fFrac;
}


void setCMD()
{
    char    achStringBuff[256];
//    float   fTempZone;
    uint8_t ubyPwmNum;
    uint8_t ubyZoneNum;
    uint8_t ubyPwmPercentage;
    int8_t  byTempLowVal;
    int8_t  byTempHighVal;
    int8_t  byTempRangeval;


    /*
     * start with the assumption that the command is entered wrong or just wrong
     * reason is because, we want to reduce the amount of cases where
     *  "unrecognized command" string is to be displayed.
     * if command is successful, then this boolean, bIsCmdGood, will be set
     *  to true so that the "unrecognized command" string is not driven out
     */
    bool bIsCmdGood = false;

    if((strcmp((const char*)achTokenArray[1],"diag") == 0)&& (ubyTokenIndex == 3))
    {
        if (strcmp((const char*)achTokenArray[2],"on") == 0)
        {
            if (!bIsDiagActive)
            {
                enableDiagnostics();
                UART_putStringSerial("Diagnostics Data Started:");
                UART_printNewLine();
            }
            else
            {
                UART_putStringSerial("Diagnostics is running :-)");
                UART_printNewLine();
            }

            bIsCmdGood = true;
        }
        else if (strcmp((const char*)achTokenArray[2],"off") == 0)
        {
            if(bIsDiagActive)
            {
                geDiagTmrUpdateState = DIAG_RESET_DIAG_TMR;
                gstMainEvts.bits.svcDiag = true;
            }
            else
            {
                UART_putStringSerial("Diagnostics not running :-(");
                UART_printNewLineAndPrompt();
            }

            bIsCmdGood = true;
        }
    }
    // set pwm pwm# zone# val
    else if((strcmp((const char*)achTokenArray[1],"pwm") == 0)  && (ubyTokenIndex == 5))
    {
        ubyPwmNum        = (uint8_t)(int16_t)(atof(achTokenArray[2]));
        if (ubyPwmNum < NUM_FANS)
        {
            ubyZoneNum       = (uint8_t)(int16_t)(atof(achTokenArray[3]));
            ubyPwmPercentage = (uint8_t)(int16_t)(atof(achTokenArray[4]));

            if((ubyPwmNum < 6) && (ubyZoneNum < 8) && (ubyPwmPercentage < 101))
            {
                fFanPwm[ubyPwmNum][ubyZoneNum] = ubyPwmPercentage;
                sprintf (achStringBuff, "Changed Zone#%d Duty Cycle for PWM%d to %d%%",ubyZoneNum, ubyPwmNum, ubyPwmPercentage);
                UART_putStringSerial(achStringBuff);
                UART_printNewLineAndPrompt();
                setPwmFromTz();
                bIsCmdGood = true;
            }
        }
    }
    // set tempthresh zone# low_range_val high_range_val

    /*
     * // change zone#0 High threshold value from 15C to 20C followed by
     * // changing zone#1 Low threshold value from 15C to 20C (to make sure temperature ranges are continuous).
     *
     * // command: invoke 'set tempthresh' command with appropriate values.
     * >>set tempthresh 0 -99 20
     * Changed zone 0 Low and High temperature ranges  Check new ranges by invoking 'get tempthresh' command
     * if ok with entries invoke 'set tempupdate' command
     *         it is user's responsibility to insure temp ranges are continuous
     *
     * >>
     *
     * // check the new entries by invoking 'get tempthresh' command
     * // command:
     * >>get tempthresh
     * Z#0 L=-99  H=20    <= High value of Zone 0 was changed from 15C to 20C
     * Z#1 L=15  H=25     <= However, the Low Value of Zone 1 remained 15C. We would need to change this to 20C.
     * Z#2 L=25   H=30
     * Z#3 L=30   H=35
     * Z#4 L=35   H=40
     * Z#5 L=40   H=45
     * Z#6 L=45   H=50
     * Z#7 L=50   H=999
     *
     * hysteresis = 2,
     *
     *
     * // in order to make sure temerature ranges are continuous, change Zone#1 Low threshold value from 15C to 20C.
     * // command:
     * >>set tempthresh 1 20 25
     * Changed zone 1 Low and High temperature ranges  Check new ranges by invoking 'get tempthresh' command
     *         if ok with entries invoke 'set tempupdate' command
     *         it is user's responsibility to insure temp ranges are continuous
     *
     * >>
     *
     * // command:
     * >>get tempthresh
     * Z#0 L=-99  H=20
     * Z#1 L=20  H=25     <= Low value changed from 15C to 20C (all range now is continuous)
     * Z#2 L=25   H=30
     * Z#3 L=30   H=35
     * Z#4 L=35   H=40
     * Z#5 L=40   H=45
     * Z#6 L=45   H=50
     * Z#7 L=50   H=999
     *
     * hysteresis = 2,
     * >>
     *
     * // now that we are good with the above table, invoke 'set tempupdate' command
     * // command:
     * >>set tempupdate
     * updated temp zone and related pwm setting
     * >>
     * >>
     */
    else if ((strcmp((const char*)achTokenArray[1],"tempthresh") == 0)  && (ubyTokenIndex == 5))
    {
        ubyZoneNum    = (uint8_t)(int16_t)(atof(achTokenArray[2]));
        byTempLowVal  = (uint8_t)(int16_t)(atof(achTokenArray[3]));
        byTempHighVal = (uint8_t)(int16_t)(atof(achTokenArray[4]));
        if(ubyZoneNum < 8)
        {
            fTz[ubyZoneNum][0] = byTempLowVal;
            fTz[ubyZoneNum][1] = byTempHighVal;
            sprintf (achStringBuff, "Changed zone %d Low and High temperature ranges", ubyZoneNum);
            UART_putStringSerial(achStringBuff);
            UART_putStringSerial("\tCheck new ranges by invoking 'get tempthresh' command\n\r");
            UART_putStringSerial("\tif ok with entries invoke 'set tempupdate' command\n\r");
            UART_putStringSerial("\tit is user's responsibility to insure temp ranges are continuous\n\r");
            UART_printNewLineAndPrompt();
            bIsCmdGood = true;
        }
        else
        {
            bIsCmdGood = false;
        }
    }
    else if((strcmp((const char*)achTokenArray[1],"hyst") == 0) && (ubyTokenIndex == 3))
    {
        ubyTempHysteresis = atof(achTokenArray[2]);
        UART_putStringSerial("updated temp hysteresis; use get tempthresh cmd to see update");
        UART_printNewLineAndPrompt();
        bIsCmdGood = true;
    }
    else if (strcmp((const char*)achTokenArray[1],"tempupdate") == 0)
    {
        findTz();
        setPwmFromTz();
        UART_putStringSerial("updated temp zone and related pwm setting");
        UART_printNewLineAndPrompt();
        bIsCmdGood = true;
    }
    else if ((strcmp((const char*)achTokenArray[1],"tempcycle") == 0) && (ubyTokenIndex == 3))
    {
        bIsCmdGood = true;
        if (strcmp((const char*)achTokenArray[2],"on") == 0)
        {
            gbEnableTempCycleTest = true;
            sprintf (achStringBuff, "temp cycling for debugging enabled\r\n");
            UART_putStringSerial(achStringBuff);
        }
        else if (strcmp((const char*)achTokenArray[2],"off") == 0)
        {
            gbEnableTempCycleTest = false;
            sprintf (achStringBuff, "temp cycling for debugging disabled\r\n");
            UART_putStringSerial(achStringBuff);
        }
        else
        {
            bIsCmdGood = false;
        }
    }
    else if((strcmp((const char*)achTokenArray[1],"defaults") == 0) && (ubyTokenIndex == 2))
    {
        bIsCmdGood = true;
        persistentMemoryInitialized = 0;
        initThermalControl();
        findTz();
        setPwmFromTz();
        UART_putStringSerial("persistent variables set to default values");
        UART_printNewLineAndPrompt();
    }
    else if((strcmp((const char*)achTokenArray[1],"range") == 0) && (ubyTokenIndex == 4))
    {
        bIsCmdGood = true;
        if (strcmp((const char*)achTokenArray[2],"max") == 0)
        {
            if((byTempRangeval = (int8_t)(int16_t)(atof(achTokenArray[3]))) < 0)
            {
               UART_putStringSerial("value entered is > max value of 127\r\n");
               UART_putStringSerial("please enter good value.");
            }
            else
            {
                gbyTmpRangeMax = byTempRangeval;
                sprintf (achStringBuff, "Max temperature range has changed to: %d", gbyTmpRangeMax);
                UART_putStringSerial(achStringBuff);
            }
        }
        else if (strcmp((const char*)achTokenArray[2],"min") == 0)
        {
            gbyTmpRangeMin = (int8_t)(int16_t)(atof(achTokenArray[3]));
            sprintf (achStringBuff, "Min temperature range has changed to: %d", gbyTmpRangeMin);
            UART_putStringSerial(achStringBuff);
        }
        else
        {
            bIsCmdGood = false;
        }
    }
    else if((strcmp((const char*)achTokenArray[1],"defaults") == 0) && (ubyTokenIndex == 1))
    {
        persistentMemoryInitialized = 0;
        initThermalControl();
        findTz();
        setPwmFromTz();
        bIsCmdGood = true;
    }

    if (!bIsCmdGood)
    {
        UART_putStringSerial("Unrecognized Command!");
    }
    UART_printNewLineAndPrompt();

    return;
}

void manCMD()
{
    UART_putStringSerial("set diag/tempcycle on/off\r\n");
    UART_putStringSerial("get pwm(s)/hyst (for set hyst cmd: enter val\r\n");
    UART_putStringSerial("set pwm#:0-1 Zn#:0-7 Pwm%:0-100>\r\n");
    UART_putStringSerial("get/set tempthresh (for set cmd: Zn#:0-7 HVal LVal)\r\n");
    UART_putStringSerial("get/set range max/min\r\n");
    UART_putStringSerial("set tempupdate\r\n");
    UART_putStringSerial("set defaults\r\n");
    UART_putStringSerial("get version\r\n");
    UART_putStringSerial("update\r\n");
    UART_printNewLineAndPrompt();
}

void updateCMD()
{
    UART_putStringSerial("About to update Firmware\n\r");
    UART_putStringSerial("Press Y to update, N to abort\n\r");
    registerTimer(&stBslLaunchTmr);
    enableDisableTimer(&stBslLaunchTmr, TMR_ENABLE);
}


// parse string from the supplied parameters into tokens and store token
//  addresses into a global table tokenArray[].
/* Divide input line into tokens */
uint16_t makeTokens(stTimerStruct_t* myTimer)
{
static char *chToken;
static char *chTokenChar;

    ubyTokenIndex = 0;
    // get first token/word
    chToken = strtok(achCmdLineBuff, " ,");

    /*
     * change all word characters case to lower case
     *
     * get the next token (if no token is found a NULL
     *  is returned)
     */
    while ((chToken != NULL) && (ubyTokenIndex < MAX_CMD_LENGTH))
    {
        // make a copy of the token address; need token later
        chTokenChar = chToken;

        /*
         * chTokenChar is pointing to the start address of a token.
         * each token is a word stored as a string, i.e. an array
         *  of characters ending with a null char.
         * '*chTokenChar' is pointing to the start address of the token.
         * accessing one char element of the token/word at a time and
         *  insuring all chars are in lower case format.
         * do to lower case translation until reaching the end marker
         *  of a string, '\0'.
         */
        for ( ; *chTokenChar; ++chTokenChar)
        {
            *chTokenChar = tolower(*chTokenChar);  // Make lower case
        }

        // store token address in to a table
        achTokenArray[ubyTokenIndex++] = chToken;            // Add tokens into the array

        // get next token. Note that a NULL is used for accessing successive
        //  tokens. This is because, the input parameter string content is
        //  changing for every token that is extracted and can not
        //  use the original string identifier. If so, the token value
        //  will remain the same first token and will never exit this
        //  loop.
        chToken = strtok(NULL, " ,");
    }

    // indicate last tokenArray[] table entry as a NULL
    achTokenArray[ubyTokenIndex] = NULL;

    if(ubyTokenIndex != 0)
    {
        enableDisableTimer(&stSerialCmdTokenExecuteTmr, TMR_ENABLE);
    }
    else
    {
        UART_putStringSerial("Unrecognized Command!");
        UART_printNewLineAndPrompt();
    }

    return 0;
}


void enableDiagnostics(void)
{
    registerTimer(&stDiagnosticsDataTmr);
    enableDisableTimer(&stDiagnosticsDataTmr, TMR_ENABLE);
    gbDiagTimeoutChanged = false;
}


void disableDiagnostics(void)
{
    enableDisableTimer(&stDiagnosticsDataTmr, TMR_DISABLE);
    deregisterTimer(&stDiagnosticsDataTmr);
    geDiagTmrUpdateState= DIAG_DEREGISTER_UPDATE_TMR;
    gstMainEvts.bits.svcDiag = true;
}


uint16_t outputDiagData(stTimerStruct_t* myTimer)
{
    eDiagElement_t eDiagElement;

    if(!gbDiagTimeoutChanged)
    {
        // tell main to schedule the timeout change
        // can't self medicate. this timer can not change all of its own
        //  states
        gbDiagTimeoutChanged = true;
        geDiagTmrUpdateState = DIAG_ENABLE_UPDATE_TMR;
        gstMainEvts.bits.svcDiag = true;
    }

    for(eDiagElement = DIAG_START; eDiagElement < DIAG_STOP; eDiagElement++)
    {
        printDiagElement(eDiagElement);
    }

    UART_printNewLine();

    return 0;
}


void maintainDiagMsgDisplay(void)
{
    gstMainEvts.bits.svcDiag = false;

    switch(geDiagTmrUpdateState)
    {
    case DIAG_UPDATE_NONE:
        break;

    case DIAG_ENABLE_UPDATE_TMR:
        registerTimer(&stUpdateDiagTimoutTmr);
    case DIAG_RESET_DIAG_TMR:
    case DIAG_DEREGISTER_DIAG_TMR:
        enableDisableTimer(&stUpdateDiagTimoutTmr, TMR_ENABLE);
        break;

    case DIAG_MODIFY_DIAG_TMR:
        enableDisableTimer(&stDiagnosticsDataTmr, TMR_ENABLE);
        break;

    case DIAG_DEREGISTER_UPDATE_TMR:
        disableDiagUpdateTimer();
        break;
    }

}


/*
 * The diag Timer timeout elements, timeoutTickCnt and recurrence have been
 *  modified from default values (DIAG_TMR_TO_VAL and TIMER_SINGLE) to
 *   and  value is replaced to DIAG_UPDATED_TO_PERIOD and TIMER_RECURSIVE.
 * This is performed by the below callback function, updateDiagTimer(),
 *  which in turn is launched from a timer object,  stUpdateDiagTimoutTmr.
 * Resaon for performing this is that, the diag timer, stDiagnosticsDataTmr,
 *  can not self modify, that is user can not change some of its element
 *  contents while executing the timer routines.
 */
uint16_t updateDiagTimer(stTimerStruct_t* myTimer)
{
    if (geDiagTmrUpdateState == DIAG_ENABLE_UPDATE_TMR)
    {
        updateDiagTimeout(DIAG_DISPLAY_INTERVAL_PERIOD);
        geDiagTmrUpdateState = DIAG_UPDATE_NONE;
        bIsDiagActive        = true;        // indicate that diag is in process
        enableDisableTimer(&stDiagnosticsDataTmr, TMR_ENABLE);
    }
    else if(geDiagTmrUpdateState == DIAG_RESET_DIAG_TMR)
    {
        resetDiagTimeout();
        disableDiagnostics();
        geDiagTmrUpdateState     = DIAG_DEREGISTER_DIAG_TMR;
        bIsDiagActive            = false;   // indicate that diag is stopped
        gstMainEvts.bits.svcDiag = true;
    }
    else if(geDiagTmrUpdateState == DIAG_DEREGISTER_DIAG_TMR)
    {
//        disableDiagnostics();
        geDiagTmrUpdateState     = DIAG_DEREGISTER_UPDATE_TMR;
        gstMainEvts.bits.svcDiag = true;
    }


    return 0;
}


void disableDiagUpdateTimer(void)
{
    enableDisableTimer(&stUpdateDiagTimoutTmr, TMR_DISABLE);
    deregisterTimer(&stUpdateDiagTimoutTmr);
    geDiagTmrUpdateState = DIAG_UPDATE_NONE;
    UART_putStringSerial("Diagnostics Data Stopped:");
    UART_printNewLineAndPrompt();
}

void updateDiagTimeout(uint16_t u16Val)
{
    stDiagnosticsDataTmr.recurrence     = TIMER_RECURRING;
    stDiagnosticsDataTmr.timeoutTickCnt = u16Val;
}


/*
 * The diagnostic timer was being used, not only to launch
 *  the first diagnostics but the recurring one's. In order
 *  to achieve this, the recurrence and timeoutTickCnt values
 *  were replaced with TIMER_RECURSIVE and DIAG_UPDATED_TO_PERIOD.
 * When the command to turn off the diag display is executed,
 *  the default values need to be placed back. That is what
 *  this function does; restore default values.
 */
void resetDiagTimeout()
{
    stDiagnosticsDataTmr.recurrence     = TIMER_SINGLE;
    stDiagnosticsDataTmr.timeoutTickCnt = DIAG_TMR_TO_VAL;
}


uint16_t printDiagElement(eDiagElement_t eDiagElement)
{
    stDiagDataStruc_t stDiagVal;
    char    achDiagBuff[TEXT_STR_SZ];
    int16_t i16WholePart;
    int16_t i16FracPart;

    /*
     * number of diag elements displayed depends on the enum type eDiagElement_t.
     * for each element of type eDiagElement_t defined, there should be a corresponding
     *  handler, case statement, within getDiagVal() function.
     */
    stDiagVal = getDiagVal(eDiagElement);

    switch(stDiagVal.eDataTypeVal)
    {
    case DIAG_FLOAT_TYPE:
        i16WholePart = getInt(stDiagVal.fDataVal);
        i16FracPart  = getFract(stDiagVal.fDataVal);
        switch(i16NumOfZeros)
        {
            default:
            case 0:
                sprintf(achDiagBuff, "%d.%d, ", i16WholePart, i16FracPart);
                break;

            case 1:
                sprintf(achDiagBuff, "%d.0%d, ", i16WholePart, i16FracPart);
                break;
        }
//        sprintf(achDiagBuff, "%d.%d, ", getInt(stDiagVal.fDataVal), getFract(stDiagVal.fDataVal));
        break;

    case DIAG_CHAR_TYPE:
        sprintf(achDiagBuff, "%d, ", stDiagVal.byDataVal);
        break;

    case DIAG_UINT8_TYPE:
        sprintf(achDiagBuff, "%d, ", stDiagVal.ubyDataVal);
        break;

    case DIAG_UINT_TYPE:
        sprintf(achDiagBuff, "%d, ", stDiagVal.u16DataVal);
        break;

    case DIAG_INT_TYPE:
        sprintf(achDiagBuff, "%d, ", stDiagVal.i16DataVal);
        break;

    case DIAG_BOOL_TYPE:
        sprintf(achDiagBuff, "%d, ", stDiagVal.bDataVal);
        break;

    case DIAG_TEXT_TYPE:
        sprintf(achDiagBuff, "%s, ", stDiagVal.chStrVal);
        break;
    }

    UART_putStringSerial(achDiagBuff);
    return 0;
}


/*
 * an enum data type, eDiagElement_t, holds all the diagnostics elements.
 */
stDiagDataStruc_t getDiagVal(eDiagElement_t eDiagElement)
{
    stDiagDataStruc_t stDiagVal;

    switch(eDiagElement)
    {
    case DIAG_PWM_0TO5_TEXT:
        strcpy(stDiagVal.chStrVal, "PwmCpuGpu: ");                        // sz<=TEXT_STR_SZ
        stDiagVal.eDataTypeVal = DIAG_TEXT_TYPE;
        break;

    case DIAG_PWM5:             // CPU fan
        stDiagVal.byDataVal    = TMR_PwmGetDcPercenatage(tb3, ccr1);    // PWM5 TB3.1, P6.0, Ch5
        stDiagVal.eDataTypeVal = DIAG_CHAR_TYPE;
        break;

    case DIAG_PWM4:             // GPU fan
        stDiagVal.byDataVal    = TMR_PwmGetDcPercenatage(tb3, ccr2);    // PWM4 TB3.2, P6.1, Ch4
        stDiagVal.eDataTypeVal = DIAG_CHAR_TYPE;
        break;

    case DIAG_FTACH_5and4_TEXT:
        strcpy(stDiagVal.chStrVal, "FtachPrt4_0,1: ");                        // sz<=TEXT_STR_SZ
        stDiagVal.eDataTypeVal = DIAG_TEXT_TYPE;
        break;

    case DIAG_CPU_FTACH5_RPM5:
        stDiagVal.u16DataVal = stFanTach[FAN_TACH5_P4_0].u16RpmPrevious;    // RPM0(TACH0)
        stDiagVal.eDataTypeVal = DIAG_UINT_TYPE;
        break;

    case DIAG_GPU_FTACH4_RPM4:
        stDiagVal.u16DataVal = stFanTach[FAN_TACH4_P4_1].u16RpmPrevious;    // RPM1(TACH1)
        stDiagVal.eDataTypeVal = DIAG_UINT_TYPE;
        break;

    case DIAG_TEMP_RTD_TEXT:
        strcpy(stDiagVal.chStrVal, "RtdAvgIntCpuGpu: ");      // sz<=TEXT_STR_SZ
        stDiagVal.eDataTypeVal = DIAG_TEXT_TYPE;
        break;

    case DIAG_RTD_TEMP_AVG:
        stDiagVal.fDataVal     = gfRtdTempAvg;
        stDiagVal.eDataTypeVal = DIAG_FLOAT_TYPE;
        break;

    case DIAG_ON_CHIP_CH12:
        stDiagVal.fDataVal     = stAdcChA12.fAdcXformVal;
        stDiagVal.eDataTypeVal = DIAG_FLOAT_TYPE;
        break;

    case DIAG_RTD_CH5:
        stDiagVal.fDataVal     = stAdcChA5.fAdcXformVal;    // PWM5 TB3.1, P6.0, Ch5
        stDiagVal.eDataTypeVal = DIAG_FLOAT_TYPE;
        break;

    case DIAG_RTD_CH4:
        stDiagVal.fDataVal     = stAdcChA4.fAdcXformVal;    // PWM4 TB3.2, P6.1, Ch4
        stDiagVal.eDataTypeVal = DIAG_FLOAT_TYPE;
        break;

    case DIAG_TEMP_ZONE_TEXT:
        strcpy(stDiagVal.chStrVal, "PwmActiveZn#CpuGpu: ");     // sz<=TEXT_STR_SZ
        stDiagVal.eDataTypeVal = DIAG_TEXT_TYPE;
        break;

    case DIAG_PWM5_ZONE_NUM:
        stDiagVal.ubyDataVal   = gubyCurrentTz[0];          // PWM5/CPU Fan
        stDiagVal.eDataTypeVal = DIAG_UINT8_TYPE;
        break;

    case DIAG_PWM4_ZONE_NUM:
        stDiagVal.ubyDataVal   = gubyCurrentTz[1];          // PWM4/GPU Fan
        stDiagVal.eDataTypeVal = DIAG_UINT8_TYPE;
        break;

    case DIAG_TEMP_RTD_ADC_TEXT:
        strcpy(stDiagVal.chStrVal, "RtdAdcIntCpuGpu: ");      // sz<=TEXT_STR_SZ
        stDiagVal.eDataTypeVal = DIAG_TEXT_TYPE;
        break;

    case DIAG_ON_CHIP_ADC_CH12:
        stDiagVal.u16DataVal   = stAdcChA12.u16AdcChVal;
        stDiagVal.eDataTypeVal = DIAG_UINT_TYPE;
        break;

    case DIAG_RTD_ADC_CH5:                                  // PWM5 TB3.1, P6.0, Ch5
        stDiagVal.u16DataVal   = stAdcChA5.u16AdcChVal;
        stDiagVal.eDataTypeVal = DIAG_UINT_TYPE;
        break;

    case DIAG_RTD_ADC_CH4:                                  // PWM4 TB3.2, P6.1, Ch4
        stDiagVal.u16DataVal   = stAdcChA4.u16AdcChVal;
        stDiagVal.eDataTypeVal = DIAG_UINT_TYPE;
        break;

    case DIAG_2RTD_ADC_SELF_CPY_TMP_TEXT:
        strcpy(stDiagVal.chStrVal, "TempSelfCpyCpuGpu: ");    // sz<=TEXT_STR_SZ
        stDiagVal.eDataTypeVal = DIAG_TEXT_TYPE;
        break;

    case DIAG_RTD_TEMP_CH4_0_CPU_SELF:
        stDiagVal.bDataVal     = stAdcChA5.bSelfTemp;
        stDiagVal.eDataTypeVal = DIAG_BOOL_TYPE;
        break;

    case DIAG_RTD_TEMP_CH4_1_GPU_SELF:
        stDiagVal.bDataVal     = stAdcChA4.bSelfTemp;
        stDiagVal.eDataTypeVal = DIAG_BOOL_TYPE;
        break;

    }
    return stDiagVal;
}

