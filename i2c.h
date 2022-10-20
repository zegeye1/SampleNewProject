/*
 * i2c.h
 *
 *  Created on: Feb 11, 2021
 *      Author: ZAlemu
 */

#ifndef I2C_H_
#define I2C_H_
#include "timer_utilities.h"

#define I2C_BAUD_RATE_1100HZ            (1100)
#define I2C_BAUD_RATE_1500HZ            (1500)
#define I2C_BAUD_RATE_2000HZ            (2000)
#define I2C_BAUD_RATE_10000HZ           (10000)
#define I2C_BAUD_RATE_20000HZ           (20000)


typedef enum I2C_SNSR_TYPE
{
    I2C_TEMP_SNSR_TMP1075,
    I2C_CURRENT_SNSR_INA219,
    I2C_LTC_V_AND_A_SNSR_LTC4151
}eI2cSnsrType_t;


typedef enum I2C_PERIPH
{
    I2C_0,
    I2C_1,
}eI2cPeripheral_t;


typedef enum I2C_STAT
{
    I2C_IDLE,
    I2C_BUSY,
    I2C_TRANSMITTING,
    I2C_RECEIVING,
    I2C_NAK,
    I2C_COMPLETE
}eI2cStat_t;


typedef enum TMP1075REGS
{
    TMP1075_TEMPERATURE,
    TMP1075_CONFIGURATION,
    TMP1075_LOWLIMIT,
    TMP1075_HIGHLIMIT,
    TMP1075_DEVICEID
}eTmp1075regs_t;


/*
 * for I2C message pertaining to TMP1075:
 *  write is always a single byte (write to a pointer) and
 *  read is always two bytes (12-bit actual data read as 2 bytes)
 */
typedef struct I2C_MESSAGE
{
    eI2cSnsrType_t   eI2cSnsrType;
    eI2cPeripheral_t eI2cInUse;
    uint8_t  ubySnsrStatus;
    uint8_t  ubyAddress;
    uint8_t  ubySnsrNumber;
    uint8_t  ubyTxByteCount;
    uint8_t  ubyTxByteCounter;
    volatile  uint8_t* pbyTxBuff;
    uint8_t  ubyRxByteCount;
    uint8_t  ubyRxByteCounter;
    volatile uint8_t* pbyRxBuff;
    eI2cStat_t  eStatus;
    void (*callback)();
    float    fI2cRead1stValue;
    float    fI2cRead2ndValue;
    float    fI2cRead1stValueSave;
    float    fI2cRead2ndValueSave;
}stI2cTrasaction_t;

extern stI2cTrasaction_t stI2cMessageActive;
extern stI2cTrasaction_t* pstI2cActiveMessage;

void initI2c(eI2cPeripheral_t eI2cNum, uint16_t ui16I2cBaudRate);
void deInitI2c(eI2cPeripheral_t eI2cNum);
void cfgPortForI2c();
void invokeStartCondition();
void invokeStopCondition();

void startI2cMsg(stI2cTrasaction_t* pstI2cMsg);


#endif /* I2C_H_ */
