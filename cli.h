/*
 * cli.h
 *
 *  Created on: Feb 1, 2021
 *      Author: ZAlemu
 */

#ifndef CLI_H_
#define CLI_H_
#include "timer_utilities.h"

#define SERIAL_BUFFER_SZ        256
#define NUM_CMDS                4
#define MAX_CMD_LENGTH          10   // total # of tokens making a command
#define TEXT_STR_SZ             25


typedef enum DIAG_ELEMENT
{
    DIAG_START,
    DIAG_PWM_0TO5_TEXT = DIAG_START,
    DIAG_PWM5,              // CPU fan
    DIAG_PWM4,              // GPU fan

    DIAG_FTACH_5and4_TEXT,
    DIAG_CPU_FTACH5_RPM5,   // PWM5, CPU fans RPMs
    DIAG_GPU_FTACH4_RPM4,   // PWM4, GPU fan RPM

    DIAG_TEMP_RTD_TEXT,
    DIAG_RTD_TEMP_AVG,
    DIAG_ON_CHIP_CH12,      // internal sensor
    DIAG_RTD_CH5,           // PWM5, sensor by CPU
    DIAG_RTD_CH4,           // PWM4, sensor by GPU

    DIAG_TEMP_ZONE_TEXT,
    DIAG_PWM5_ZONE_NUM,
    DIAG_PWM4_ZONE_NUM,

    DIAG_TEMP_RTD_ADC_TEXT,
    DIAG_ON_CHIP_ADC_CH12,
    DIAG_RTD_ADC_CH5,
    DIAG_RTD_ADC_CH4,

    DIAG_2RTD_ADC_SELF_CPY_TMP_TEXT,
    DIAG_RTD_TEMP_CH4_0_CPU_SELF,
    DIAG_RTD_TEMP_CH4_1_GPU_SELF,
    DIAG_ELEMENT_COUNT,
    DIAG_STOP = DIAG_ELEMENT_COUNT
}eDiagElement_t;


typedef enum DIAG_DATA_TYPE
{
    DIAG_UINT8_TYPE,
    DIAG_CHAR_TYPE,
    DIAG_INT_TYPE,
    DIAG_UINT_TYPE,
    DIAG_LONG_INT_TYPE,
    DIAG_FLOAT_TYPE,
    DIAG_BOOL_TYPE,
    DIAG_TEXT_TYPE,

    DIAG_TYPE_CNT
}eDiagDataType_t;


typedef enum DIAG_TMR_STATE
{
    DIAG_UPDATE_NONE,
    DIAG_ENABLE_UPDATE_TMR,
    DIAG_MODIFY_DIAG_TMR,
    DIAG_RESET_DIAG_TMR,
    DIAG_DEREGISTER_DIAG_TMR,
    DIAG_DEREGISTER_UPDATE_TMR,
}eDiagTmrState_t;



typedef struct DIAG_DATA_STRUC
{
    bool            bDataVal;
    char            byDataVal;
    uint8_t         ubyDataVal;
    uint16_t        u16DataVal;
    uint32_t        u32DataVal;
    int16_t         i16DataVal;
    float           fDataVal;
    // short 2 string response; eg HON
    char            chStrVal[TEXT_STR_SZ];
    eDiagDataType_t eDataTypeVal;

}stDiagDataStruc_t;


extern bool gbDiagTimeoutChanged;;
extern eDiagTmrState_t geDiagTmrUpdateState;
extern stTimerStruct_t gstCopySeralCmdToCmdBuffTmr;
//extern stTimerStruct_t stSerialCmdMakeTokensTmr;
//extern stTimerStruct_t stSerialCmdTokenExecuteTmr;
//extern stTimerStruct_t stSerialCmdTokenExecuteTmr;
//extern stTimerStruct_t stDiagnosticsDataTmr;

void getCMD();
void setCMD();
void updateCMD();
void manCMD();

void initCli();
uint16_t cpySerialCmd2CmdBuf(stTimerStruct_t* myTimer);
uint16_t executeUartCmd(stTimerStruct_t* myTimer);
uint16_t makeTokens(stTimerStruct_t* myTimer);
uint16_t outputDiagData(stTimerStruct_t* myTimer);
uint16_t updateDiagTimer(stTimerStruct_t* myTimer);


void enableDiagnostics(void);
void disableDiagnostics(void);
uint16_t printDiagElement(eDiagElement_t eDiagElement);
stDiagDataStruc_t getDiagVal(eDiagElement_t eDiagElement);
void resetDiagTimeout();
void updateDiagTimeout(uint16_t u16Val);


void disableDiagUpdateTimer(void);
void maintainDiagMsgDisplay(void);
uint16_t updateCmdCb(stTimerStruct_t* myTimer);

#endif /* CLI_H_ */
