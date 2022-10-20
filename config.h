/*
 * config.h
 *
 *  Created on: May 18, 2020
 *      Author: ZAlemu
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include "clocks.h"
#include "uart.h"
#include "i2c.h"

/*****************************************************************************
 * user input - clock configuration
 *            initialize constant MHZCLK with desired frequency
 *            MHz1, MHz2, MHz4, MHz8, MHz12, MHz16, or MHz20
 *
 * usage example (set constant value to 8MHz)
 *       #define MHZCLK             MHz8
 *
 * Advise:
 *   User is encouraged to use the default value of 8MHz (MHz8) since
 *   FRAM maximum operation frequency is 8MHz. Anything above 8 MHz
 *    will generate wait state in FRAM access causing delay and more power
 *    consumption. If need to go lower than 8MHz, that is OK.
 *   Validate output correctness when utilizing it for the
 *    first time in a project.
 *
 * WARNING:
 *         It was observed that the PLL was unable to converge when
 *          configured for a 1MHz (MHz1) and 4MHz (MHz4).
 *         Have checked on the fly calculation to match hard coded
 *          sample codes available. Not sure why at this moment
 *          these two frequencies do not work.
 *****************************************************************************
 */
#define MHZCLK                      MHz8

/*****************************************************************************
 * ----- DEBUG: Observe device clock frequency
 *
 * choose MCLK/SMCLK/or SMCLK clock to output on one of the
 *  following ports. Only one of the 3 clocks can be viewed at a time.
 *
 * MCLK Clock  @: MCLK_P2_6 or MCLK_P3_0,
 * SMCLK Clock @: SMCLK_P1_0 or SMCLK_P3_4,
 * ACLK Clock  @: ACLK_P1_1
 *
 * Note:
 * default setting:
 * ^^^^^^^^^^^^^^^^
 *   Do NOT Output Clock (Since Port Usage is Unknown)
 *   #define MCLK_PORT_OUTPUT            MCLK_SMCLK_NOT_OUTPUTED
 *
 * usage example
 *   Output MCLK clock via P3.0
 *   #define MCLK_PORT_OUTPUT            MCLK_P3_0
 *
 * the values (domain) supported by this constant MCLK_PORT_OUTPUT should
 *  be derived from a typedef enum MCU_CLK_OUT defined
 *  within clock.h
 *
 *  MCLK_SMCLK_NOT_OUTPUTED, MCLK_P2_6, MCLK_P3_0, SMCLK_P1_0, SMCLK_P3_4,
 *  ACLK_P1_1
 *****************************************************************************
 */
#ifdef ___DEBUG___
    #define MCLK_SMCLK_ACLK_OUTPUT      MCLK_SMCLK_NOT_OUTPUTED
//  #define MCLK_SMCLK_ACLK_OUTPUT      MCLK_P2_6
//  #define MCLK_SMCLK_ACLK_OUTPUT      MCLK_P3_0
//  #define MCLK_SMCLK_ACLK_OUTPUT      SMCLK_P1_0
    /* we are using P3_4 on launch board to output ticker
     * insure that you do not attempt to enable both at the same time.
     * since the gpio usage is configured last, the use of the port
     *  will be dedicated to the ticker and not SMCLK output.
     */
//  #define MCLK_SMCLK_ACLK_OUTPUT      SMCLK_P3_4
//  #define MCLK_SMCLK_ACLK_OUTPUT      ACLK_P1_1
#endif



/*****************************************************************************
        timer
 *****************************************************************************
 */
#define TMR_TICKER_PERIOD               (0.001)     // set tick clk period
#define TMR_TICKER_TIMER                (TMR_B0)    // selct TMR_B0/1/2/or3


/*****************************************************************************
        I2C Baud Rate
        see i2c.h; choose from pre-defined values or set your own
 *****************************************************************************
 */
#define I2C_BAUD_IN_USE                 I2C_BAUD_RATE_20000HZ
#define I2C_TMP1075_PERIODIC_STRT_TICK  (1000)
// if enough i2c transaction has not been received, reset i2c
#define SW_WATCHDOG_TICK_CNT            (15000)
#define RST_I2C_TRANSACTION_CNT         (1)




/*****************************************************************************
        UART Baud Rate
 *****************************************************************************
 */
#define UART_BAUD_IN_USE                UART_BAUD_115200


/*****************************************************************************
        CLI
 *****************************************************************************
 */
#define DIAG_DISPLAY_INTERVAL_PERIOD    (900)


/*****************************************************************************
        ADC
 *****************************************************************************
 */
#define ADC_NUM_OF_CHS_ENABLED          (3) // should include int temp if used
#define ADC_ACQ_PERIOD                  (300)


/*****************************************************************************
        HEATER
 *****************************************************************************
 */
#define HEATER_ON_INTERVAL_MS           (2000)


/*****************************************************************************
        FAN PWM, RPM Calc Tick Count, and Temp Set points related to PWM Chg
 *****************************************************************************
 */
#define TMR_PWM_PRD_FREQ_IN_USE         TMR_TBCCRx_PWM_FREQ_25000
//#define TMR_PWM_PRD_FREQ_IN_USE         TMR_TBCCRx_PWM_FREQ_20000
//#define TMR_PWM_PRD_FREQ_IN_USE         TMR_TBCCRx_PWM_FREQ_18000
//#define TMR_PWM_PRD_FREQ_IN_USE         TMR_TBCCRx_PWM_FREQ_15000
//#define TMR_PWM_PRD_FREQ_IN_USE         TMR_TBCCRx_PWM_FREQ_10000
//#define TMR_PWM_PRD_FREQ_IN_USE         TMR_TBCCRx_PWM_FREQ_5000
//#define TMR_PWM_PRD_FREQ_IN_USE         TMR_TBCCRx_PWM_FREQ_1500

//# of sec = FAN_RPM_CALC_COUNT_TICK * TICK_PRD; e.g. (2000 * 0.001 = 2sec)
#define FAN_RPM_CALC_COUNT_TICK         (2000)

#define HEATER_ON_THRESHOLD             (-4)
#define FAN_HYSTERISIS_TEMP             (2)

#endif /* CONFIG_H_ */
