/*
 * TODO cc1101 reset before application starts
 *
 * 2015 (C) Panagiotis Karagiannis
 * This software is distributed with the GPLv3+ Licence
 *
 * rfboot v0.6 wireless bootloader for atmega328p with the TI cc1101 chip
 * this file implements the bootloader part of rfboot
 *
 * - it is not based on optiboot or other bootloasers but is
 * written from scrach.
 * Uses the last 2 bytes of EEPROM as seed for the generation of the Initialization Vector.
 * Configurable settings can only
 * be changed at compile time.
 * Cannot be used to send code with serial port. In fact does not even touch the Rx Tx pins wich can be used
 * for other purposes (as GPIO pins) or to connect to another serial device (GPS for example)
 * RELIABILITY. This is where rfboot really shines.
 *
 * In the following note "brick" the device we mean that we cannot upload firmware
 * REMOTELY. We can always upload code with physical access to the reset pin or by power cycle the atmega chip.
 *
 * And before all we need
 * to define what we want from rfboot
 * Here is the scenario:
 * We have a atmega328p powered device buried in a wall, in a roof, in a robot, or whatever
 * We have it running an application wich we can remotely control (possibly via the RF chip but this is not necessary), and tell it
 * to reset in order to remotely update the application code. This is the intended use
 * of rfboot. There are other bootloaders doing the same, free like rfboot and most are
 * compatible with avrdude which obviously is a plus (rfboot isn't) .
 * In order to update the firmware of our appliance, we need first to tell to the app we want to reset the chip
 * in order the bootloader to take control.
 * most bootloaders (wireless or not) is that even if the upload process fails, finally the control
 * passes to the half written app.
 * Of course the app will crash with probability ~100% and then you cannot tell to
 * the application, PLEASE reboot again ! I cannot access the reset button !
 * optiboot-rf solves it by a very clever trick. Before bootloader gives control to the application
 * bootloader enables Watchdog 4 sec timer. If the application really crashes the watchdog reset MCU
 * and gives us the time to update the code. This approach requires of course that the app resets the watchdog timer periodically. Which is a good thing anyway.
 * At this stage you need hammers, screwdrivers etc to have
 * physical access to the device
 * rfboot also enables watchdog timer (see comment below)
 * rfboot however does it differently. The first info it receives from our side is the size
 * and the checksum of the application. (This is the reason I could not do rfboot to
 * be compatible with stk500/avrdude.)
 * if for some reason the upload fails, rfboot knows the app is not correctly
 * written and will never try to execute such an app. Instead it is waiting patiently
 * for you
 * to send new code until you succeed. Note that it not only withstands failures from
 * our side (bad RF signal for example, or a Laptop with the battery
 * failing at the wrong time) It also withstands failures from the avr side,
 * like power loss, brownout , hardware resets etc.
 * So the only way to brick the device is to upload a malfunctioning app witch refuses
 * to reset the device due to a software bug. Given that you already tested the app in
 * your lab and you are not trying to send the wrong app to the wrong chip, this
 * possibility can be quite small.
 *
 * Bootloader for atmega328 mcu for wireless and optionally encrypted
 * code uploads using the cc1101 module. It is very speedy
 * and according to my tests very reliable.
 * Even if you kill the upload process in the middle, rfboot detect it
 * and waits for a new upload session. Even if cut the power from the
 * atmega at the time of programming, when the power comes back rfboot
 * will detect it and will not try to start any corrupted code but
 * instead stays waiting for new code. When eventually the upload
 * process finishes, rfboot reads back the flashed program, calculates
 * the crc, and checks if it is equal with the crc came from the
 * programmer.
 * only then the program starts
 * if you like the idea of encryption, you have to also disable code
 * extraction  using an ISP programmer. The encryption key is saved
 * somewhere in the bootloader area. Using OTA encryption and at the
 * same time allow code extraction from the chip doesnt make a lot of
 * sense except when developing-debugging. Start with Lock fuse
 * setting 0x0C. I read in many forums that the code can be read with
 * the correct equipment and there are companies doing this for you for
 * a premium. So again the confidentiality of the code is not guaranteed.
 * I mainly added encryption because it was very easy. I really have no
 * idea of the level of "security" it offers
 * this bootloader can be very useful when the mcu is hard to
 * disassemble from the installed location, but you still need to make code changes.
 * we send a signal to the loaded application to (possibly save any settings to eeprom and) reboot the mcu via watchdog
 *
 * Then the programmer (the equivalent of avrdude) sends the code_size the CRC and finally the code in packets of 32 bytes
 *
 * checksum of the app. The bootloader writes the code to flash and in the end reads the writen flash
 * contents and recalculates the CRC. if is correct, jumps to the newly uploaded
 * app. The upload process is happening in reverse order and if for some reason is interrupted, then the first pages of flash have the known 0xFF pattern of an unprogrammed chip,
 *
 * Of course if the flash is empty the bootloader stays waiting for code upload
 *
 * if the mcu reset with the reset pin rfboot wait for code upload for 0.25 sec.
 *
 * uses cc1101 module for code upload
 *
 * rfboot cannot initialize a reset by itself. The duty for this is in the application. I consider this not a problem  because of the intended use of this bootloader.  if you are developing a non "hello world" program the hassle to add some code for this function is not an issue. Also -generally speaking- a real life program probably wont like unconditional resets with the hardware reset pin because maybe some variables need to be saved in EEPROM. So a polite "reset" request to the application has better results for a useful in real life,  but still possible to update application.
 *
 *
 * WARNING ! Don't install this bootloader to an arduino board. It does not use serial UART. This means, if you burn this bootoader to a ProMini 3.3V for example, you will not be able to program it over serial any more. Only via cc1101 module.
 *
 * rfboot is designed to be used on bare atmega328p chips. Of course you can still -as I do- develop Arduino applications, a bootloader is code agnostic.
 *
 * WARNING ! rfboot cannot protect you from buggy code. You can upload anything to the MCU, a prog that freezes or fails to communicate with you or just ignores a reboot command from your side. Or a code written for another station/project. Then you need physical access to the mcu to be able to reset it. test your code first.

 * The hardware for rfboot is typical is atmega328p 8Mhz@3.3V
 * because CC1101 chip does not tolerate 5V on any pin and atmega328
 * DOES NOT RUN at 16Mh at 3.3V
 * FUSES = E2 DA 05 without crystal
 * FUSES = E2 DA 05 with crystal
 * TODO
 *
 *
 *
 * The fact that rfboot does not initialize UART has an interesting effect.
 * You can use Hardware Serial to connect to a serial device (a GSM
 * modem for example) instead of relying on software serial. My tests show that software serial must run on slow speeds like 9600 or 4800 to be reliable, especially at 8Mhz or 1Mhz.
 *
 * not compatible unfortunately with avrdude. rfboot uses a companion program rftool instead of avrdude. Moreover it also needs a USB to RF adapter connected to the PC. This module is easy enough to build but adds to the effort to make this bootloader working in the first time.
 *
 * Arduino users: You can continue to use arduino sketces because a bootloader is code agnostic.
 *
 * Encrypts the packets on the air see
 * https://github.com/pkarsy/rfboot/wiki/Encryption
 *
 * TODO cc1101 reset before app start
 * */

#include <avr/io.h>
#include <util/delay.h>
#include <avr/boot.h>
#include <inttypes.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include <avr/wdt.h>
#include <util/atomic.h>
#include <util/crc16.h>

#include "xtea.h"

#define byte uint8_t


// Parameters that are different in every rfboot project
// RF Channel , Syncword , XTEA key
// The file is generated by "rftool create ProjName"
#include "rfboot_settings.h"


// CC1101 has 64 bytes buffer but we restrict it to 32 bytes. If the reception is not ideal
// long packets have greater probability to be corrupted
#define PAYLOAD 32

// this is a random enough number contained in the first packet.
// the programmer sends it and rfboot requires this number to be
// in the start of the header in order to continue. This protects
// the bootloader from using any random packet, if happens
// to be in the air, as a header
// Compatibility reasons, require this number not to be changed
// It is crucial that rfboot and rftool (the flasher) agree
#define START_SIGNATURE 0xd20f6cdf


// this is the structure of the first packet and contains the header.
// Total is 11 bytes. the other 21 bytes are unused.
// TODO require the 21 bytes to be 0
struct start_packet {
	uint32_t start_signature1;
	uint16_t app_size;
	uint16_t app_crc;
	uint16_t app_crc2;
	uint16_t round;
	uint32_t start_signature2;
};

// The XTEA cipher uses blocks of 8 bytes
#define XTEA_BLOCK_SIZE 8

// rfboot is designed to be a little smaller than this size
// FUSES for this bootloader size
// TODO
#define BOOTLOADER_SECTION_SIZE 4096

// This is the status codes rfboot is sending back to the programmer
const uint8_t RFB_NO_SIGNATURE = 1;
const uint8_t RFB_INVALID_CODE_SIZE = 2;

//const uint8_t RFB_ROUND_IS = 3;
const uint8_t RFB_SEND_PKT = 4;

const uint8_t RFB_WRONG_CRC=5;
const uint8_t RFB_SUCCESS=6;

// rfboot approach to start the application code is to trigger a Watchdog Reset
// and after this the application
// code starts. With this mechanism we can be sure that the state of the MCU is correct
// This is also the method optiboot uses.
// However unlike optiboot, rfboot must know who is triggered the watchdog reset.
// If triggered from the app rfboot must wait for firmware update.
// If triggered from rfboot itself then the application code should start

// The solution is to use a uninitialized global variable
// (witch basically means a specific location in the atmega RAM)
// the contents of which will inform rfboot who is triggered the reset.

// if this (".noinit" on purpose) variable has the value RESET_BY_RFBOOT
// rfboot knows that ITSELF triggered a wdog reset with
// the intention to start the application and not load itself again.
// memory is preserved across resets so appart from EEPROM (witch
// we don't want to touch) is the only way to send info from one instance
// of rfboot to the next (after a reset)
// this variable cannot be declared register !!!
const uint32_t RESET_BY_RFBOOT=0xd8317bc2;
volatile uint32_t reset_origin __attribute__ ((section (".noinit")));

// there is a very small possibility this mechanism can fail: The app
// can trigger a wdog reset and happens the memory contents of this variable
// to be equal with RESET_BY_RFBOOT due to random manipulations in memory.
// if happens, the upload can always be done with the hardware reset.

// the next variable is also preserved between 2 continuous rfboot executions
// It is used to report correct reset causes EXTRF WDRF OWR BROWNOUT to the app
// (via the "r2" register) like optiboot
// Technically a watchdog reset is triggered from rfboot itself
// before the app starts. But we don't of course want to report this to the
// application code.

volatile uint8_t previous_reset_cause __attribute__ ((section (".noinit")));

// if we don't want to be a register then we MUST put a line
//  __asm__ __volatile__ ("mov r2, %0\n" :: "r" (mcusr_mirror));
// inside function reset_mcu();
// right before jmp
register uint8_t mcusr_mirror asm("r2") __attribute__ ((section (".noinit")));

// recommended code from avr-libc documentation
// MCUSR manipulation is VERY tricky so better leave it as is
void get_mcusr(void) __attribute__((naked)) __attribute__((section(".init3")));
void get_mcusr(void) {
	mcusr_mirror = MCUSR;
	MCUSR = 0;
	// rfboot does always enables a 2 sec Watchdog timer. So even if we manage to
	// send buggy code to the target, watchdog timer will finally reset the
	// device allowing, to upload new code, WITHOUT the need of physically contact
	// The application at normal operation will
	// have the duty to reset the WDOG timer periodically, witch is a good
	// practice anyway.
	//wdt_enable(WDTO_2S);
}

#ifdef USE_ENTROPY
	#include "entropy.h"
#endif

#include "cc1101.h"

// a flag that a wireless packet has been received

register bool data_ready asm("r3") __attribute__ ((section (".noinit")));

// in this packet we store data coming from RF
CCPACKET ccpacket __attribute__ ((section (".noinit")));
uint8_t* packet = ccpacket.data;

// Generated by CC1101 when receives SyncWord (or sends a packet)
ISR (INT0_vect)
{
	/* interrupt code here */
	data_ready = true;
}

#define get_data() cc1101_receiveData(&ccpacket)

void radio_init(void) {
	cc1101_init();
	cc1101_setChannel(RFBOOT_CHANNEL);
	cc1101_setSyncWord(RFB_SYNCWORD[0],RFB_SYNCWORD[1]);
	disableAddressCheck();
	get_data();

	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		EICRA |= (1 << ISC01);    // set INT0 to trigger on falling edge
		EIMSK |= (1 << INT0);     // Turns on INT0
	}

	__asm__("nop\n\t");
	__asm__("nop\n\t");
	data_ready = false;

}

// This is used to send packets. Probably no need for separate
// in and out packets
// TODO low priority
CCPACKET outpkt;
void send_pkt(uint8_t msg, uint16_t data) {

	outpkt.length=3;
	outpkt.data[0]= msg ;
	outpkt.data[1]= data & 0xff ;
	outpkt.data[2]= data >> 8 ;
	cc1101_sendData(outpkt);
	while (! data_ready);
	data_ready = false;
}

void  send_iv(const uint32_t* iv) {
	outpkt.length=8;
	memcpy(outpkt.data,(byte*)iv,8);
	cc1101_sendData(outpkt);
	while (! data_ready);
	data_ready = false;
}

// Never returns, so "naked" and "noreturn" attributes don't hurt and reduce
// code size
void reset_mcu() __attribute__ ((naked))  __attribute__ ((__noreturn__));
void reset_mcu() {

	// rfboot will boot in a while
	// without remembering anything
	// But reset_origin variable will
	// contain the magic value RESET_BY_RFBOOT
	// so rfboot will know it is time to start the app
	reset_origin = RESET_BY_RFBOOT;

	// We save mcusr_mirror. after 15ms rfboot will put this value
	// to the r2 register before it gives control to the application
	previous_reset_cause = mcusr_mirror;

	// we enable watchdog at 15ms
	wdt_enable(WDTO_15MS);
	// we stay here until watchdog resets MCU
	while(1);
}

// we did it a function as we call the same thing a lot of times
void page_erase(uint16_t page) {
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		boot_spm_busy_wait();
		boot_page_erase(page);
	}
}

// the same
void flash_read_enable() {
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		boot_spm_busy_wait();
		boot_rww_enable();
	}
}

int main(void) {

	// Disable interrupts.
	cli();

	// if the first 2 bytes of flash are 0xff, rfboot consider the flash
	// empty and stays waiting for code indefinitely
	// I searched the Internet and as I understand it, there is no
	// 0xffff AVR documented opcode at least for atmega328. So there is no
	// possibility an application will start with 0xffff by chance.

	if ( (pgm_read_word(0) != 0xffff) && (mcusr_mirror & _BV(WDRF) ) && (reset_origin == RESET_BY_RFBOOT) )
	{
		///////////////////////////////////
		// - We have an installed application as the first word differs from 0xffff
		// - We have watchdog reset
		// - reset_origin variable says that the reset triggered by the bootloader itself
		//   As a consequence : It is time to start the application code
		///////////////////////////////////

		// it is absolutely necessary to alter the reset_origin variable to something different
		// than RESET_BY_RFBOOT because there is a very high
		// probability that the app will not overwrite the memory
		// region referring to this variable. If the application
		// triggers a WDOG reset, rfboot will find that
		// reset_origin == RESET_BY_RFBOOT and believing it itself
		// is the cause of the reset will jump directly to the application,
		// causing firmware updates to fail (when triggered from the application)
		reset_origin = 0;

		// mcusr_mirror is the register "r2" which can be used by the
		// application to examine the cause of the reset. We set "r2" to
		// be the reset cause of the previous reset, otherwise the application
		// will see as reset cause always WDOG
		mcusr_mirror = previous_reset_cause;

		// finally we start the application
		// note that we come from a WDOG reset
		// so the MCU registers I/O etc are in pristine state
		// The app has responsibility of initialize the rf module
		// the application must reset the watchdog every 2 secs at least
		// or to change watchdog settings
		asm("jmp 0");
	}

	// The bootloader uses interrupts for 2 reasons:
	// to use CC1101 (GDO0 is interrupt)
	// to gather entropy (with WDIE). Not implemented. We use EEPROM at the moment

	// Enable change of interrupt vectors
	// I guess without this, next command will have no effect
	MCUCR = (1<<IVCE);
	// Move interrupts to boot flash section
	// without this interrupts will go to the application code. BAD
	MCUCR = (1<<IVSEL);

	//
	// here we gather entropy to be ready when the radio is on
	//
	// TODO
	uint32_t iv[2];
	#ifdef USE_ENTROPY
		TCCR1B |= (1 << CS10) ; // Set up timer1 with prescaler 1
		wdtSetup();
		byte* p=iv;

		for (byte i=0;i<8;i++) {
			while (! gatherEntropy() ) ;
			p[i]=result;
		}
	#else


		// Here the IV is created using the last 2 bytes of EEPROM
		uint16_t round = eeprom_read_word(E2END-1)+1;
		iv[0]=round;
		iv[1]=COMPILE_TIME;
		xtea_encipher( (byte*)iv,XTEAKEY);
	#endif


	wdt_enable(WDTO_2S);

	// here we set RF channel, SyncWord etc
	radio_init();
	// sei(); radio_init() does it
	{
		// 250 iterations before give up
		uint8_t i=250;
		while(1) {

			if (data_ready) {
				if ( get_data() == 4 && ccpacket.crc_ok) {
					data_ready = false;
					break;
				}
				else i=250;
			}

			i--;
			if (!i) reset_mcu();
			_delay_us(1000);

		}
		uint32_t* p = packet;
		if (*p != START_SIGNATURE) reset_mcu();
	}

	//ccpacket.length = 8;
	//cc1101_sendData(ccpacket);
	//memcpy(iv,packet,8);
	//while (! data_ready);
	//data_ready = false;
	// The packet with the IV is out
	send_iv(iv);


	// the struct is used to extract upload parameters from the first packet
	struct start_packet *spacket = packet;

	{
		uint8_t i=250;

		while (true) {

			if (data_ready) {
				if ( get_data() == PAYLOAD && ccpacket.crc_ok) {
					data_ready = false;

					break;
				 }
				 else {
					 i=250;
					 wdt_reset();
				 }
			}

			i--;
			if (!i) reset_mcu();
			_delay_us(1000);
		}
	}

	//
	// OK we got the header
	//

	// reset watchdog to be sure
	wdt_reset();

	// We check if the first packet contains the correct signature
	// The signature is 32 bit, and transmitted in 2 places in the packet
	// so is almost impossible for an unrelated
	// packet send by mistake (from an earlier upload for example)
	// to confuse the bootloader

	for (uint8_t i=0; i<=3; i++) {
		xtea_decipher_cbc( (uint32_t*)(packet+i*XTEA_BLOCK_SIZE) , XTEAKEY,iv );
	}

	if ( (spacket->start_signature1 == START_SIGNATURE) && (spacket->start_signature2 == START_SIGNATURE) ) {
		// sig1 and sig2 OK
	}
	else {
		// We were unable to find the signature so we give up.
		// Probably the programmer used encryption with the wrong key or IV
		send_pkt(RFB_NO_SIGNATURE,0xffff);
		reset_mcu();
	}

	// We store here the size of the incoming application
	// this info is in the first packet
	// now using spacket pointer we extract info from the packet
	uint16_t app_size = spacket->app_size;

	// Here some basic checks for a valid app size.
	// app_size is always a multiple
	// of PAYLOAD. Trailing bytes are padded with 0xff
	// This is ensured by the programmer (rftool+usb2rf)
	if ( (app_size > (FLASHEND-BOOTLOADER_SECTION_SIZE+1) ) ||
	(app_size%PAYLOAD!=0) || (app_size==0) )
	{
		send_pkt(RFB_INVALID_CODE_SIZE,0xffff);
		reset_mcu();
	}

	// remote programmer sends 2 CRCs (16bit each)
	// witch rfboot uses after the flash, to check if the code is OK.

	// this is the CRC16 of the application's opcode
	uint16_t remote_crc = spacket->app_crc;

	// this is the CRC16 calculated with the bytes in reverse order
	// again this is probably an overkill but it is too
	// simple to implement and offers many orders of magnitude
	// better error detection
	uint16_t remote_crc2 = spacket->app_crc2;

	// This variable will point to the flash location to be written
	uint16_t app_idx=app_size;

	// Here we send the request for the first packet
	// the packets are transmitted and received in reverse order
	// from the last 32 byte packet to the first
	send_pkt(RFB_SEND_PKT, app_idx);

	// Before any write, we erase the first SPM page. If for some reason
	// the upload process fails, the first page will contain
	// 0xff and  rfboot will refuse to start the corrupted code.
	// see the start of main() how this is implemented

	// TODO
	//if (pgm_read_word(0) != 0xffff) {
	#ifndef USE_ENTROPY
		eeprom_update_word(E2END-1, round);
		eeprom_busy_wait();
	#endif

	page_erase(0);
	//}

	// one loop per SPM page (in reverse order)
	do
	{
		wdt_reset();
		// if i.e. app_idx == 256 we are going to burn the flash from 128-255
		// if app_idx == 128 we are going to burn the flash from 0-127 etc
		// We erase this SPM page unless it is the 0-127 page
		// which is erased before this loop starts
		if (app_idx>SPM_PAGESIZE) { // this means _not_ the first page
			uint16_t spm_page=(app_idx-1)/SPM_PAGESIZE*SPM_PAGESIZE;
			page_erase(spm_page);
		}
		// one loop per network packet 32 bytes == 4 xtea blocks == 2 AES blocks
		do
		{
			{ // we send a request and expect a data packet
				uint16_t i=40*10-1;

				while (true) {
					// every 20ms we send a request
					if ( (i%40)==0) send_pkt(RFB_SEND_PKT, app_idx);

					if (data_ready) {

						if ( (get_data()==PAYLOAD) && ccpacket.crc_ok) {
							break;
						}
						else i=40*10+1;
						wdt_reset();

					}
					i--;
					if (i==0) reset_mcu();
					_delay_us(500);
				}
			}

			// We send a request for the next packet (unless this was the last one)
			// before even consume this
			// one. This is for efficiency. The answer will need some time
			// to arrive, so is better to do some work in the meantime
			if (app_idx-PAYLOAD>0) send_pkt(RFB_SEND_PKT, app_idx-PAYLOAD);

			// We decrypt the packet. 4 XTEA blocks
			for (uint8_t i=0; i<=3; i++) {
				xtea_decipher_cbc( (uint32_t*)(packet+i*XTEA_BLOCK_SIZE) , XTEAKEY ,iv );
			}
			// at this point the packet is in cleartext

			// next thing is to wtite it in flash
			// the following code is basically what avr-gcc documentation
			// suggests
			ATOMIC_BLOCK(ATOMIC_FORCEON) {
				boot_spm_busy_wait();
				uint8_t j=PAYLOAD;
				do {
					app_idx-=2;
					j-=2;
					boot_page_fill(app_idx, *(uint16_t*)(packet+j));
				} while(j);
			}

		// we repeat the above 4 times until an SPM (128 bytes) page fills
		} while (app_idx%SPM_PAGESIZE);
		//
		// Now we filled a full SPM page we have to burn it in flash
		//
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			boot_spm_busy_wait();
			boot_page_write(app_idx);
		}
	// We repeat writing SPM pages
	// if app_idx becomes 0 we have just written the SPM page 0-127
	// and the whole application is written to the flash
	} while (app_idx);

	// We got all RF packets
	// the upload process is finished

	// Now we are going to check if the CRC of the written code are the same with
	// the CRC sent from the remote programmer. So we need to enable
	// flash read

	flash_read_enable();

	// as usual the crc's are initialized with zero
	uint16_t local_crc=0;
	uint16_t local_crc2=0;
	for (uint16_t i = 0; i < app_size ; i++) {

		// the first crc calculated reading the flash from start to end.
		local_crc = _crc16_update(local_crc,pgm_read_byte(i));

		// the second crc is calculated reading the flash from end to start
		// The 2 crc's according to my (non scientific) tests seem independent
		// offering an effective 32bit CRC
		// so offer a ~100% probability that flash is correctly
		// written. I am including a C program to test my hypothesis. If anyone has a
		// formal mathematical proof, I am very interested to know so
		// send me a note to  include a link here.
		// Note also that the use of 2 CRC16 is
		// probably an overkill but it costs only 40-50 bytes in flash
		// and minimal MCU time.
		local_crc2 = _crc16_update(local_crc2,pgm_read_byte(app_size-1-i));
		// Note: I tried a CRC32 once, but bloated the code badly
	}

	// Now both crc's are calculated, we do the test

	if ( (remote_crc != local_crc) || (remote_crc2 != local_crc2) ) {
		// if the crc's dont match, we erase the first SPM page again, so rfboot wont try
		// to start a corrupted code. Note that this should be rare, since
		// the network packets are already protected with CRC.

		page_erase(0);
		send_pkt(RFB_WRONG_CRC,0);

		//I am not sure if this is needed but it doesn't hurt either
		flash_read_enable();
	}
	else {
		send_pkt(RFB_SUCCESS,0);
	}

	// Reset MCU. If flash is correctly wtitten the application can start, not
	// directly but using a Watchdog reset. This ensures
	// that the loaded app will see I/O pins etc are in their default state
	//
	// Of course , if the process fails, which at
	// this point means the crc check failed, rfboot
	// will wait indefinitelly for new code
	reset_mcu();
}
