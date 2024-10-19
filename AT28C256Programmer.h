/*
             LUFA Library
     Copyright (C) Dean Camera, 2021.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2021  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Header file for VirtualSerial.c.
 */

#ifndef _AT28C256_PROGRAMMER_H_
#define _AT28C256_PROGRAMMER_H_

	/* Includes: */
		#include <avr/io.h>
		#include <avr/wdt.h>
		#include <avr/power.h>
		#include <avr/interrupt.h>
		#include <string.h>
		#include <stdio.h>
		#include <util/delay.h>

		#include "Descriptors.h"

        #include <LUFA/Drivers/Misc/RingBuffer.h>
		#include <LUFA/Drivers/USB/USB.h>
		#include <LUFA/Platform/Platform.h>

	/* Macros: */

    /* Input state machine states */
    #define GET_OPCODE      0
    #define GET_H_ADDRESS   1
    #define GET_L_ADDRESS   2
    #define READ_WRITE      3

    /* Command op codes */
    #define READ            'r'
    #define WRITE           'w'
    #define SDP_ON          'e'
    #define SDP_OFF         'd'

	/* Function Prototypes: */
		void SetupHardware(void);

		void EVENT_USB_Device_Connect(void);
		void EVENT_USB_Device_Disconnect(void);
		void EVENT_USB_Device_ConfigurationChanged(void);
		void EVENT_USB_Device_ControlRequest(void);

        void GPIO_Init(void);

        void ProcessBuffer(void);

        bool DataPoll(uint16_t address, uint8_t byte);

        void EnableSDP(void) ;
        void DisableSDP(void) ;

        void WriteByteToAddress(uint16_t address, uint8_t byte);
        uint8_t ReadByteFromAddress(uint16_t address);

    /* GPIO bitmasks */
    #define OE_ACTIVE_LOW 0b11111011
    #define WE_ACTIVE_LOW 0b10111111
    #define CE_ACTIVE_LOW 0b01111111

    #define PORTB_PIN_MASK 0b11111111
    #define PORTC_PIN_MASK 0b11000000
    #define PORTD_PIN_MASK 0b11111111
    #define PORTE_PIN_MASK 0b01000100
    #define PORTF_PIN_MASK 0b11110011

#endif

