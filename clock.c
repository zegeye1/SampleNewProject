#include <msp430.h>
#include <stdint.h>
#include <stdio.h>
#include <intrinsics.h>
#include "clocks.h"
#include "config.h"


const uint16_t  uiFlld = 0;         // divider/multiplier value (cpu feq/clk input)
uint16_t        uiFlln;             // divider/multiplier value (cpu feq/clk input)
uint8_t         ubyCsRangeSelect;   // range value corresponding to desired clk freq
stDevClks_t     stDevClks;          // Holds clock values configured
eCsMclkOutClk_t eMclkClkOut;        // MCLK/SMCLK Clock Out freq in MHz unit*
uint8_t         ubyFramWaitState;

/*
 *****************************************************************************
 * Function:    WD_DisableWdog()
 * -----------------------------
 * WD_DisableWdog(): disables watchdog timer; by default it's enabled
 *
 * no input parameter
 *
 * returns: none
 *
 * Watchdog functionality is enabled after power up in MSP430 platform.
 * Since no watchdog handler is present need to disable functionality
 *  as early as possible when gaining control.
 *****************************************************************************
 */
void WD_DisableWdog()
{
    WDTCTL = WDTPW | WDTHOLD;               // Stop watchdog timer
}


/*
 *****************************************************************************
 * Function:    CS_DisableFll()
 * ----------------------------
 * CS_DisableFll(): disables FLL module
 *
 * input: none
 *
 * returns: none
 *
 * SCG0 is one of the Status Register (SR) field. It is used to
 *  enable/disable FLL
 *****************************************************************************
 */
void CS_DisableFll()
{
    __bis_SR_register(SCG0);                // Disable FLL (set bit=>Disable)
    CSCTL1 &= ~0x01;                        // Disable Modulation
}


/*
 *****************************************************************************
 * Function:    CS_EnableFll()
 * ---------------------------
 * Enables FLL module
 *
 * no input parameter
 *
 * returns: none
 *
 * note: Enable FLL after configuration completed
 *       Will enabled, it will re-configure settings
 *       like DCO and MOD to achieve locking
 *****************************************************************************
 */
void CS_EnableFll()
{
    __bic_SR_register(SCG0);                // Enable FLL (clr bit=>Enable)
}


/*
 *****************************************************************************
 * Function:    CS_CfgClk()
 * ------------------------
 * Configures the Clock System to generate MCLK/SMCLK/ACLK clock signals
 *
 * Following settings are hard coded to these values.
 * MCLK_DIVIDER is set to 1,
 * SMCLK_DIVIDER is set to 1,
 * ACLK_DIVIDER is set to 1,
 * ACLK_CLK_SRC is set to Internal Reference Clock,
 * REF_CLK_4DCO is set to Internal Reference Clock
 *
 * returns: none
 *
 * note: high level API for user interface
 *****************************************************************************
 */
void CS_CfgClkApi(eCsMclkOutClk_t eMclkClock)
{
    eMclkClkOut           = eMclkClock; // save selection to be used in other functions

    /*
     * Configure FRAM wait state as required by the device data sheet for MCLK
     *  operation beyond 8MHz _before_ configuring the clock system.
     * clk <= 8MHz WS=0;  8MHz < clk <= 16MHz WS=1; 16MHz < clk <=24MHz WS=2
     *
     * need to write this wait state value for all frequency. omitting this step
     *  will cause FRAM access with previously configured setting, that is previous
     *  setting will be retained if not updated.
     */
    ubyFramWaitState = (eMclkClock/8.0f + 0.9) - 1;
    FRCTL0 = FRCTLPW | (ubyFramWaitState << 4);

    // if x1 is available use for high speed (USB) or low freq 1MHz
    CS_X1OrRefoDcoSrc(REF_CLK_4DCO);    // use REFOCLK if have no ext clock for FLL
    CS_DcoMclkSMclkSrc();               // DCO source MCLK/SMCLK
    CS_ComputeFllnMult(eMclkClock);     // Compute FLLN & associate freq with Range
    CS_EnableFllTrim();                 // enable FLL trim
    CS_MclkDivider(MCLK_DIVIDER1);      // MCLK = eMclkClkOut / M-Divider (/1)
    CS_SMclkDivider(MCLK_DIVIDER1, SMCLK_DIVIDER1); // SMCLK = MCLK / S-Divider (/1)
    CS_X1orRefoAclkrc(ACLK_CLK_SRC);    // choose ACLK clock source (REFOCLK chosen)
    CS_X1AclkDivider(ACLK_DIVIDER1);    // only meaningful if clk source is XT1 HF
    __delay_cycles(10);
    CS_EnableFll();
    __delay_cycles(10);
    CS_SoftwareTrim(eMclkClock);  // clock configuration end

    stDevClks.ui32RefClkHz  = CS_INP_CLK_REFOCLK_Hz;    // save REF Clk freq
}


/*
 *****************************************************************************
 * Function:    CS_X1OrRefoDcoSrc()
 * --------------------------------
 * FLL reference select (REFOCLK or XT1)
 *
 * ubyX1OrRefClk: CSCTL3.SELREF[b5:b4] = 00b/01b => XT1CLK/REFOCLK
 *
 * returns: none
 *****************************************************************************
 */
void CS_X1OrRefoDcoSrc(uint8_t ubyX1OrRefClk)
{
    // perform clear and set operation
    CSCTL3 &= ~SELREF;
    CSCTL3 |= ubyX1OrRefClk;
}


/*
 *****************************************************************************
 * Function:    CS_EnableFllTrim()
 * -------------------------------
 * Enables the FLL to adjust the frequency in a finer granularity
 *
 * ubyX1OrRefClk: CSCTL1.DCOFTRIMEN[b7] = 0/1 => Enable/Disable FLL trimm
 *
 * returns: none
 *****************************************************************************
 */
void CS_EnableFllTrim()
{
    CSCTL1 |= ubyCsRangeSelect << 1;
    CSCTL1 |= DCOFTRIMEN;
}


/*
 *****************************************************************************
 * Function:    CS_DcoMclkSMclkSrc()
 * ---------------------------------
 * selects the DCO to be the clock source for MCLK and SMCLK
 *
 * ubyX1OrRefClk: CSCTL1.DCOFTRIMEN[b7] = 0/1 => Enable/Disable FLL trimm
 *
 * returns: none
 *
 * note: device supports multiple clock source options (DCO, REFCLK, XT1 & VLO)
 *       however, DCOCLKDIV is usually the choice of the clock because low freq
 *       clock source is used as a source to generate a high frequency clock.
 *****************************************************************************
 */
void CS_DcoMclkSMclkSrc()
{
    // we are forcing here for DCO to be the source for MCLK/SMCLK
    // clear and set operation
    CSCTL4 &= ~SELMS;
    CSCTL4 |= SELMS__DCOCLKDIV;
}


/*
 *****************************************************************************
 * Function:    CS_MclkDivider()
 * -----------------------------
 * Configures a supported divider value to further divide DCOCLKDIV
 *  and generate final MCLK frequency
 *
 * ubyMclkDiv:  mclk clock divider designator CSCTL5.DIVM[b2:b0] =
 *              [0,1,2,3,4,5,6,7] => [/1,/2,/4,/8,/16,/32,/64,/128]
 *
 * returns: none
 *****************************************************************************
 */
void CS_MclkDivider(uint8_t ubyMclkDiv)
{
    uint8_t ubyDiv;
    switch (ubyMclkDiv)
    {
        case 1:
            ubyDiv = 0;
            break;

        case 2:
            ubyDiv = 1;
            break;

        case 4:
            ubyDiv = 2;
            break;

        case 8:
            ubyDiv = 3;
            break;

        case 16:
            ubyDiv = 4;
            break;

        case 32:
            ubyDiv = 5;
            break;

        case 64:
            ubyDiv = 6;
            break;

        case 128:
            ubyDiv = 7;
            break;

        default:
            ubyMclkDiv = 0;
            ubyDiv     = 0;
            break;
    }

    CSCTL5               &= ~7;                                     // clear field
    CSCTL5               |= ubyDiv;                                 // set field
    stDevClks.ui32MclkHz = (eMclkClkOut * 1000000) / ubyMclkDiv;    // save MCLK freq
}


/*
 *****************************************************************************
 * Function:    CS_SMclkDivider()
 * ------------------------------
 * Configures a supported divider value to further divide MCLK
 *  and generate final SMCLK clock
 *
 * ubyMclkDiv:  smclk clock divider designator CSCTL5.DIVS[b5:b4] =
 *              [0,1,2,3] => [/1,/2,/4,/8]
 *
 * returns: none
 *
 * note: in order for the correct SMCLK frequency to be displayed in the structure
 *       stDevClks, MCLK frequency needs to be identified. this requires CS_MclkDivider()
 *       to be called prior to the SMCLK divider function; this information would be lost
 *       and SMCLK frequency will display a 0Hz value even though it is not.
 *       in order to avoid this dependency a copy of the MCLK divider;ubyMclkDivCopy, is
 *       used to store MCLK clock divider value to be used here.
 *****************************************************************************
 */

void CS_SMclkDivider(uint8_t ubyMclkDiv, uint8_t ubySmclkDiv)
{
    uint8_t ubyDiv;

    switch (ubySmclkDiv)
    {
        case 1:
            ubyDiv = 0;
            break;

        case 2:
            ubyDiv = 1;
            break;

        case 4:
            ubyDiv = 2;
            break;

        case 8:
            ubyDiv = 3;
            break;

        default:
            ubySmclkDiv = 1;
            ubyDiv      = 0;
            break;
    }
    CSCTL5 &= ~(3 << 4);        // clear field
    CSCTL5 |= (ubyDiv << 4);    // set field

    // save SMCLK freq
    // stDevClks.ui32SmclkHz = stDevClks.ui32MclkHz /  ubySmclkDiv;
    stDevClks.ui32SmclkHz =  ((eMclkClkOut * 1000000) / ubyMclkDiv) / ubySmclkDiv;
}


/*
 *****************************************************************************
 * Function:    CS_X1orRefoAclkrc()
 * ------------------------------__
 * Configures a supported divider value to further divide ACLK source (XT1)
 *  and generate final ACLK
 *
 * uiAclkSrc:   aclk clock divider designator CSCTL6.DIVA[b11:b8] =
 *              [0,1,2,3,4,5,6,7] => [/1,/16,/32,/64,/128,/256,/384,/512]
 *
 * returns: none
 *
 * note:
 *****************************************************************************
 */
void CS_X1orRefoAclkrc(uint16_t ui6AclkSrc)
{
    CSCTL4 &= ~SELA;
    CSCTL4 |= ui6AclkSrc;
}


/*
 *****************************************************************************
 * Function:    CS_X1AclkDivider()
 * -----------------------------__
 * ACLK clock divider if input clock is X1
 *
 * uiAclkSrc: CSCTL6.DIVA[b11:b8] = 00b/01b => XT1CLK/REFOCLK
 *
 * returns: none
 *
 * note: VLO is also another input source (internally generated ~10KHz),
 *       with a setting value of 10b, that can be used. much lower frequency
 *       accuracy than REFOCLK. REFOCLK is fixed to 32K so no divider
 *       exist for REFOCLK.
 *       If XT1 is used and its frequency is higher than 40KHz, need to
 *       further divide the clock to get it within <40KHz range.
 *****************************************************************************
 */
void CS_X1AclkDivider(uint8_t ubyX1AclkDiv)
{
    // this is meaningful if clock source is external; ignored if internal
    CSCTL6 &= ~DIVA;
    CSCTL6 |= (ubyX1AclkDiv << 8);

    if (REF_CLK_4DCO == SELREF__REFOCLK)
    {   // if using internal, it is always REFOCLK
        stDevClks.ui32AclkHz  = CS_INP_CLK_REFOCLK_Hz;                  // save ACLK freq
    }
    else
    {   // if external, it is X1 LF or HF. Usually divider val  is used if X1 HF used
        stDevClks.ui32AclkHz  = CS_INP_CLK_REFOCLK_Hz / ubyX1AclkDiv;   // save ACLK freq
    }
}


/*
 *****************************************************************************
 * Function:    CS_ComputeFllnMult()
 * ---------------------------------
 * computes the FLL Multiplier (FLLN) value in order to generate the desired
 *  DCOCLKDIV clock frequency from a given range of frequency identifier.
 *  From the comparator perspective, this is a divider value as opposed to
 *  multiplier that scales down DCOCLKDIV to the input clock source frequency.
 *
 * eMclkClock:    1,2,4,8,12,16,20 (supported frequency ranges)
 *
 * returns: none
 *
 * note: the FLL Divider (FLLD) is hard coded to 0 in order to multiply
 *       DCOCLK by 1 and generate DCOCLKDIV.
 *****************************************************************************
 */
void CS_ComputeFllnMult(eCsMclkOutClk_t eMclkClock)
{
    switch((uint8_t)eMclkClock)
    {
        // perform integer division
        // associate clock frequency given in MHz with Range Select given
        //  with values between 0 & 7
        case 1:
            uiFlln  = (CS_DCO_RANGE_1MHz/CS_INP_CLK_REFOCLK_Hz) + 0.5f;
            ubyCsRangeSelect = 0;
            break;

        case 2:
            uiFlln  = (CS_DCO_RANGE_2MHz/CS_INP_CLK_REFOCLK_Hz) + 0.5f;
            ubyCsRangeSelect = 1;
            break;

        case 4:
            uiFlln  = (CS_DCO_RANGE_4MHz/CS_INP_CLK_REFOCLK_Hz) + 0.5f;
            ubyCsRangeSelect = 2;
            break;

        case 8:
            uiFlln  = (CS_DCO_RANGE_8MHz/CS_INP_CLK_REFOCLK_Hz) + 0.5f;
            ubyCsRangeSelect = 3;
            break;

        case 12:
            uiFlln  = (CS_DCO_RANGE_12MHz/CS_INP_CLK_REFOCLK_Hz) + 0.5f;
            ubyCsRangeSelect = 4;
            break;

        case 16:
            uiFlln  = (CS_DCO_RANGE_16MHz/CS_INP_CLK_REFOCLK_Hz) + 0.5f;
            ubyCsRangeSelect = 5;
            break;

        case 20:
            uiFlln  = (CS_DCO_RANGE_20MHz/CS_INP_CLK_REFOCLK_Hz) + 0.5f;
            ubyCsRangeSelect = 6;
            break;

        /*
         * had a problem with the 24 MHz configuration & disabled it for now.
         * this setting has destroyed the launch board in such a way that no new
         *  firmware can be loaded.
         * forcing the default, which is the 8 MHz option selection if attempting
         *  for the 24MHz configuration.
         */
        case 24:
            uiFlln  = (CS_DCO_RANGE_24MHz/CS_INP_CLK_REFOCLK_Hz) + 0.5f;
            ubyCsRangeSelect = 7;
//           break;

        default:
            uiFlln           = (CS_DCO_RANGE_8MHz/CS_INP_CLK_REFOCLK_Hz) + 0.5f;
            ubyCsRangeSelect = 3;
            break;
    }

    // subtract 1 because the formula adds 1 to compute clock frequency
    uiFlln -= 1;
    // set DCO divider
    CSCTL2 = ((uiFlld << 12) + uiFlln);
}


/*
 *****************************************************************************
 * Function:    CS_MonitorDevClks()
 * --------------------------------
 * configures assigned clock monitoring output pins for device clock
 *  signals.
 *
 * input: enum value corresponding to a Port # for one of available clocks
 *
 * returns: none
 *
 * note: MCLK is available on pins  P3.0 & P2.6
 *        (Table 6-65/6-64; P3SELx=01b/ P2SELx=01b)
 *       SMCLK is available on pins P3.4 & P1.0
 *        (Table 6-65/6-63; P3SELx=01b/ P1SELx=10b)
 *       ACLK is available on pin   P1.1
 *        (Table 6-63;      P1SELx=10b)
 *****************************************************************************
 */
void CS_MonitorDevClks(eDbgMcuClkOutPort_t ePortNum)
{
    switch((uint8_t)ePortNum)
    {
    // MCLK pins -------------------------
    case MCLK_P2_6:
        P2DIR  |= BIT6;
        // P2SELx=01b
        P2SEL0 |= BIT6;
        P2SEL1 &= ~BIT6;
        break;

    case MCLK_P3_0:
        P3DIR  |= BIT0;
        // P3SELx=01b
        P3SEL0 |= BIT0;
        P3SEL1 &= ~BIT0;
        break;

    // SMCLK pins ------------------------
    case SMCLK_P1_0:
        P1DIR  |= BIT0;
        // P1SELx=10b
        P1SEL0 &= ~BIT0;
        P1SEL1 |= BIT0;
        break;

    case SMCLK_P3_4:
        P3DIR  |= BIT4;
        //P3SELx=01b
        P3SEL0 |= BIT4;
        P3SEL1 &= ~BIT4;
        break;

    // ACLK pins -------------------------
    case ACLK_P1_1:
        P1DIR |= BIT1;
        // P1SELx=10b
        P1SEL0 &= ~BIT1;
        P1SEL1 |= BIT1;
        break;

    case MCLK_SMCLK_NOT_OUTPUTED:
    default:
        break;
    }
}


/*
 *****************************************************************************
 * Function:    getClockFreq()
 * ---------------------------
 * returns the frequency values in Hz for MCLK, SMCLK and ACLK
 *
 * no input parameter
 *
 * returns: structure type stDevClks_t
 *
 * note: return structure also include input clock source freq to the FLL
 *****************************************************************************
 */
stDevClks_t getClockFreq()
{
    return stDevClks;
}


/*
 *****************************************************************************
 * Function:    CS_SoftwareTrim()
 * ------------------------------
 * computes the DCO and MOD field values that is close to the center of
 *  DCO value, which is 256.
 *
 * several trim coarses are available.
 * FCOTRIM is the first of three trim coarses.
 * Followed by DCOTRIM and lastly
 * followed by DCO Modulator.
 *
 * Idea is that, so long the correct range (DCORANGE and
 *  multipliers are selected, the PLL is going to converge.
 *  However, user wants it more than converge. user desires
 *  that the PLL remains locked as the temperature, operating
 *  times and pressure changes. This is done by insuring
 *  the largest possible tolerance to the setting available.
 *  That is where software trimming is about. finding the
 *  highest tolerance setting (which is DCO value = 256).
 *
 * start with the default FCOTRIM value (in the middle).
 * enable DCO and wait until it lock.
 * after lock, check if DCO is in the center ((512/2)=256).
 * - if DCO < 256, update FCOTRIM by subtracting 1
 * - if DCO > 256, update FCOTRIM by adding 1
 *
 * NOTE: FLL should be enabled prior to running this function so that DCO
 *       values are automatically are updated.
 *
 * no input parameter.
 *
 * returns: none
 *****************************************************************************
 */

void CS_SoftwareTrim(eCsMclkOutClk_t eMclkClock)
{
unsigned int oldDcoTap = 0x01ff;
unsigned int newDcoTap = 0x01ff;
unsigned int newDcoDelta = 0xffff;
unsigned int bestDcoDelta = 0xffff;
unsigned int csCtl0Copy = 0;
unsigned int csCtl1Copy = 0;
unsigned int csCtl0Read = 0;
unsigned int csCtl1Read = 0;
unsigned int dcoFreqTrim = 3;
unsigned char endLoop = 0;

    do
    {
        // start with middle DCO value (512/2=256)
        // since FLL is enabled, FLL will automatically update
        //  this value until a lock is achieved.
        CSCTL0 = 0x100;
        do
        {
            // DCO fault flag is set when DCO value is at the extreme boundaries
            //  0 or 511. since by default oscillator fault int is disabled will
            //  not generate interrupt.
            // not sure how this is corrected. FLL continues updating DCO field
            //  and eventually finds a lock before reaching boundary.
            // if oscillator is bad, then it stays here forever.
            CSCTL7 &= ~DCOFFG;                  // Clear DCO fault flag
        }while (CSCTL7 & DCOFFG);               // Test DCO fault flag

        __delay_cycles((unsigned int)3000 * MHZCLK);        // Wait FLL lock status (FLLUNLOCK) to be stable
                                                            // Suggest to wait 24 cycles of divided FLL reference clock

        // FLLUNCLOCK1=0 : FLLUNLOCK0=1 = 01b => DCOCLK is currently running SLOW
        // FLLUNCLOCK1=1 : FLLUNLOCK0=0 = 10b => DCOCLK is currently running FAST
        // FLLUNCLOCK1=1 : FLLUNLOCK0=1 = 11b => DCO Error; DCO Out of Range
        // FLLUNCLOCK1=0 : FLLUNLOCK0=0 = 00b => FLL is locked
        // DCOFFG=1 => DCO Fault occured; i.e. DCO = 0 or 255

        // fw exits this while loop (parameter(s) become false) when either one
        //  of the two occurs
        // - when FLL is NOT locked (FLLUNLOCK[1:0]==00b)or
        // - DCO error occured (i.e. DCOFFG = 1b)
        while((CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1)) && ((CSCTL7 & DCOFFG) == 0));

        csCtl0Read = CSCTL0;                   // Read CSCTL0
        csCtl1Read = CSCTL1;                   // Read CSCTL1

		// first loop, both (oldDcoTap and newDcoTap)of these are initialized
		//  to 0xFFFF so not change
		// successive loop, newDcoTap, will be updated with FLL computed value

        oldDcoTap = newDcoTap;                 // Record DCOTAP value of last time
        newDcoTap = csCtl0Read & 0x01ff;       // Get DCOTAP value of this time
        dcoFreqTrim = (csCtl1Read & 0x0070)>>4;// Get DCOFTRIM value

        // oldDcoTap holds the value of DCO prior to updating DCOFTRIM value by +/-1.
        // DCOFTRIM will be increased by 1 if newDcoTap is > 256.
        // DCOFTRIM will be decreased by 1 if newDcoTap is < 256.
        //
        // question is when are we going stop updating?
        // stop updating if previous (old) DCO value is < 256 and current (new) DCO value is > 256
        //                or
        // stop updating if previous (old) DCO value is > 256 and current (new) DCO value is < 256
        //
        // latter condition is tested here
        if(newDcoTap < 256)                    // DCOTAP < 256
        {
            newDcoDelta = 256 - newDcoTap;     // Delta value between 256 and DCOTAP

			// we want FLL stop updating DCO when reaching the center value.
			// if old DCO val is >= 256 and the new DCO val is < 256
			//  the center value, 256, is crossed

/*
 *          if((oldDcoTap != 0xffff) && (oldDcoTap >= 256)) // DCOTAP cross 256
 *          oldDcoTap will never be equal, except the first time it runs the
 *          loop. After first time, it will never be equal and will always
 *          be true.
 */
            if((oldDcoTap != 0x01ff) && (oldDcoTap >= 256)) // DCOTAP cross 256
            {
                endLoop = 1;                    // Stop while loop
            }
            else
            {
                dcoFreqTrim--;
                // update DCOFTRIM value (start with a value of 3 for this var)
                // what happens if dcoFreqTrim decrements less than 0; since
                //  it is a 3 bit field, it will be set to 0x7 = 111b
                // CSCTL1[b15:b8]=Reserved
                // CSCTL1[b7]=DCOFTRIMEN is set to 1 so that FLL will
                //  update DCO and MOD fields. since (1 | 1 = 1) DCOFTRIMEN
                //  will not be affected.
                //
                // update CSCTL1:DCOFTRIM with a new value (clear and set operation)
                CSCTL1 = (csCtl1Read & (~DCOFTRIM)) | (dcoFreqTrim<<4);
            }
        }
        else // if (newDcoTap > 256)
        {
            // oldDcoTap is set to be EQUAL to newDcoTap above and
            //  newDcoTap is updated with the FLL computed DCO value
            newDcoDelta = newDcoTap - 256;     // Delta value between DCPTAP and 256

			// we are satisfied with PLL lock DCO when reaching the center value.
			//  if old DCO val is < 256 and the new DCO val is > 256
			// fw can continue knowing that good clock lock is in place.
			// this does not mean that the device will stop updating DCO and MOD fields.
            if(oldDcoTap < 256)                // DCOTAP cross 256
                endLoop = 1;                   // Stop while loop
            else
            {
                dcoFreqTrim++;
                // update DCOFTRIM value (start with a value of 3 for this var)
                // what happens if dcoFreqTrim increment past 7; since
                //  it is a 16 bit value, it will be set to 8
                // CSCTL1[b15:b8]=Reserved
                // CSCTL1[b7]=DCOFTRIMEN is set to 1 so that FLL will
                //  update DCO and MOD fields. since (1 | 1 = 1) DCOFTRIMEN
                //  will not be affected.
                CSCTL1 = (csCtl1Read & (~DCOFTRIM)) | (dcoFreqTrim<<4);
            }
        }

        // track the setting that generated the closest value to the center (256)
        // the last condition that will stop the search will not necessarily be the
        //  best setting (endloop=1 ). it is just an indication that the permutation
        //  for finding the best setting is exhausted.
        // best value setting has been tracking since start.
        // store values for programming registers after exit (endloop=1)
        if(newDcoDelta < bestDcoDelta)         // Record DCOTAP closest to 256
        {
            csCtl0Copy = csCtl0Read;
            csCtl1Copy = csCtl1Read;
            bestDcoDelta = newDcoDelta;
        }

    }while(endLoop == 0);                      // Poll until endLoop == 1

    CSCTL0 = csCtl0Copy;                       // Reload locked DCOTAP
    CSCTL1 = csCtl1Copy;                       // Reload locked DCOFTRIM
    while(CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1)); // Poll until FLL is locked
}
