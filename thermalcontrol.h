/*
 * thermalcontrol.h
 *
 *  Created on: Jul 9, 2021
 *      Author: ZAlemu
 */

#ifndef THERMALCONTROL_H_
#define THERMALCONTROL_H_

#include <stdint.h>
#include <msp430.h>
#include "fans.h"

#define ON          true
#define OFF         false

// Port Abstractions
#define SETBIT(p,b) p##OUT |= b
#define CLRBIT(p,b) p##OUT &= ~b

#define NUM_TZONES              (8)
#define NUM_FANS                (2)     // Only PWM4 and PWM5 are to be used to control GPU & CPU Cooling

#define MAX_TEST_TEMPERATURE    70      // to be used when testing fan pwm and temp association
#define MIN_TEST_TEMPERATURE    0       // to be used when testing fan pwm and temp association

#define LOW_HIGH_LIMIT      (2)
#define HTR_ALL_ON_OFF      (63)    // 0x3F

#define HTR_CTRL0(arg)      if(arg) {SETBIT(P2,BIT5);}  else    {CLRBIT(P2,BIT5);}
#define HTR_CTRL1(arg)      if(arg) {SETBIT(P2,BIT4);}  else    {CLRBIT(P2,BIT4);}
#define HTR_CTRL2(arg)      if(arg) {SETBIT(P2,BIT3);}  else    {CLRBIT(P2,BIT3);}
#define HTR_CTRL3(arg)      if(arg) {SETBIT(P2,BIT2);}  else    {CLRBIT(P2,BIT2);}
#define HTR_CTRL4(arg)      if(arg) {SETBIT(P2,BIT1);}  else    {CLRBIT(P2,BIT1);}
#define HTR_CTRL5(arg)      if(arg) {SETBIT(P2,BIT0);}  else    {CLRBIT(P2,BIT0);}
#define HTR_CTRLALL(arg)    if(arg)                                                 \
                            {                                                       \
                                P2OUT |= (BIT5 | BIT4 | BIT3 | BIT2 | BIT1 | BIT0); \
                            }                                                       \
                            else                                                    \
                            {                                                       \
                                P2OUT &= ~(BIT5 | BIT4 | BIT3 | BIT2 | BIT1 | BIT0);\
                            }

enum TZ_CONSTS
{
    TZ0_LOW,                //0
    TZX_LOW =   TZ0_LOW,    //0     // High of Lower Zone pt equals LOW of next Higher Zone pt
    TZ1_LOW,                //1
    TZX_HIGH =  TZ1_LOW,    //1
    TZ2_LOW,                //2
    TZ3_LOW,                //3
    TZ4_LOW,                //4
    TZ5_LOW,                //5
    TZ6_LOW,                //6
    TZ7_LOW,                //7
};

extern bool gbIsHtrOn;
extern bool bAllHeatersOn;
extern bool bHeaterTmrOnStatus;
extern stTimerStruct_t stHeaterOntTmr;

extern uint8_t  ubyTempHysteresis;
extern float    fTz[NUM_TZONES][LOW_HIGH_LIMIT];
extern int8_t   gbyHtrOnSetPt;
extern bool     gbEnableTempCycleTest;
extern uint16_t persistentMemoryInitialized;
extern uint8_t  gubyCurrentTz[];
extern uint8_t  gubyPwmInTest;
extern int8_t   gbyTestTemperatureVal;
extern uint8_t  fFanPwm[NUM_FANS][NUM_TZONES];

void initThermalControl();
void findTz();
void updateTz();
void setPwmFromTz();
void setSinglePwmFromTz(uint8_t ubyPwmNum);
void processThermalControl();

void turnOnHeater();
void disableHtrTmr();
void updateHeater();
void turnHeaterOnOff(uint8_t ubyHtrNum, bool bOnOff);
uint16_t htrOnCb(stTimerStruct_t* myTimer);
void forceTempVal();

#endif /* THERMALCONTROL_H_ */
