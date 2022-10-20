#include <msp430.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <intrinsics.h>
#include "clocks.h"
#include "main.h"
#include "uart.h"
#include "cli.h"

/*
 * uart.c
 * MSP430FR2355 has 2 of them
 * eUSCI_A0 and eUSCI_A1
 *   Pin Out:
 *   eUSCI_A0   P1.6 UCA0RXD - Pin 32
 *              P1.7 UCA0TXD - Pin 31
 *
 *   eUSCI_A1   P4.2 UCA1RXD - Pin 24
 *              P4.3 UCA1TXD - Pin 23
 *
 */

char achUartInputBuffer[SERIAL_BUFFER_SZ];
char achUartOutputBuffer[SERIAL_BUFFER_SZ];
uint8_t ubyUrtInBuffLdrIndx;
uint8_t ubyUrtInBuffUnLdIndx;
uint8_t ubyUrtOutBuffLdrIndx;
uint8_t ubyUrtOutBuffUnLdrIndx;

uint16_t ui16RcvStatus;

bool    bUartXmitBusy = true;

UART_GrpRegsAddress_t stUartRegsAddress;

stDevClks_t stCurrentClkFreq;


void deInitUart(eUartNum_t eUartNum)
{
    // disable UART interrupt
    UART_EnableDisableRxInt(UART_DISABLE_RXINT);
    UART_EnableDisableRxInt(UART_DISABLE_TXINT);

    // hold UART in reset
    UART_HoldReleaseFromRst(UART_HOLD_IN_RST);

    // reconfigure port for gpio usage
    switch(eUartNum)
    {
    case UART_A0:
        // Ports 1.6 and 1.7 are used by eUSCI_A0 (P1SEL0_6/7 = 1/1)
        P1SEL0 &= ~(BIT6 | BIT7);
        P1SEL1 &= ~(BIT6 | BIT7);
        break;

    case UART_A1:
        // Ports 4.2(Rx) and 4.3(Tx) are used by eUSCI_A1 (P4SEL0_2/3 = 1/1)
        P4SEL0 &= ~(BIT2 | BIT3);
        P4SEL1 &= ~(BIT2 | BIT3);
        break;
    }
}

void UART_CfgUart(eUartNum_t eUartNum, eUartClk_t eUartClk,
                  eUartBaud_t eBaudRate, bool bRxInt, bool bTxInt)
{
    // get uart regs address
    UART_GetUartRegsAddress(eUartNum);
    UART_HoldReleaseFromRst(UART_HOLD_IN_RST);
    UART_CfgUartCtrlWord0(eUartClk);
    UART_CfgUartBaud(eBaudRate);
    UART_HoldReleaseFromRst(UART_RELEASE_FROM_RST);
    // must release from reset prior to configuring int enable
    UART_CfgPortForUartUsage(eUartNum);
    UART_EnableDisableRxInt(bRxInt);
    UART_EnableDisableTxInt(bTxInt);

    // for baud rate cfg, need to know input clk freq
    stCurrentClkFreq = getClockFreq();
}



void UART_CfgPortForUartUsage(eUartNum_t eUartNum)
{
    switch(eUartNum)
    {
    case UART_A0:
        // Ports 1.6 and 1.7 are used by eUSCI_A0 (P1SEL0_6/7 = 1/1)
        P1SEL0 |= (BIT6 | BIT7);
        P1SEL1 &= ~(BIT6 | BIT7);
        break;

    case UART_A1:
        // Ports 4.2(Rx) and 4.3(Tx) are used by eUSCI_A1 (P4SEL0_2/3 = 1/1)
        P4SEL0 |= (BIT2 | BIT3);
        P4SEL1 &= ~(BIT2 | BIT3);
        break;
    }
}



void UART_EnableDisableRxInt(bool bEnableDisabeRxInt)
{
    if(bEnableDisabeRxInt)
    {
        *stUartRegsAddress.pUartIntEnable |= UCRXIE;
    }
    else
    {
        *stUartRegsAddress.pUartIntEnable &= ~UART_ENABLE_RX_INT;

    }
}


void UART_EnableDisableTxInt(bool bEnableDisabeTxInt)
{
    if(bEnableDisabeTxInt)
    {
        *stUartRegsAddress.pUartIntEnable |= UCTXIE;
    }
    else
    {
        *stUartRegsAddress.pUartIntEnable &= ~UART_ENABLE_TX_INT;

    }
}



void UART_CfgUartCtrlWord0(eUartClk_t eUartClk)
{
    *stUartRegsAddress.pUartCtrlWord0 &= ~UCSSEL;
    *stUartRegsAddress.pUartCtrlWord0 |= eUartClk;
}


void UART_HoldReleaseFromRst(bool bRelHold)
{
    if(bRelHold)
    {
        *stUartRegsAddress.pUartCtrlWord0 |= UART_HOLD_IN_RST;
    }
    else
    {
        *stUartRegsAddress.pUartCtrlWord0 &= ~UART_HOLD_IN_RST;
    }
}


// values, extracted from 'slau445i, Table 22-5'
/*
 * these values pertain to the use of 8MHz SMCLK frequency as
 *  input to the UART module.
 */
void UART_CfgUartBaud(eUartBaud_t eUartBaud)
{
    if (stCurrentClkFreq.ui32SmclkHz == 8000000)
    {
        switch (eUartBaud)
        {
        case UART_BAUD_9600:
            *stUartRegsAddress.pUartBaudRateWord        = 52;
            *stUartRegsAddress.pUartModulationCntrlWord = 0x4911;
        //            UCA0BR0 = 52;
        //            UCA0MCTLW = 0x4911;
            break;
        case UART_BAUD_19200:
            *stUartRegsAddress.pUartBaudRateWord        = 26;
            *stUartRegsAddress.pUartModulationCntrlWord = 0xB601;
        //            UCA0BR0 = 26;
        //            UCA0MCTLW = 0xB601;
            break;
        case UART_BAUD_38400:
            *stUartRegsAddress.pUartBaudRateWord         = 13;
            *stUartRegsAddress.pUartModulationCntrlWord = 0x8401;
        //            UCA0BR0 = 13;
        //            UCA0MCTLW = 0x8401;
            break;
        case UART_BAUD_57600:
            *stUartRegsAddress.pUartBaudRateWord        = 8;
            *stUartRegsAddress.pUartModulationCntrlWord = 0xF7A1;
        //            UCA0BR0 = 8;
        //            UCA0MCTLW = 0xF7A1;
            break;
        case UART_BAUD_115200:
            *stUartRegsAddress.pUartBaudRateWord        = 4;
            *stUartRegsAddress.pUartModulationCntrlWord = 0x5551;
        //            UCA0BR0 = 4;
        //            UCA0MCTLW = 0x5551;
            break;
        }

    }
}



// Get UART Register Addresses for either eUSCI_A0 or eUSCI_A1
void UART_GetUartRegsAddress(eUartNum_t eUartNum)
{
    switch(eUartNum)
    {
        case(UART_A0):
            stUartRegsAddress.pUartCtrlWord0           = &UCA0CTLW0;
            stUartRegsAddress.pUartCtrlWord1           = &UCA0CTLW1;
            stUartRegsAddress.pUartBaudRateWord        = &UCA0BRW;
            stUartRegsAddress.pUartModulationCntrlWord = &UCA0MCTLW;

            stUartRegsAddress.pUartStatus              = &UCA0STATW;
            stUartRegsAddress.pUartRxBuffReg           = &UCA0RXBUF;
            stUartRegsAddress.pUartTxBuffReg           = &UCA0TXBUF;

            stUartRegsAddress.pUartIntEnable           = &UCA0IE;
            stUartRegsAddress.pUartIntFlag             = &UCA0IFG;
            break;

        case(UART_A1):
            stUartRegsAddress.pUartCtrlWord0           = &UCA1CTLW0;
            stUartRegsAddress.pUartCtrlWord1           = &UCA1CTLW1;
            stUartRegsAddress.pUartBaudRateWord        = &UCA1BRW;
            stUartRegsAddress.pUartModulationCntrlWord = &UCA1MCTLW;

            stUartRegsAddress.pUartStatus              = &UCA1STATW;
            stUartRegsAddress.pUartRxBuffReg           = &UCA1RXBUF;
            stUartRegsAddress.pUartTxBuffReg           = &UCA1TXBUF;

            stUartRegsAddress.pUartIntEnable           = &UCA1IE;
            stUartRegsAddress.pUartIntFlag             = &UCA1IFG;
            break;
    }
}



void UART_svcUartRx(void)
{
    char chRcvd;

    gstMainEvts.bits.svcUartRx = false;

    // get a copy of just received char for convenience
    chRcvd = achUartInputBuffer[ubyUrtInBuffLdrIndx];

    /*
     * when handling backspace you will need to:
     * 1. move cursor back 1 character
     * 2. over-write a space onto the character you are deleting
     *    this write would move the cursor past the 'space' written
     * 3. move back the cursor one character back, the space character.
     *
     * now you are ready to enter a new character or
     *  proceed on repeating the backspace action to delete more
     *  characters and writing spaces.
     */
    // check for backspace
    if(chRcvd == '\b')
    {
        UART_putStringSerial("\b \b");
        ubyUrtInBuffLdrIndx--;
        return;
    }

    UART_echoCharacter();


    // if received CR or LF (Enter) replace ch with end of string; NULL
    if(chRcvd == '\r' || chRcvd == '\n')
    {
        achUartInputBuffer[ubyUrtInBuffLdrIndx++] = NULL;
        UART_EnableDisableRxInt(UART_DISABLE_RXINT);
        enableDisableTimer(&gstCopySeralCmdToCmdBuffTmr, TMR_ENABLE);
    }
    else
    {
        // increment loader index
        ubyUrtInBuffLdrIndx++;
    }

    /*
     * Ideal case: handle received data so that over run does
     *             not occur.
     * Overrun:    if overrun occur, overwrite last character
     * Overrun indicator:
     *             if InputIndex == OutputIndex
     */

    if (ubyUrtInBuffLdrIndx >= SERIAL_BUFFER_SZ)
    {
        ubyUrtInBuffLdrIndx = 0;
    }

    // handle over run case
    if (ubyUrtInBuffLdrIndx == ubyUrtInBuffUnLdIndx)
    {
        if(ubyUrtInBuffUnLdIndx == 0)
        {
            ubyUrtInBuffLdrIndx = SERIAL_BUFFER_SZ -1;
        }
        else
        {
            ubyUrtInBuffLdrIndx--;
        }
    }
}



void UART_echoCharacter(void)
{
    char chRcvd;

    chRcvd = achUartInputBuffer[ubyUrtInBuffLdrIndx];

    // load rcv'd char into output buffer (echo)
    // if received CR or LF (Enter) replace ch with end of string; NULL
    if(chRcvd == '\r' || chRcvd == '\n')
    {
        // schedule to output new line
        achUartOutputBuffer[ubyUrtOutBuffLdrIndx++] = '\r';
        achUartOutputBuffer[ubyUrtOutBuffLdrIndx++] = '\n';
    }
    else
    {
        // output received data
        achUartOutputBuffer[ubyUrtOutBuffLdrIndx++] = chRcvd;
    }

    // handler output index boundary
    if (ubyUrtOutBuffLdrIndx >= SERIAL_BUFFER_SZ)
    {
        ubyUrtOutBuffLdrIndx = 0;
    }

    // if transmitter is idle, start a tx transaction here
    // else ISR will transmit it when ready
    if (bUartXmitBusy == false)
    {
        bUartXmitBusy = true;
        *stUartRegsAddress.pUartTxBuffReg = achUartOutputBuffer[ubyUrtOutBuffUnLdrIndx++];
        if(ubyUrtOutBuffUnLdrIndx >= SERIAL_BUFFER_SZ)
        {
            ubyUrtOutBuffUnLdrIndx = 0;
        }
    }
}


void UART_svcUartTx(void)
{
    gstMainEvts.bits.svcUartTx = false;

    // if data is in UART transmit buffer send
    // else, if last sent data generated int, mark UART Xmiter is idle
    if (ubyUrtOutBuffLdrIndx != ubyUrtOutBuffUnLdrIndx)
    {
        *stUartRegsAddress.pUartTxBuffReg = achUartOutputBuffer[ubyUrtOutBuffUnLdrIndx++];
        if(ubyUrtOutBuffUnLdrIndx >= SERIAL_BUFFER_SZ)
        {
            ubyUrtOutBuffUnLdrIndx = 0;
        }
    }
    else
    {
        bUartXmitBusy = false;
    }
}


void UART_putStringSerial(char *string)  // Blocking
{
    // Block while we don't have room for the string
    do
    {
        __no_operation();
    } while (bUartXmitBusy && (ubyUrtOutBuffLdrIndx == ubyUrtOutBuffUnLdrIndx)) ;

    __disable_interrupt();
    while ( *string != 0 )
    {
        achUartOutputBuffer[ubyUrtOutBuffLdrIndx++] = *string++;
        // if run out of transmit buffer, overwrite the last char

        if(ubyUrtOutBuffLdrIndx == ubyUrtOutBuffUnLdrIndx)
        {
        // handle over run case
            if(ubyUrtOutBuffUnLdrIndx == 0)
            {
                ubyUrtOutBuffLdrIndx = SERIAL_BUFFER_SZ -1;
            }
            else
            {
                ubyUrtOutBuffLdrIndx--;
            }
        }
    }

    /*
     * txIntsStopped will be set to TRUE by ISR when completed transmitting
     *  all pending chars from OutBuff. If ISR is still in a middle of a
     *  transfer, the content of this semaphore, txIntsStopped, remain FALSE.
     *  this function, when reaching here need do nothing if ISR is in a
     *  middle of a transfer.
     * ISR will continue to transmit out all chars, including the current
     *  payload just loaded, since the end of a transfer is identified by
     *  the state of the indices.
     */

    // If we aren't expecting any more TX interrupts, send the
    // character now to prime the pump
    if (!bUartXmitBusy)
    {
        bUartXmitBusy = true;
        *stUartRegsAddress.pUartTxBuffReg = achUartOutputBuffer[ubyUrtOutBuffUnLdrIndx++];

        if (ubyUrtOutBuffUnLdrIndx >= SERIAL_BUFFER_SZ)
        {
            ubyUrtOutBuffUnLdrIndx = 0;
        }
    }
    __enable_interrupt();
}


void UART_testTransmit(void)
{
    char     achStringBuff[128];
    uint16_t wTempVar = 1234;
//    uint32_t lwTempVar = 98763;
    gstMainEvts.bits.svcTestUartTx = false;
    sprintf (achStringBuff, "TemVar value is: %d.", wTempVar);
    UART_putStringSerial(achStringBuff);
    UART_printNewLineAndPrompt();
}


void UART_printNewLine(void)
{
    char achStringBuff[4];
    achStringBuff[0] = '\n';
    achStringBuff[1] = '\r';
    achStringBuff[2] = '\0';
    UART_putStringSerial(achStringBuff);
}


void UART_printNewLineAndPrompt(void)
{
    UART_printNewLine();
    UART_putStringSerial(">>");
}
/*
 * #pragma associates the ISR handler with the Interrupt Vector, i.e.,
 *   places the ISR address into the Vector table
 * __interrupt key word allows the compiler to save the content of the
 *   CPU before starting to execute the ISR code. It also tells the
 *   compiler to restore saved content when exiting ISR.
 */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A0_VECTOR))) USCI_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
    switch(__even_in_range(UCA0IV,USCI_UART_UCTXCPTIFG))
    {
        case USCI_NONE: break;      // case 0

        case USCI_UART_UCRXIFG:     // case 2 // Receive buffer empty
            // store error condition before being cleared by reading the receive buffer
            // currently not making use of it.
            ui16RcvStatus = *stUartRegsAddress.pUartStatus;

            // UCRXIFG is automatically reset when UCAxRXBUF is read.
            // don't inc; might need to update ch if ch recv'd is '\r' or '\n'
            achUartInputBuffer[ubyUrtInBuffLdrIndx] = *stUartRegsAddress.pUartRxBuffReg;
            gstMainEvts.bits.svcUartRx = true;
            __bic_SR_register_on_exit(LPM0_bits);
            break;


        case USCI_UART_UCTXIFG:     //case 4 // Transmit buffer empty
            // handle transaction from main
            if(ubyUrtOutBuffLdrIndx == ubyUrtOutBuffUnLdrIndx)
            {
                bUartXmitBusy = false;
            }
            else
            {
                gstMainEvts.bits.svcUartTx = true;
            }
            __bic_SR_register_on_exit(LPM0_bits);

            break;

        case USCI_UART_UCSTTIFG:    // case 6
            break;      // Start byte received

        case USCI_UART_UCTXCPTIFG:  // case 8
            break;      // Transmit complete
    }
}


/*
 * #pragma associates the ISR handler with the Interrupt Vector, i.e.,
 *   places the ISR address into the Vector table
 * __interrupt key word allows the compiler to save the content of the
 *   CPU before starting to execute the ISR code. It also tells the
 *   compiler to restore saved content when exiting ISR.
 */
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A1_VECTOR))) USCI_A1_ISR (void)
#else
#error Compiler not supported!
#endif
{

    switch(__even_in_range(UCA1IV,USCI_UART_UCTXCPTIFG))
        {
        case USCI_NONE: break;      // case 0

        case USCI_UART_UCRXIFG:     // case 2 // Receive buffer empty
            // store error condition before being cleared by reading the receive buffer
            // currently not making use of it.
            ui16RcvStatus = *stUartRegsAddress.pUartStatus;

            // UCRXIFG is automatically reset when UCAxRXBUF is read.
            // don't inc; might need to update ch if ch recv'd is '\r' or '\n'
            achUartInputBuffer[ubyUrtInBuffLdrIndx] = *stUartRegsAddress.pUartRxBuffReg;
            gstMainEvts.bits.svcUartRx = true;
            __bic_SR_register_on_exit(LPM0_bits);
            break;


        case USCI_UART_UCTXIFG:     //case 4 // Transmit buffer empty
            // handle transaction from main
            if(ubyUrtOutBuffLdrIndx == ubyUrtOutBuffUnLdrIndx)
            {
                bUartXmitBusy = false;
            }
            else
            {
                gstMainEvts.bits.svcUartTx = true;
            }
            __bic_SR_register_on_exit(LPM0_bits);

            break;

        case USCI_UART_UCSTTIFG:    // case 6
            break;      // Start byte received

        case USCI_UART_UCTXCPTIFG:  // case 8
            break;      // Transmit complete
    }
}

