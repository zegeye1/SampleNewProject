//___________________________________________________________________________
//
// Filename : Revisions.h
// Project  : DEMEC7040SYS-01
// Processor: MSP430FR2355A
// Compiler : Code Composer Studio Compiler
//            CCS Version: 10.2.0.00009
//
// Description:
//
//  This file contains the revision history text
//
#ifndef REVISIONS_H_
#define REVISIONS_H_

// This is a programmer's revision history
//___________________________________________________________________________
//
//  Version DEMEC7040SYS-01 V0.1            16/Sep/2022
//
//      Initial Commit:
//          -cloned from EC7040_HAWK_STRIKE V1.1 dated 09/Sep/2022
//
//___________________________________________________________________________
//
//  Version DEMEC7040SYS-01 V0.2            29/Sep/2022
//
//      BSL:
//          - changed the wait time for the firmware update from 10ms to
//            50 ms since the 3.3V TTL uart cable was introducing some
//            delay and not all user direction was printed out on the screen.
//
//      RPM/TACH:
//          - corrected CLI diagnostic output to correctly display RPM
//            values instead of Fan Tach counts.
//
//___________________________________________________________________________
//
//  Version DEMEC7040SYS-02 V0.3            29/Sep/2022
//
//      PROJECT_NAME:
//          - corrected project repo name and firmware name from
//            DEMEC7040SYS-01 to DEMEC7040SYS-02.
//
//___________________________________________________________________________
//
//  Version DEMEC7040SYS-02 V1.0            06/Oct/2022
//
//  This is RELEASE version.
//
//      ADC Structure:
//          - modified adc user defined structure, stAdcSnsrData_t, by adding
//            couple more elements for ease of programming.
//
//      Thermal control:
//          - modified thermalcontrol.c with a modified function handler as
//            well as PWM and Zone control tables.
//
//      RTD:
//          - modified RTD.c file with an updated transformation function
//            handler.
//
//      CLI:
//          - modified the diagnostics output to match with the required
//            information for this project.
//
//      Self Test:
//          - modified the PWM transition test via force temperature values
//            for a full single test run, on release version, and for
//            continuous/infinite cycle during development.
//
//___________________________________________________________________________


#define FIRMWARE_ID_SZ  32
// FIRMWARE_ID length needs to be <= FIRMWARE_ID_SZ-1
//                   12345678901234567890123456789012
#define FIRMWARE_ID "DEMEC7040SYS-02 V1.0"

//------------------------------------
// if fw is an intermediate update, i.e. not a release version
//  please comment the #define below.
#define RELEASED_VERSION
#ifdef RELEASED_VERSION
    #define RELEASE_UPDATE "- Released"
#else
    #define RELEASE_UPDATE "- Updated"
#endif

#define DATE_COMMIT "06/Oct/2022"


#endif /* REVISIONS_H_ */
