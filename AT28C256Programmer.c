/*

    This makes use of, and is based on:

             LUFA Library
     Copyright (C) Dean Camera, 2021.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org

    And:

  https://github.com/KITcar-Team/kitcar-usbserial

    As well as:

  https://github.com/wagiminator/ATmega-EEPROM-Programmer

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
 *  Main source file for the AT28C256Programmer project. This file contains 
 * the main tasks of  the project and is responsible for the initial 
 * application hardware configuration.
 */

#include "AT28C256Programmer.h"

/** Circular buffer to hold data from the host before it is sent to the 
 * device via the serial port. 
 */
static RingBuffer_t USB_Incoming_Buffer;

/** Underlying data buffer for \ref USB_Incoming_Buffer, where the stored 
 * bytes are located. 
 */
static uint8_t      USB_Incoming_Buffer_Data[10];

/** Circular buffer to hold data from the serial port before it is sent to 
 * the host. 
 */
static RingBuffer_t USB_Outgoing_Buffer;

/** Underlying data buffer for \ref USB_Outgoing_Buffer, where the stored 
 * bytes are located. 
 */
static uint8_t      USB_Outgoing_Buffer_Data[10];

/** LUFA CDC Class driver interface configuration and state information. 
 * This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of 
 * the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = INTERFACE_ID_CDC_CCI,
				.DataINEndpoint                 =
					{
						.Address                = CDC_TX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.DataOUTEndpoint                =
					{
						.Address                = CDC_RX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.NotificationEndpoint           =
					{
						.Address                = CDC_NOTIFICATION_EPADDR,
						.Size                   = CDC_NOTIFICATION_EPSIZE,
						.Banks                  = 1,
					},
			},
	};


/** Main program entry point. This routine contains the overall program flow, 
 * including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();

	RingBuffer_InitBuffer(
        &USB_Incoming_Buffer, 
        USB_Incoming_Buffer_Data, 
        sizeof(USB_Incoming_Buffer_Data)
    );
	RingBuffer_InitBuffer(
        &USB_Outgoing_Buffer, 
        USB_Outgoing_Buffer_Data, 
        sizeof(USB_Outgoing_Buffer_Data)
    );

	GlobalInterruptEnable();

	for (;;)
	{

		/* Only try to read in bytes from the CDC interface if the transmit 
         * buffer is not full 
         */
		if (!(RingBuffer_IsFull(&USB_Incoming_Buffer)))
		{
			int16_t ReceivedByte = CDC_Device_ReceiveByte(
                &VirtualSerial_CDC_Interface
            );

			/* Store received byte into the USART transmit buffer 
             */
			if (!(ReceivedByte < 0))
			  RingBuffer_Insert(&USB_Incoming_Buffer, ReceivedByte);
		}

		uint16_t BufferCount = RingBuffer_GetCount(&USB_Outgoing_Buffer);
		if (BufferCount)
		{
			Endpoint_SelectEndpoint(
                VirtualSerial_CDC_Interface.Config.DataINEndpoint.Address
            );

			/* Check if a packet is already enqueued to the host - if so, we 
             * shouldn't try to send more data until it completes as there 
             * is a chance nothing is listening and a lengthy timeout could 
             * occur 
             */
			if (Endpoint_IsINReady())
			{
				/* Never send more than one bank size less one byte to the 
                 * host at a time, so that we don't block while a Zero Length 
                 * Packet (ZLP) to terminate the transfer is sent if the host 
                 * isn't listening 
                 */
				uint8_t BytesToSend = MIN(BufferCount, (CDC_TXRX_EPSIZE - 1));

				/* Read bytes from the USART receive buffer into the USB IN 
                 * endpoint i
                 */
				while (BytesToSend--)
				{
					/* Try to send the next byte of data to the host, abort 
                     * if there is an error without dequeuing 
                     */
					if (
                        CDC_Device_SendByte(
                            &VirtualSerial_CDC_Interface,
                            RingBuffer_Peek( &USB_Outgoing_Buffer)
                        ) != ENDPOINT_READYWAIT_NoError
                    ) {
						break;
					}

					/* Dequeue the already sent byte from the buffer now we 
                     * have confirmed that no transmission error occurred 
                     */
					RingBuffer_Remove(&USB_Outgoing_Buffer);
				}
			}
		}

        ProcessBuffer();

		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's 
 * functionality. 
 */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses 
     */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

    /* Disable JTAG
     */
    MCUCR |= 0x80; // disable JTAG interface
    MCUCR |= 0x80; // Have to tell twice

	/* Disable clock division 
     */
	clock_prescale_set(clock_div_1);

	/* Hardware Initialization 
     */
	USB_Init();
    GPIO_Init();
}

/** Event handler for the library USB Connection event. 
 */
void EVENT_USB_Device_Connect(void) {
}

/** Event handler for the library USB Disconnection event. 
 */
void EVENT_USB_Device_Disconnect(void) {
}

/** Event handler for the library USB Configuration Changed event. 
 */
void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(
        &VirtualSerial_CDC_Interface
    );
}

/** Event handler for the library USB Control Request reception event. 
 */
void EVENT_USB_Device_ControlRequest(void) {
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

void GPIO_Init(void) {

    // Pins E6 (/WE) and E2 (/OE) are outputs
    DDRE |= PORTE_PIN_MASK ;

    // Pull /OE and /WE high
    PORTE |= PORTE_PIN_MASK ;

    // All pins on port B are outputs
    DDRB = PORTB_PIN_MASK ;
    // Pull CE high, other outputs low
    PORTB = ~ CE_ACTIVE_LOW ;

    // All pins on port D are outputs
    DDRD = PORTD_PIN_MASK ;
    // Pull all pins low
    PORTD = ~ PORTD_PIN_MASK ;

    // Set the data pins of PORTC en PORTF as input (0)
    DDRC &= ~ PORTC_PIN_MASK ;
    DDRF &= ~ PORTF_PIN_MASK ;
    // Enable the pullup resistors
    PORTC |= PORTC_PIN_MASK ;
    PORTF |= PORTF_PIN_MASK ;
}

/** Process the command strings received from the host computer using a 
           simple state machine */
void ProcessBuffer(void) {

    static uint8_t state = GET_OPCODE;
    static uint8_t opcode;
    static uint16_t address;

    uint8_t byte ;

    if ( 
           !(RingBuffer_IsFull(&USB_Outgoing_Buffer)) 
        && !(RingBuffer_IsEmpty(&USB_Incoming_Buffer))
    ) {

        switch (state) {
            case GET_OPCODE :
                opcode = RingBuffer_Remove(&USB_Incoming_Buffer);
                state = GET_H_ADDRESS;
                if (opcode == SDP_ON) {
                    EnableSDP();
                    RingBuffer_Insert(&USB_Outgoing_Buffer, 'E');
                    state = GET_OPCODE ;
                } else if (opcode == SDP_OFF) {
                    RingBuffer_Insert(&USB_Outgoing_Buffer, 'D');
                    DisableSDP();
                    state = GET_OPCODE ;
                }
                break;
            case GET_H_ADDRESS:
                address = (RingBuffer_Remove(&USB_Incoming_Buffer) << 8);
                state = GET_L_ADDRESS;
                break;
            case GET_L_ADDRESS:
                address |= RingBuffer_Remove(&USB_Incoming_Buffer);
                state = READ_WRITE;
                break;
        }
    }

    if (state == READ_WRITE) {
        if (
               opcode == READ 
            && !(RingBuffer_IsFull(&USB_Outgoing_Buffer))
        ) {
            byte = ReadByteFromAddress(address) ;
            RingBuffer_Insert(&USB_Outgoing_Buffer, byte);
            state = GET_OPCODE ;
        } else if (
               opcode == WRITE 
            && !(RingBuffer_IsEmpty(&USB_Incoming_Buffer))
            && !(RingBuffer_IsFull(&USB_Outgoing_Buffer))
        ) {
            byte = RingBuffer_Remove(&USB_Incoming_Buffer) ;
            WriteByteToAddress( address, byte );
            DataPoll(address, byte);
            byte = ReadByteFromAddress(address) ;
            RingBuffer_Insert(&USB_Outgoing_Buffer, byte);
            state = GET_OPCODE ;
        }
    }
}

void WriteByteToAddress(uint16_t address, uint8_t byte) {

    PORTE |= PORTE_PIN_MASK;                // /OE and /WE are high

    // Set the address and set /CE to low
    // Note that B7 is used to set /CE
    // Address space is limited to 32K, B7 is 0
    PORTD =  (address & PORTD_PIN_MASK);          // set low address bits
    PORTB = ((address >> 8) & CE_ACTIVE_LOW ) ;  // set high address bts + /CE

    // Set the data pins to output (to high)
    DDRC |= PORTC_PIN_MASK ;
    DDRF |= PORTF_PIN_MASK ;

    // Initialize the data pins as zero
    PORTC &= ~ PORTC_PIN_MASK ;
    PORTF &= ~ PORTF_PIN_MASK ;

    // Set the data pins for the bits that are 1
    PORTF |=   byte & PORTF_PIN_MASK ;
    PORTC |= ((byte & ~ PORTF_PIN_MASK) << 4) ;

    // Pull /WE low, /OE is high
    PORTE &= WE_ACTIVE_LOW ;        // /WE

    // >100ns delay
    _delay_us(1) ;

    PORTE |= PORTE_PIN_MASK;                // /OE and /WE are high

    // Pull /CE high and release address bus
    PORTB |= ~ CE_ACTIVE_LOW ;
    PORTD |= ~ PORTD_PIN_MASK ;

}

bool DataPoll(uint16_t address, uint8_t byte) {

    // Finish if the byte value is observed when
    // reading from the address, or when 10 ms 
    // have elapsed.

    uint8_t received ;

    for (uint16_t i = 0; i < 2500; i++) {
        received = ReadByteFromAddress(address);
        if ( received == byte) {
            return received;
        }
    }

    return received;
}

uint8_t ReadByteFromAddress(uint16_t address) {

    uint8_t byte = 0;

    PORTE |= PORTE_PIN_MASK;                // /OE and /WE are high

    // Set the address
    PORTD = (address & PORTD_PIN_MASK);           // Set low address byte 
    // Set high address bits and CE is high
    PORTB = (~ CE_ACTIVE_LOW | ((address >> 8) & CE_ACTIVE_LOW));

    // Set the relevant data pins as input (0)
    DDRC &= ~ PORTC_PIN_MASK ;
    DDRF &= ~ PORTF_PIN_MASK ;
    // Enable the pullup resistors
    PORTC |= PORTC_PIN_MASK ;
    PORTF |= PORTF_PIN_MASK ;

    // /OE is pulled low
    PORTE &= OE_ACTIVE_LOW ;        // /OE
    // /CE is pulled low
    PORTB &= CE_ACTIVE_LOW ;    // /CE is low

    // wait >100 ns
    _delay_us(1);
    
    // Read the input registers into the byte
    byte = ((PINC & PORTC_PIN_MASK) >> 4) ;
    byte |= (PINF & PORTF_PIN_MASK) ;

    // /OE is pulled high (and /WE)
    PORTE |= PORTE_PIN_MASK ;

    // Pull /CE high and release address bus
    PORTB |= ~ CE_ACTIVE_LOW ;
    PORTD |= ~ PORTD_PIN_MASK ;

    return byte;
}

void EnableSDP(void) {
    // Assumption: /OE is high
    WriteByteToAddress(0x5555, 0xAA);
    WriteByteToAddress(0x2AAA, 0x55);
    WriteByteToAddress(0x5555, 0xA0);
}

void DisableSDP(void) {
    // Assumption: /OE is high
    WriteByteToAddress(0x5555, 0xAA);
    WriteByteToAddress(0x2AAA, 0x55);
    WriteByteToAddress(0x5555, 0x80);
    WriteByteToAddress(0x5555, 0xAA);
    WriteByteToAddress(0x2AAA, 0x55);
    WriteByteToAddress(0x5555, 0x20);
}


