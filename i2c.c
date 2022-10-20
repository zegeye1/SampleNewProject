/*
 * i2c.c
 *
 *  Created on: Feb 11, 2021
 *      Author: ZAlemu
 */

#include <msp430.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <intrinsics.h>
#include "config.h"
#include "i2c.h"
#include "tmp1075.h"
#include "timer_utilities.h"
#include "clocks.h"
#include "main.h"

// type of I2C sensor; TMP1075 or INA219, etc
eI2cSnsrType_t eSnsrActive;

// variable that holds the address of the current/active I2C message
stI2cTrasaction_t* pstI2cActiveMessage;

// active messages
stI2cTrasaction_t stI2cMessageActive;


/*
 * interrupts are enabled when about to start transactions and
 *  in the middle of transactions.
 */
void initI2c(eI2cPeripheral_t eI2cNum, uint16_t ui16I2cBaudRate)
{
    stDevClks_t stDeviceClks = getClockFreq();
    stI2cMessageActive.eI2cInUse = eI2cNum;

    cfgPortForI2c();
    if(eI2cNum == I2C_0)
    {
        UCB0CTL1  |= UCSWRST;                  // put eUSCI_B in reset state
        UCB0CTLW0 |= UCMODE_3 | UCMST | UCSYNC;   // I2C mode, master, synchronous
        UCB0BRW    = (uint16_t)(stDeviceClks.ui32SmclkHz / ui16I2cBaudRate);
        UCB0CTL1  &= ~UCSWRST;                  // release eUSCI_B from reset state
    }
    else
    {
        UCB1CTL1  |= UCSWRST;                  // put eUSCI_B in reset state
        UCB1CTLW0 |= UCMODE_3 | UCMST | UCSYNC;   // I2C mode, master, synchronous
        UCB1BRW    = (uint16_t)(stDeviceClks.ui32SmclkHz / ui16I2cBaudRate);
        UCB1CTL1  &= ~UCSWRST;                  // release eUSCI_B from reset state
    }

}

void startI2cMsg(stI2cTrasaction_t* pstI2cMsg)
{
    eSnsrActive = stI2cMessageActive.eI2cSnsrType;
    stI2cMessageActive.pbyRxBuff = pstI2cMsg->pbyRxBuff;
    stI2cMessageActive.pbyTxBuff = pstI2cMsg->pbyTxBuff;
    stI2cMessageActive.ubyAddress = pstI2cMsg->ubyAddress;
    stI2cMessageActive.ubyRxByteCount = pstI2cMsg->ubyRxByteCount;
    stI2cMessageActive.ubyTxByteCount = pstI2cMsg->ubyTxByteCount;
    // clear rx byte counters
    stI2cMessageActive.ubyRxByteCounter = 0;
    pstI2cMsg->ubyRxByteCounter = 0;
    // clear tx byte counters
    stI2cMessageActive.ubyTxByteCounter = 0;
    pstI2cMsg->ubyTxByteCounter = 0;
    stI2cMessageActive.eStatus = I2C_BUSY;
    pstI2cMsg->eStatus = I2C_BUSY;
    invokeStartCondition();
}


/*
 * invokeStartCondition() is called out of ISR when starting a new transaction.
 * that is, it is not used for a Repeat Start.
 * Repeat Start is invoked within ISR.
 * usually, if need to perform tx and rx transaction, you do the tx first.
 * reason is because you are writing to a pointer that will dictate where
 *  you read from.
 * however, if need to read with out writing, use ubyTxByteCount variable
 *  content to figure out if it is 0 or not. if 0 you directly read.
 */
void invokeStartCondition()
{
    _disable_interrupts();

    UCB0I2CSA = stI2cMessageActive.ubyAddress;

    if(stI2cMessageActive.eI2cInUse == I2C_0)
    {
        if(stI2cMessageActive.ubyTxByteCount == 0)
        {
            UCB0CTLW0 |= UCTR__RX + UCTXSTT;
            UCB0IE     = UCRXIE | UCNACKIE;
            stI2cMessageActive.eStatus = I2C_RECEIVING;
        }
        else
        {
            UCB0CTLW0 |= UCTR__TX + UCTXSTT;
            UCB0IE     = UCTXIE | UCNACKIE;
        }
    }
    else
    {
        if(stI2cMessageActive.ubyTxByteCount == 0)
        {
            UCB1CTLW0 |= UCTR__RX + UCTXSTT;
            UCB1IE    |= UCRXIE | UCNACKIE;
            stI2cMessageActive.eStatus = I2C_RECEIVING;
        }
        else
        {
            UCB1CTLW0 |= UCTXSTT;
            UCB1IE    |= UCTXIE | UCNACKIE;
        }
    }
    _enable_interrupts();
    stI2cMessageActive.eStatus = I2C_TRANSMITTING;
}



void invokeStopCondition()
{
    _disable_interrupts();
    if(stI2cMessageActive.eI2cInUse == I2C_0)
    {

        UCB0CTL1 |= UCTXSTP;    // Generate a STOP Condition
        // wait until bus is idle
        while(UCB0STATW &= UCBBUSY);
    }
    else
    {
        UCB1CTL1 |= UCTXSTP;    // Generate a STOP Condition
        // wait until bus is idle
        while(UCB1STATW &= UCBBUSY);
    }


    _enable_interrupts();
    stI2cMessageActive.eStatus = I2C_IDLE;
}


void cfgPortForI2c()
{
// configures ports P1.2(SDA) and P1.3(SCL) for eUSCI_B0 operation.
       /*
        *       Table 6-63. Port P1 Pin Functions
        * for (UCB0SIMO/UCB0SDA) function, P1DIR.x=2 = Don't Care and
        *  P1SEL2[1:0]=01b
        *
        * for *UCB0SOMI/UCB0SCL) function, P1DIR.x=3 = Don't Care and
        *  P1SEL3[1:0]=01b
        */
    if(stI2cMessageActive.eI2cInUse == I2C_0)
    {
        P1SEL0 |= (BIT2 | BIT3);
        P1SEL1 &= ~(BIT2 | BIT3);
    }
    else
    {
        P4SEL0 |= (BIT6 | BIT7);
        P4SEL1 &= ~(BIT6 | BIT7);
    }
}

void deInitI2c(eI2cPeripheral_t eI2cNum)
{
    if(eI2cNum == I2C_0)
    {
        // hold module in reset
        UCB0CTL1  |= UCSWRST;

        // config port for gpio usage
        P1SEL0 &= ~(BIT2 | BIT3);
        P1SEL1 &= ~(BIT2 | BIT3);
    }
    else
    {
        // hold module in reset
        UCB1CTL1  |= UCSWRST;
        // assign port for gpio usage
        P4SEL0 &= ~(BIT6 | BIT7);
        P4SEL1 &= ~(BIT6 | BIT7);
    }
}



#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = USCI_B0_VECTOR
__interrupt void USCIB0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_B0_VECTOR))) USCIB0_ISR (void)
#else
#error Compiler not supported!
#endif
{
  switch(__even_in_range(UCB0IV, USCI_I2C_UCBIT9IFG))
  {
    case USCI_NONE: break;                  // Vector 0: No interrupts
    case USCI_I2C_UCALIFG: break;           // Vector 2: ALIFG
    case USCI_I2C_UCNACKIFG:                // Vector 4: NACKIFG
/*
 * if transmit interrupt is not disabled, this ISR will be called again
 *  since flag remain set with the expectation of successful transmit transaction.
 * So clear transmit interrupt enable and set status to I2C_NAK; for the reason
 *  that got us here.
*/
        stI2cMessageActive.eStatus = I2C_NAK;
        pstI2cActiveMessage->eStatus = I2C_NAK;

        // clear both interrupt enables (these enables are always set when a START
        //  condition is enabled
        UCB0IE   &= ~(UCTXIE | UCNACKIE);

        // handle NAK anomaly; proccessI2cMsg() will retry one more time before
        //  moving on to the next message. Repeat START or STOP events are exercised
        //  by main
        gstMainEvts.bits.svcI2c = true;
        __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0

    case USCI_I2C_UCSTTIFG: break;          // Vector 6: STTIFG
    case USCI_I2C_UCSTPIFG: break;          // Vector 8: STPIFG
    case USCI_I2C_UCRXIFG3: break;          // Vector 10: RXIFG3 0x0a
    case USCI_I2C_UCTXIFG3: break;          // Vector 12: TXIFG3 0x0c
    case USCI_I2C_UCRXIFG2: break;          // Vector 14: RXIFG2 0x0e
    case USCI_I2C_UCTXIFG2: break;          // Vector 16: TXIFG2 0x10
    case USCI_I2C_UCRXIFG1: break;          // Vector 18: RXIFG1 0x12
    case USCI_I2C_UCTXIFG1: break;          // Vector 20: TXIFG1 0x14
    case USCI_I2C_UCRXIFG0:                 // Vector 22: RXIFG0 0x16     **** RX ***
        // Store the received byte in our buffer
        *stI2cMessageActive.pbyRxBuff++ = UCB0RXBUF;
        // First calculate how far till the end of our message, then increment the pointer/counter
        // ...for the next byte.  Finally, do what is necessary during the last two bytes
        switch (stI2cMessageActive.ubyRxByteCount - stI2cMessageActive.ubyRxByteCounter++)
        {
            case 2: // This was the second to last byte
                UCB0CTL1 |= UCTXSTP;            // Send stop bit
            break;

            case 1: // This was the last byte
                UCB0IE &= ~UCRXIE;             // DISABLE receive interrupt
                // record message receive counter status
                pstI2cActiveMessage->ubyRxByteCounter = stI2cMessageActive.ubyRxByteCounter - 1;
                stI2cMessageActive.eStatus = I2C_COMPLETE;
                gstMainEvts.bits.svcI2c = true;
                __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
            break;
        }
      break;

      case USCI_I2C_UCTXIFG0:               // Vector 24: T0IFG0 0x18     **** TX ***
          // Transmitter empty interrupt
          /*
           * the usual TMP1075 protocol performs a byte write to a pointer register with
           *  the offset/index of the register wanted to read and quickly follow up with
           *  a read using a repeat start.
           * this is usually the time this interrupt handler used here (to write a single
           *  byte to the pointer register.
           */
          if (stI2cMessageActive.ubyTxByteCounter++ < stI2cMessageActive.ubyTxByteCount)
          {
              stI2cMessageActive.eStatus = I2C_TRANSMITTING;
              // If more bytes to send, then send them
              UCB0TXBUF = *stI2cMessageActive.pbyTxBuff++;   // Load data byte
          }
          else
          {
              /*
               *  Last byte already sent; ready to close transaction or change direction
               *   and receive data if expecting to receive. need to perform a repeat start
               *   to configure module to be in a receive mode
               */
             UCB0IFG &= ~UCTXIE;            // DISABLE transmit interrupt

             // record correct message transmit counter status
             pstI2cActiveMessage->ubyTxByteCounter = stI2cMessageActive.ubyTxByteCounter - 1;

             // Anything to receive?
             if (stI2cMessageActive.ubyRxByteCount > 0)
             // Yes, switch to receive mode and do a restart
             {
                 UCB0CTL1 &= ~UCTR;            // Put us in receive mode
                 UCB0IE |= UCRXIE;            // ENABLE receive interrupts
                 UCB0CTL1 |= UCTXSTT;          // Send the start bit causing a restart
                 stI2cMessageActive.eStatus = I2C_RECEIVING;
             }
             else
             // transmit only transaction; end transaction
             {
                UCB0CTL1 |= UCTXSTP;        // Send stop bit
                pstI2cActiveMessage->ubyTxByteCounter = stI2cMessageActive.ubyRxByteCounter - 1;
                stI2cMessageActive.eStatus = I2C_COMPLETE;
                gstMainEvts.bits.svcI2c = true;
                __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
             }
          }
          break;

    case USCI_I2C_UCBCNTIFG:                // Vector 26: BCNTIFG 0x1a
    case USCI_I2C_UCCLTOIFG: break;         // Vector 28: clock low timeout 0x1c
    case USCI_I2C_UCBIT9IFG: break;         // Vector 30: 9th bit 0x1e
    default: break;
  }
}


#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = USCI_B1_VECTOR
__interrupt void USCIB1_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_B1_VECTOR))) USCIB1_ISR (void)
#else
#error Compiler not supported!
#endif
{
  switch(__even_in_range(UCB1IV, USCI_I2C_UCBIT9IFG))
  {
    case USCI_NONE: break;                  // Vector 0: No interrupts
    case USCI_I2C_UCALIFG: break;           // Vector 2: ALIFG
    case USCI_I2C_UCNACKIFG:                // Vector 4: NACKIFG
/*
 * if transmit interrupt is not disabled, this ISR will be called again
 *  since flag remain set with the expectation of successful transmit transaction.
 * So clear transmit interrupt enable and set status to I2C_NAK; for the reason
 *  that got us here.
*/
        stI2cMessageActive.eStatus   = I2C_NAK;
        pstI2cActiveMessage->eStatus = I2C_NAK;

        // clear both interrupt enables (these enables are always set when a START
        //  condition is enabled
        UCB1IE   &= ~(UCTXIE | UCNACKIE);

        // handle NAK anomaly; proccessI2cMsg() will retry one more time before
        //  moving on to the next message. Repeat START or STOP events are exercised
        //  by main
        gstMainEvts.bits.svcI2c = true;
        __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0

    case USCI_I2C_UCSTTIFG: break;          // Vector 6: STTIFG
    case USCI_I2C_UCSTPIFG: break;          // Vector 8: STPIFG
    case USCI_I2C_UCRXIFG3: break;          // Vector 10: RXIFG3 0x0a
    case USCI_I2C_UCTXIFG3: break;          // Vector 12: TXIFG3 0x0c
    case USCI_I2C_UCRXIFG2: break;          // Vector 14: RXIFG2 0x0e
    case USCI_I2C_UCTXIFG2: break;          // Vector 16: TXIFG2 0x10
    case USCI_I2C_UCRXIFG1: break;          // Vector 18: RXIFG1 0x12
    case USCI_I2C_UCTXIFG1: break;          // Vector 20: TXIFG1 0x14
    case USCI_I2C_UCRXIFG0:                 // Vector 22: RXIFG0 0x16     **** RX ***
        // Store the received byte in our buffer
        *stI2cMessageActive.pbyRxBuff++ = UCB1RXBUF;
        // First calculate how far till the end of our message, then increment the pointer/counter
        // ...for the next byte.  Finally, do what is necessary during the last two bytes
        switch (stI2cMessageActive.ubyRxByteCount - stI2cMessageActive.ubyRxByteCounter++)
        {
            case 2: // This was the second to last byte
                UCB1CTL1 |= UCTXSTP;            // Send stop bit
            break;

            case 1: // This was the last byte
                UCB1IE &= ~UCRXIE;             // DISABLE receive interrupt
                // record message receive counter status
                pstI2cActiveMessage->ubyRxByteCounter = stI2cMessageActive.ubyRxByteCounter - 1;
                stI2cMessageActive.eStatus = I2C_COMPLETE;
                gstMainEvts.bits.svcI2c = true;
                __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
            break;
        }
      break;

    case USCI_I2C_UCTXIFG0:               // Vector 24: T0IFG0 0x18     **** TX ***
        // Transmitter empty interrupt
        /*
         * the usual TMP1075 protocol performs a byte write to a pointer register with
         *  the offset/index of the register wanted to read and quickly follow up with
         *  a read using a repeat start.
         * this is usually the time this interrupt handler used here (to write a single
         *  byte to the pointer register.
         */
        if (stI2cMessageActive.ubyTxByteCounter++ < stI2cMessageActive.ubyTxByteCount)
        {
            stI2cMessageActive.eStatus = I2C_TRANSMITTING;
            // If more bytes to send, then send them
            UCB1TXBUF = *stI2cMessageActive.pbyTxBuff++;   // Load data byte
        }
        else
        {
           /*
            *  Last byte already sent; ready to close transaction or change direction
            *   and receive data if expecting to receive. need to perform a repeat start
            *   to configure module to be in a receive mode
            */
           UCB1IFG &= ~UCTXIE;            // DISABLE transmit interrupt

           // record correct message transmit counter status
           pstI2cActiveMessage->ubyTxByteCounter = stI2cMessageActive.ubyTxByteCounter - 1;

           // Anything to receive?
           if (stI2cMessageActive.ubyRxByteCount > 0)
           // Yes, switch to receive mode and do a restart
           {
               UCB1CTL1 &= ~UCTR;           // Put us in receive mode
               UCB1IE   |= UCRXIE;          // ENABLE receive interrupts
               UCB1CTL1 |= UCTXSTT;         // Send the start bit causing a restart
               stI2cMessageActive.eStatus = I2C_RECEIVING;
           }
           else
           // transmit only transaction; end transaction
           {
              UCB1CTL1  |= UCTXSTP;         // Send stop bit
              pstI2cActiveMessage->ubyTxByteCounter = stI2cMessageActive.ubyRxByteCounter - 1;
              stI2cMessageActive.eStatus = I2C_COMPLETE;
              gstMainEvts.bits.svcI2c = true;
              __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
           }
        }
        break;

    case USCI_I2C_UCBCNTIFG:                // Vector 26: BCNTIFG 0x1a
    case USCI_I2C_UCCLTOIFG: break;         // Vector 28: clock low timeout 0x1c
    case USCI_I2C_UCBIT9IFG: break;         // Vector 30: 9th bit 0x1e
    default: break;
  }
}
