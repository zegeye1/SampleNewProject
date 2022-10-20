/*
 * uart.h
 *
 *  Created on: Jan 29, 2021
 *      Author: ZAlemu
 */

#ifndef UART_H_
#define UART_H_
#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>

#define UART_ENABLE_RXINT       (true)
#define UART_DISABLE_RXINT      (false)
#define UART_ENABLE_TXINT       (true)
#define UART_DISABLE_TXINT      (false)


#define UART_RELEASE_FROM_RST   (0)
#define UART_HOLD_IN_RST        (1)
#define UART_ENABLE_RX_INT      (UCRXIE)
#define UART_ENABLE_TX_INT      (UCTXIE)


typedef enum UART_CLK
{
    UART_CLK_UCLK  = UCSSEL__UCLK,
    UART_CLK_SMCLK = UCSSEL__SMCLK
}eUartClk_t;


typedef enum UART_NUMBER
{
    UART_A0,
    UART_A1
}eUartNum_t;


// since baud rate is used by diagnostics, in addition to
//  configuration, need to tag the baud rate used within
//  config.h
typedef enum UART_BAUD
{
    UART_BAUD_9600   = 9600,
    UART_BAUD_19200  = 19200,
    UART_BAUD_38400  = 38400,
    UART_BAUD_57600  = 57600,
    UART_BAUD_115200 = 115200
}eUartBaud_t;


typedef struct UART_GRP_REGS_ADDRESS
{
    volatile uint16_t* pUartCtrlWord0;
    volatile uint16_t* pUartCtrlWord1;
    volatile uint16_t* pUartBaudRateWord;
    volatile uint16_t* pUartModulationCntrlWord;

    volatile uint16_t* pUartStatus;
    volatile uint16_t* pUartRxBuffReg;
    volatile uint16_t* pUartTxBuffReg;

    volatile uint16_t* pUartIntEnable;
    volatile uint16_t* pUartIntFlag;
}UART_GrpRegsAddress_t;

extern char achUartInputBuffer[];
extern char achUartOutputBuffer[];
extern uint8_t ubyUrtInBuffLdrIndx;
extern uint8_t ubyUrtInBuffUnLdIndx;
extern uint8_t ubyUrtOutBuffLdrIndx;
extern uint8_t ubyUrtOutBuffUnLdrIndx;

void UART_GetUartRegsAddress(eUartNum_t eUartNum);
void UART_CfgPortForUartUsage(eUartNum_t eUartNum);
void UART_CfgUart(eUartNum_t eUartNum, eUartClk_t eUartClk,
                  eUartBaud_t eBaudRate, bool bRxInt, bool bTxInt);
void UART_CfgUartCtrlWord0(eUartClk_t eUartClk);
void UART_CfgUartBaud(uint32_t ui32BaudRate);
void UART_EnableDisableRxInt(bool bEnableDisabeRxInt);
void UART_EnableDisableTxInt(bool bEnableDisabeTxInt);
void UART_HoldReleaseFromRst(bool bRelHold);

void UART_svcUartRx(void);
void UART_svcUartTx(void);
void UART_echoCharacter(void);
void UART_putStringSerial(char *string);
void UART_testTransmit(void);
void UART_printNewLineAndPrompt(void);
void UART_printNewLine(void);
void UART_printPrompt(void);


extern UART_GrpRegsAddress_t stUartRegsAddress;


#endif /* UART_H_ */
