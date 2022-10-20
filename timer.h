/*
 * timer.h
 *
 */

#ifndef TIMER_H_
#define TIMER_H_

#include "timer_utilities.h"

#define INPUT_CLK_EXT_PIN       TBSSEL_0
#define INPUT_CLK_ACLK          TBSSEL_1
#define INPUT_CLK_SMCLK         TBSSEL_2
#define INPUT_CLK_INCLK         TBSSEL_3

#define INPUT_CLK_DIV1          ID_0            // 0000h
#define INPUT_CLK_DIV2          ID_1            // 0040h
#define INPUT_CLK_DIV4          ID_2            // 0080h
#define INPUT_CLK_DIV8          ID_3            // 00C0h

#define TMR_STOP_MODE           MC__STOP
#define TMR_UP_MODE             MC__UP
#define TMR_CONTINUOUS_MODE     MC__CONTINUOUS
#define TMR_UP_DOWN_MODE        MC__UPDOWN

#define TIMERB_0                0
#define TIMERB_1                1
#define TIMERB_2                2
#define TIMERB_3                3

// identifies TBxCTL or TBxCCTL0 regs
#define CTRL_REG_TYPE           0       // TBxCTL Regs ID
#define CTRL_REG0_CAP_COMP_TYPE 1       // TBxCCTL0 Regs ID

#define TMR_B0_BASE_ADD         0380h
#define TMR_B1_BASE_ADD         03C0h
#define TMR_B2_BASE_ADD         0400h
#define TMR_B3_BASE_ADD         0440h

#define TMR_BX_CLEAR            TBCLR   // TBCLR = 1
#define TMR_BX_INT_ENABLE       TBIE    // TBIE = 1
#define TMR_BX_INT_DISABLE      !TBIE   // TBIE = 0
#define TMR_BX_INT_FLAG         TBIFG

#define CTRL_REG_DATA16_BIT     (CNTL__16)
#define CTRL_REG_DATA12_BIT     (CNTL__12)
#define CTRL_REG_DATA10_BIT     (CNTL__10)
#define CTRL_REG_DATA8_BIT      (CNTL__8)
#define CTRL_REG_CLK_ACLK       (TBSSEL__ACLK)
#define CTRL_REG_CLK_SMCLK      (TBSSEL__SMCLK)
#define CTRL_REG_ID_DIV1        (ID__1)
#define CTRL_REG_ID_DIV2        (ID__2)
#define CTRL_REG_ID_DIV4        (ID__4)
#define CTRL_REG_ID_DIV8        (ID__8)
#define CTRL_REG_MODE_STOP      (MC__STOP)
#define CTRL_REG_MODE_UP        (MC__UP)
#define CTRL_REG_MODE_CONT      (MC__CONTINUOUS)
#define CTRL_REG_MODE_UP_DWN    (MC__UPDOWN)
#define CTRL_REG_CLR_FIELDS     (TBCLR)
#define CTRL_REG_INT_ENABLE     (TBIE)
#define CTRL_REG_INT_DISABLE    (TBIE_0)
#define CTRL_REG_INT_FLAG       (TBIFG)

// Timer Capture/Compare Control Register
#define CC_CNTL_REG_SYNC        (SCS)
#define CC_CNTL_REG_COMPARE     (CAP__COMPARE)
#define CC_CNTL_REG_OUTMOD0     (OUTMOD_0)
#define CC_CNTL_REG_OUTMOD1     (OUTMOD_1)
#define CC_CNTL_REG_OUTMOD2     (OUTMOD_2)
#define CC_CNTL_REG_OUTMOD3     (OUTMOD_3)
#define CC_CNTL_REG_OUTMOD4     (OUTMOD_4)
#define CC_CNTL_REG_OUTMOD5     (OUTMOD_5)
#define CC_CNTL_REG_OUTMOD6     (OUTMOD_6)
#define CC_CNTL_REG_OUTMOD7     (OUTMOD_7)
#define CC_CNTL_REG_INT_ENABLE  (CCIE)
#define CC_CNTL_REG_INT_DISABLE (CCIE_0)
#define CC_CNTL_REG_INT_FLAG    (CCIFG)

// Timer B, TIMERB0_3, TIMERB1_3, TIMERB2_3, TIMERB3_7
#define TMR_B0                  (0)
#define TMR_B1                  (1)
#define TMR_B2                  (2)
#define TMR_B3                  (3)
#define TMR_CCR0                (0)
#define TMR_CCR1                (1)
#define PWM5_CCR1               TMR_CCR1        // fan controller 810405
#define TMR_CCR2                (2)
#define PWM4_CCR2               TMR_CCR2        // fan controller 810405
#define TMR_CCR3                (3)
#define PWM3_CCR3               TMR_CCR3        // fan controller 810405
#define TMR_CCR4                (4)
#define PWM2_CCR4               TMR_CCR4        // fan controller 810405
#define TMR_CCR5                (5)
#define PWM1_CCR5               TMR_CCR5        // fan controller 810405
#define TMR_CCR6                (6)
#define PWM0_CCR6               TMR_CCR6        // fan controller 810405

// uded by CLI for pwm get/set commands
#define tb0                     (TMR_B0)
#define tb1                     (TMR_B1)
#define tb2                     (TMR_B2)
#define tb3                     (TMR_B3)
#define ccr0                    (TMR_CCR0)
#define ccr1                    (TMR_CCR1)
#define ccr2                    (TMR_CCR2)
#define ccr3                    (TMR_CCR3)
#define ccr4                    (TMR_CCR4)
#define ccr5                    (TMR_CCR5)
#define ccr6                    (TMR_CCR6)

#define TMRCLK_SMCLK            (CTRL_REG_CLK_SMCLK)

// choose one of the freq available below
#define TMR_TBCCRx_PWM_FREQ_25000       (25000)
#define TMR_TBCCRx_PWM_FREQ_20000       (20000)
#define TMR_TBCCRx_PWM_FREQ_18000       (18000)
#define TMR_TBCCRx_PWM_FREQ_15000       (15000)
#define TMR_TBCCRx_PWM_FREQ_10000       (10000)
#define TMR_TBCCRx_PWM_FREQ_5000        (5000)
#define TMR_TBCCRx_PWM_FREQ_1500        (1500)


#define HEARTBEAT_TIME          1000    // delay tick cnt to service HeartBit


/*
 * Base timer configuration requires configuration access
 *  to processor registers.
 * The MSP430FR2355 device has 4 timers where each have
 *  set of registers.
 * The length of registers is linearly increased based on
 *  the count of peripheral.
 * A common handler is used to access the same type of
 *  register from all timers. This allows a single api
 *  to handle all timers.
 * Mainly there are 4 groups of registers.
 *  Control Registers,
 *  Capture Compare Control Registers,
 *  Capture Compare Registers, and
 *  Counters
 *
 */
typedef struct TMR_GRP_REGS_ADDRESS
{
    volatile uint16_t* pTmrCntrl;
    volatile uint16_t* pTmrCapCompCntl;
    volatile uint16_t* pTmrCounter;
    volatile uint16_t* pTmrCapCompReg;
}TMR_GrpRegsAddress_t;


/*
 * this structure type is used to store timer parameters that is
 *  used to generate a PWM waveform.
 * when multiple CCRs from multiple timers are used to generate PWMs,
 *  it is hard to generate the addresses as well as compute some of
 *  the floating number values for every request. request amount could
 *  be very large.
 * these parameters and values are computed and stored when configuring
 *  timer to generate PWMs. this allows for a single effort, multi-use
 *   of the parameters when need to modify PWM Duty Cycle.
 */
typedef struct TIMERX_PWM_PARAMS
{
    uint8_t ubyTimerNum;

    uint16_t ui16PwmPrdCnt;

    uint8_t ubyPercentageDC[7];

    // (1.0f/ui16PwmPeriod) * 10000 - used for integer math of computing PWM %
    uint32_t ui32InvPrdCntScaled;   // scaled by 10000

    // holds the structure of the specfic timer; e.g. TimerB0_3 addresses
    TMR_GrpRegsAddress_t stPwmTimerRegsAddress;
}stTimerXPwmParams_t;

extern uint16_t gu16MilliSecCpuClkCycleCount;
extern TMR_GrpRegsAddress_t stTimerRegsAddress;
extern TMR_GrpRegsAddress_t stTickTimerRegsAddress;
extern stTimerXPwmParams_t  stPwmTmrsParams[4];

// global sw timers
extern stTimerStruct_t sTimerQueueHead;

// timer callback functions (handlers)
uint16_t launchPadHeartBeatToggle(stTimerStruct_t* myTimer);        // p1.0
uint16_t fanControllerHeartBeatToggle(stTimerStruct_t* myTimer);    // p1.1

bool tickTmrIsrHandler();

void TMR_GetTmrRegsAddress(uint8_t ubyTmrNum);
void TMR_SectTmrCntrLength(uint8_t ubyTmrNum, uint16_t ui16CntrlLen);
void TMR_TmrInputClk(uint8_t ubyTmrNum, uint16_t uiTmrClk);
//void TMR_SetOperatingMode(uint8_t ubyTmrNum, uint8_t ubyCcrRegNum, uint16_t uiOpMode);
void TMR_SetOperatingMode(TMR_GrpRegsAddress_t stTimerRegAddress, uint16_t uiOpMode);
uint16_t TMR_GetTimerOperatingMode(TMR_GrpRegsAddress_t stTimerRegAddress);
void TMR_SetClkDivider(uint8_t ubyTmrNum, uint16_t uiInClkDivider);
void TMR_ClearClkCtrDivDirection(uint8_t ubyTmrNum);
void TMR_IntEnableDisable(uint8_t ubyTmrNum, bool bEnableInt);
void TMR_GetTmrRegAddresss(uint8_t ubyTmrNum, uint8_t ubyCcrRegNum);
void TMR_CfgTimerBxTick(uint8_t ubyTimer, float fTickValue);
void TMR_PwmPrdCfgForTimerBx(uint8_t ubyTmrNum, uint16_t ui16PwmPrdFreq);
void TMR_PwmOutModeAndPinMuxforTimberBxAndCcrX(uint8_t ubyTmrNum,
                                                uint8_t ubyCcrNum, uint16_t ui16OutMode);
int16_t TMR_PwmSetPercentage (uint8_t ubyTmrNum, uint8_t ubyCcrNum, uint8_t ubyPercentage);
void TMR_PwmPinMuxCfg(uint8_t ubyTimer, uint8_t ubyCcrNum);
int8_t TMR_PwmGetDcPercenatage(uint8_t ubyTmrNum, uint8_t ubyCcrNum);

void timer_test();
void cfgTickClkTestPort();
void deInitTickTimer();
void deInitPwmTimerB3();


#endif /* TIMER_H_ */
