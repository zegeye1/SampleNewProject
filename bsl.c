#include <stdint.h>
#include <msp430.h>
#include "bsl.h"
#include "config.h"
#include "timer.h"
#include "adc.h"
#include "uart.h"
#include "fans.h"
#include "i2c.h"

#pragma RETAIN(bslConfigurationSignature)
#pragma DATA_SECTION(bslConfigurationSignature, ".bslconfigsignature")
const unsigned int bslConfigurationSignature = BSL_CONFIG_SIGNATURE;
#pragma RETAIN(bslConfig)
#pragma DATA_SECTION(bslConfig, ".bslconfig")
const unsigned int bslConfig = (BSL_CONFIG_PASSWORD | 1);   // Uart only, not auto detect


void invokeBsl(void)
{
volatile uint16_t c;
    __disable_interrupt();
    deInitTickTimer();
    deInitPwmTimerB3();
    deInitTachs();
    deInitAdc();
    deInitI2c(I2C_0);
    __delay_cycles(200000);
    // clear receive buffer
    c = UCA0RXBUF;
    c = UCA0RXBUF;
    if (MHZCLK == 16)
    {
        __bis_SR_register(SCG0);                                    // disable FLL
        __delay_cycles(20000);
        CSCTL1 = DCOFTRIMEN_1 | DCOFTRIM0 | DCOFTRIM1 | DCORSEL_3;  // DCOFTRIM=3, DCO Range = 8MHz
        CSCTL2 = FLLD_0 + 243;                                      // DCODIV = 8MHz
    }
    ((void (*)())0x1000)();
    return;
}
