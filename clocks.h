#ifndef CLOCKS_H_
#define CLOCKS_H_

#include <stdint.h>

/*
 * If need to observe the clock frequency (based on user configuration done
 *  in config.h file) use one of the available pins to qualify clock generation.
 *
 * MCLK is available on pins  P3.0 & P2.6  (Table 6-65/6-64; P3SELx=01b/ P2SELx=01b)
 * SMCLK is available on pins P3.4 & P1.0  (Table 6-65/6-63; P3SELx=01b/ P1SELx=10b)
 * ACLK is available on pin   P1.1         (Table 6-63;      P1SELx=10b)
 */

// mclk freq (1,2,4,8,12,16,20)
// it has been observed a pb with some devices, at least 1, that
//  freq 1MHz and 4MHz not working. FLL refuses to lock.
#define CS_DCO_RANGE_1MHz       ( 1000000)
#define CS_DCO_RANGE_2MHz       ( 2000000)
#define CS_DCO_RANGE_4MHz       ( 4000000)
#define CS_DCO_RANGE_8MHz       ( 8000000)
#define CS_DCO_RANGE_12MHz      (12000000)
#define CS_DCO_RANGE_16MHz      (16000000)
#define CS_DCO_RANGE_20MHz      (20000000)
#define CS_DCO_RANGE_24MHz      (24000000)

// MCLK Dividers
#define MCLK_DIVIDER1           (1)
#define MCLK_DIVIDER2           (2)
#define MCLK_DIVIDER4           (4)
#define MCLK_DIVIDER8           (8)
#define MCLK_DIVIDER16          (16)
#define MCLK_DIVIDER32          (32)
#define MCLK_DIVIDER64          (64)
#define MCLK_DIVIDER128         (128)

// SMCLK Dividers
#define SMCLK_DIVIDER1          (1)
#define SMCLK_DIVIDER2          (2)
#define SMCLK_DIVIDER4          (4)
#define SMCLK_DIVIDER8          (8)

// ACLK Divider (Only if X1 is selected)
// should never be > 40KHz
#define ACLK_DIVIDER1           (0)
#define ACLK_DIVIDER16          (1)
#define ACLK_DIVIDER32          (2)
#define ACLK_DIVIDER64          (3)
#define ACLK_DIVIDER128         (4)
#define ACLK_DIVIDER256         (5)
#define ACLK_DIVIDER384         (6)
#define ACLK_DIVIDER512         (7)


/*
 *                  IMPORTANT
 * MUST Input High Quality Clock Source if interfacing with USB or
 *  other targets that require accurate clocking.
 * A 32KHz Quality Crystal will do as an input, X1.
 *
 * Use REFOCLK if not requiring very high accuracy and running @
 *  higher speed like > 1MHz.
 * Use X1 quality crystal, could be 32K, if requiring accuracy
 *  or running @ lower speed like < 1MHz.
 */
// choose input clock source to the Clock System
#define REFOCLK                 (32768.0f)
// Replace this value with the actual X1 Crystal/Oscillator value
#define X1EXTCLK                (32768.0f)
#if 1
//  internal not very accurate reference clock (REFOCLK) of 32K used
#define CS_INP_CLK_REFOCLK_Hz   REFOCLK  // REFCLOCK or X1-LF 32K
#define REF_CLK_4DCO            SELREF__REFOCLK
#define ACLK_CLK_SRC            SELA__REFOCLK
#else
//  external very accurate reference clock (X1 LF) of 32K used
#define CS_INP_CLK_REFOCLK_Hz   X1EXTCLK  // REFCLOCK or X1-LF 32K
#define REF_CLK_4DCO            SELREF__XT1CLK
#define ACLK_CLK_SRC            SELA__XT1CLK
#endif


// User Entry values.
// the element of this enum value is used to initialize const value
//  MHZCLK, with one of the desired MCLK frequencies, in MHz, below.
//  see config.h
typedef enum CS_MCLK_FREQ_SELECT
{
    MHz1 = 1,
    MHz2 = 2,
    MHz4 = 4,
    MHz8 = 8,
    MHz12 = 12,
    MHz16 = 16,
    MHz20 = 20,
    MHz24 = 24
}eCsMclkOutClk_t;


// DCORSEL corresponding value for user selected Frequency, MHZCLK,
//  within, config.h
typedef enum CSRANGE_SELECT
{
    CS_RANGE_FREQ1,
    CS_RANGE_FREQ2,
    CS_RANGE_FREQ4,
    CS_RANGE_FREQ8,
    CS_RANGE_FREQ12,
    CS_RANGE_FREQ16,
    CS_RANGE_FREQ20,
    CS_RANGE_FREQ24,
}eCsRangeMhz_t;


// after clock configurations the programmed clock outputs are
//  stored here. A user getter function, getClockFreq(), is
//  used to get this data from another *.c file.
typedef struct STDEVCLKS
{
    uint32_t ui32MclkHz;
    uint32_t ui32SmclkHz;
    uint32_t ui32AclkHz;
    uint32_t ui32RefClkHz;
}stDevClks_t;


// this enum values are used to output selected
// clock source while in debug mode.
//
// since it is not known as to which port is not used
//  it is forced to output NONE by default.
typedef enum CLK_DBG_OUT_PORT
{
    MCLK_SMCLK_NOT_OUTPUTED,
    MCLK_P2_6,
    MCLK_P3_0,
    SMCLK_P1_0,
    SMCLK_P3_4,
    ACLK_P1_1
}eDbgMcuClkOutPort_t;


// watchdog is enabled by default. disable wdog if not using
void WD_DisableWdog();
// hunts for the closest setting to the desired frequency by trimming
//  DCOFTRIM/DCO values
void CS_SoftwareTrim(eCsMclkOutClk_t eMclkClock);
// enable FLL so automatic configuration value are calculated by h/w
void CS_EnableFll();
// disable FLL while configuring/reconfiguring clock registers
void CS_DisableFll();
// select clock source to the DCO/FLL to be X1 or REFOCLK
void CS_X1OrRefoDcoSrc(uint8_t ubyX1OrRefClk);
// figure out multipler (FLLN=0) and divider (FLLD) value for the desired
//  MCLK freq
void CS_ComputeFllnMult(eCsMclkOutClk_t eMclkClock);
// further divide the desired MCLK freq with supported divider value
void CS_MclkDivider(uint8_t ubyMdiv);
// further divide the desired SMCLK freq with supported divider value
void CS_SMclkDivider(uint8_t ubyMdiv, uint8_t ubySdiv);
// find value for DCO closer to its 1/2 value (256) via modifying
//  DCOFTRIM and DCO fields
void CS_EnableFllTrim();
// select ACLK clock source to be X1 or REFOCLK
void CS_X1orRefoAclkrc(uint16_t ui6AclkSrc);
// further divide input ACLK clk if X1 ACLK with higher freq is used
void CS_X1AclkDivider(uint8_t ubyX1AclkDiv);
// select MCLK output between (REFOCLK, DCO, X1, VLO) - DCO is used
void CS_DcoMclkSMclkSrc();
// user getter function declaration to get clock freq programmed
//  to be called after clock configuration completed
stDevClks_t getClockFreq();
// a single API to be used by the user to configure device clocks
//  using one of the pre-determined operating clock frequencies
void CS_CfgClkApi(eCsMclkOutClk_t eMclkClock);
void CS_MonitorDevClks(eDbgMcuClkOutPort_t ePortNum);
#endif
