/*
 * fans.h
 *
 *  Created on: Jul 8, 2021
 *      Author: ZAlemu
 */

#ifndef FANS_H_
#define FANS_H_

#include <stdio.h>
#include "timer_utilities.h"


/*
 *    FAN Controller Board; 810405; Hawk Strike
 * P4.5 <= TACH0 (Fan controlled by PWM3 TB3.6/P6.5
 * P4.4 <= TACH1 (Fan controlled by PWM3 TB3.5/P6.4
 * P4.3 <= TACH2 (Fan controlled by PWM3 TB3.4/P6.3
 * P4.2 <= TACH3 (Fan controlled by PWM2 TB3.3/P6.2
 * P4.1 <= TACH4 (Fan controlled by PWM1 TB3.2/P6.1
 * P4.0 <= TACH5 (Fan controlled by PWM0 TB3.1/P6.0
 */
#define  GPIO_PORT4                     (4)

#define GPIO_PORT4_CH0                  (0)
#define FAN_TACH5_P4_0                  GPIO_PORT4_CH0
#define GPIO_PORT4_CH1                  (1)
#define FAN_TACH4_P4_1                  GPIO_PORT4_CH1
#define GPIO_PORT4_CH2                  (2)
#define FAN_TACH3_P4_2                  GPIO_PORT4_CH2
#define GPIO_PORT4_CH3                  (3)
#define FAN_TACH2_P4_3                  GPIO_PORT4_CH3
#define GPIO_PORT4_CH4                  (4)
#define FAN_TACH1_P4_4                  GPIO_PORT4_CH4
#define GPIO_PORT4_CH5                  (5)
#define FAN_TACH0_P4_5                  GPIO_PORT4_CH5
#define GPIO_PORT4_CH6                  (6)
#define GPIO_PORT4_CH7                  (7)

#define GPIO_OUT_INPUT_WITHPDWN         (0)
#define GPIO_OUT_INPUT_WITHPUP          (1)

#define GPIO_DIR_INPUT                  (0)
#define GPIO_DIR_OUTPUT                 (1)

#define GPIO_INT_DISABLED               (0)
#define GPIO_INT_ENABLED                (1)

#define GPIO_INT_GEN_BYLOW2HIGH         (0)
#define GPIO_INT_GEN_BYHIGH2LOW         (1)

typedef struct FAN_TACH
{
    uint16_t    u16TachCount;
    uint16_t    u16TachCountPrevious;
    uint16_t    u16Rpm;
    uint16_t    u16RpmPrevious;
}stFanTach_t;

typedef enum GPIO_PULL_RES_STATUS
{
    GPIO_INTERNAL_RES_DISABLED,
    GPIO_INTERNAL_RES_PULLED_UP,
    GPIO_INTERNAL_RES_PULLED_DOWN,
}eGpioIntPullResState_t;


extern stFanTach_t stFanTach[];

void initFans();
void deInitTachs();
uint16_t fanRpmComputeCb(stTimerStruct_t* myTimer);
void cfgPwmDutyCyclesForFanCtrl(uint8_t ubyDCpercentage);
void cfgGpio4DirPullRes();
void cfgGpioP4ForTachCount(uint8_t ubyGp4Num, uint8_t ubyIsOutput, eGpioIntPullResState_t eResistor);
void cfgGpio4IntTachCount();
void cfgGpioP4Int(uint8_t ubyGp4Num, uint8_t ubyIsIntEnabled, uint8_t ubyEdgeDir);
void initTempState();

#endif /* FANS_H_ */
