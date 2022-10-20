/*
 *
 */
//***************************************************************************************

#include <msp430.h>
#include <stdio.h>
#include <stdbool.h>
#include "revisions.h"
#include "main.h"
#include "config.h"
#include "timer.h"
#include "timer_utilities.h"
#include "uart.h"
#include "cli.h"
#include "i2c.h"
#include "adc.h"
#include "rtd.h"
#include "tmp1075.h"
#include "fans.h"
#include "thermalcontrol.h"

volatile stMainEvts_t gstMainEvts;

void cfgLnchPadHrtBeatP1pin0(); // Launch Board LED
void cfgLnchPadHrtBitP1pin1();  // Fan Controller LED
stDevClks_t stCurrentClkFreq;
extern uint16_t ui16IncVar;

uint16_t uiAdcChData = 0xFFFF;

stTimerStruct_t stDispBannerAtStrtUpTmr =
{
    .prevTimer  = NULL,
    .timeoutTickCnt  = 1,
    .counter    = 0,
    .recurrence = TIMER_SINGLE,
    .status     = TIMER_RUNNING,
    .callback   = displayBannerCb,
    .nextTimer  = NULL
};

uint16_t displayBannerCb(stTimerStruct_t* myTimer)
{
    char achStringBuff[100];
    UART_printNewLine();
    sprintf (achStringBuff, ">> *** %s %s %s ***", FIRMWARE_ID, RELEASE_UPDATE, DATE_COMMIT);
    UART_putStringSerial(achStringBuff);
    UART_printNewLineAndPrompt();
    return 0;
}

/*
 * LaunchPad Port Pin 1.0 has an LED connected to it.
 * launchPadHeartBitToggle() handles the toggling state of P1.0
 */
void cfgLnchPadHrtBeatP1pin0()
{
    // PORT1 pins -------------------------
    P1DIR  |= BIT0;         // configure P1.0 as output
    // P1SELx=00b           select P1.0 for GPIO use
    P1SEL0 &= ~BIT0;
    P1SEL1 &= ~BIT0;
}

void cfgFancControllerHrtBeatP1pin1()
{
    // PORT1 pins -------------------------
    P1DIR  |= BIT1;         // configure P1.1 as output
    // P1SELx=00b           select P1.0 for GPIO use
    P1SEL0 &= ~BIT1;
    P1SEL1 &= ~BIT1;
}


int main(void)
{

    WD_DisableWdog();

    // Disable the GPIO power-on default high-impedance mode
    // in order to allow further pin setting to take place
    PM5CTL0 &= ~LOCKLPM5;

// ---------------- CLOCKING --------------------------------------

    // configure const value, MHZCLK with the desired feq in config.h
    CS_CfgClkApi(MHZCLK);

    // get current clocks frequencies
    // check if frequencies matched expectation
    stCurrentClkFreq = getClockFreq();

// ----------------------------------------------------------------


// ---------------- timer -----------------------------------------
    // cfg tick
    TMR_CfgTimerBxTick(TMR_TICKER_TIMER, TMR_TICKER_PERIOD);

// ----------------------------------------------------------------


// ---------------- UART ------------------------------------------
// can not test UART_A1 because P4.2 and P4.3 are not bonded out
//  on the MSP430FR2355 Launchboard.
    UART_CfgUart(UART_A0,
                 UART_CLK_SMCLK,
                 UART_BAUD_IN_USE,
                 UART_ENABLE_RXINT,
                 UART_ENABLE_TXINT);

// ----------------------------------------------------------------
    // normally, want to do this after initCli().
    // so long as the timeout value is long enough that initCli() has executed
    //  it should be OK.
    // kept this here to demonstrate that these function calls have to do
    //  with instantiating the UART.
    registerTimer(&stDispBannerAtStrtUpTmr);
    enableDisableTimer(&stDispBannerAtStrtUpTmr, TMR_ENABLE);

// ---------------- CLI -------------------------------------------

    initCli();
// ----------------------------------------------------------------

// ---------------- Thermal Control / PWMs / FANs -----------------

    initThermalControl();
    // selects PWM timer, configures timer, tachs, interrupts,
    //  RPM calc interval & register and enable RPM timer
    initFans();
// ----------------------------------------------------------------


#if 0
// ---------------- I2C -------------------------------------------

    /*
     * run temp sensor at a lower freq, standard mode. it's sensitive to
     *  higher frequencies, even though it is manufactured to operate at
     *  fast (400K) to high speed (1Meg)
     * msp430 i2c operates at fast speed
     * min fast mode freq from the temp sensor data sheet is 0.001 MHz = 1000 Hz
     * min delta between stop and start of the temp sensor is 1.3uS
     */
    initI2c(I2C_0, I2C_BAUD_IN_USE);
    initI2cTmp1075TransactionsParams();

    // enable sw watchdog timer
    registerTimer(&stSwWatchDogTmp1075Tmr);
    enableDisableTimer(&stSwWatchDogTmp1075Tmr, TMR_ENABLE);
// ----------------------------------------------------------------
#endif


// ---------------- ADC -------------------------------------------

    initRtd();

    // enable sw watchdog timer; will trigger periodically by setting ADCCT0:ADCSC
    registerTimer(&stAdcAcquistionTmr);
    enableDisableTimer(&stAdcAcquistionTmr, TMR_ENABLE);
// ----------------------------------------------------------------

// ----------------------------------------------------------------

    // Heart beat LED Controller GPIO config: usage: gpio and dir: output
    cfgFancControllerHrtBeatP1pin1();        // fan controller board LED @ P1.1
// ----------------------------------------------------------------

// ----------------------------------------------------------------
#if 0
    // for debug to start outputting diag data
    enableDiagnostics();
#endif
// ----------------------------------------------------------------


    while(1)
    {
        __bis_SR_register(LPM0_bits | GIE);     // Enter LPM3 w/ ints enabled

        main_events();
    }
}


