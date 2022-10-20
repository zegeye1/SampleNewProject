/*
 * tmp1075.h
 *
 *  Created on: Mar 9, 2021
 *      Author: ZAlemu
 */

#ifndef TMP1075_H_
#define TMP1075_H_
#include "timer_utilities.h"

enum TMP1075_MSG_INDICE
{
    TMP1075_MSG0,
    TMP1075_MSG1,
    TMP1075_MSG2,
    TMP1075_MSG3,
    MAX_I2C_TMP1075_MESSAGES,
};
#define MAX_I2C_TMP1075_MSG_BYTE_CNT    (2)     // Tx and Rx Max Byte Data to be transferred
//#define MAX_I2C_TMP1075_MESSAGES        (4)     // # of I2C Sensor


typedef enum TMP1075_REGS
{
    TMP1075_PTR_VAL_4_TEMPERATURE,
    TMP1075_PTR_VAL_4_CONFIGURATION,
    TMP1075_PTR_VAL_4_LOW_LIMIT,
    TMP1075_PTR_VAL_4_HIGH_LIMIT,
    TMP1075_PTR_VAL_4_DEVICE_ID = 0x0F
}eTmp1075_Regs_t;

extern stTimerStruct_t stSwWatchDogTmp1075Tmr;

extern bool     bTestTzPwm;

extern uint8_t  gbyProcessI2cTmp1075MsgNum;
extern uint8_t  gbyI2cTmp1075MsgProcessedCnt;
extern uint32_t gi32I2cTmp1075MsgSwResetCnt;


extern stI2cTrasaction_t* pastTmp1075I2cMsgTable[];


extern volatile uint8_t abyTmp1075I2cTxBuff[][];
extern volatile uint8_t abyTmp1075I2cRxBuff[][];

extern stI2cTrasaction_t stTmp1075I2cMessage0;
extern stI2cTrasaction_t stTmp1075I2cMessage1;
extern stI2cTrasaction_t stTmp1075I2cMessage2;
extern stI2cTrasaction_t stTmp1075I2cMessage3;

void initI2cTmp1075TransactionsParams();
void loadTmp1075I2cMsgTable();
uint16_t loadAndStrtFirstTmp1075I2cMsgCb(stTimerStruct_t* myTimer);
void processTmp1075I2cMsg();
void xformTmp1075Adc2Temp();
float updateAverageTemp();
void testTzAndPwm();
uint16_t tmp1075I2cWatchDog(stTimerStruct_t* myTimer);


#endif /* TMP1075_H_ */
