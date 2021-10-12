/**
 * @file 		  MySnooze.cpp
 *
 * Author		: Bernd Waldmann
 * Creation Date: 5-Jun-2017
 * Tabsize		: 4
 *
 * This Revision: $Id: MySnooze.cpp 1264 2021-10-07 09:41:42Z  $
 */

/*
   Copyright (C) 2017,2021 Bernd Waldmann

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/

   SPDX-License-Identifier: MPL-2.0
*/

/*
 * Relies on MySensors, Created by Henrik Ekblad <henrik.ekblad@mysensors.org>.
 * Note that the MySensors library is under GPL, so 
 * - if you want to combine this source code file with the MySensors library 
 *   and redistribute it, then read the GPL to find out what is allowed.
 * - if you want to combine this file with the MySensors library and only 
 *   use it at home, then read the GPL to find out what is allowed.
 * - if you want to take just this source code, learn from it, modify it, and 
 *   redistribute it, independent from MySensors, then you have to abide by 
 *   my license rules (MPL 2.0), which imho are less demanding than the GPL.
 */

/** 
	@brief sleep function for MySensors projects, extended
 */

#include <util/atomic.h>
#include <avr/wdt.h>

#include "MyConfig.h"
#include "core/MySensorsCore.h"
#include "core/MyTransport.h"
#include "core/MyIndication.h"
#include "hal/architecture/MyHwHAL.h"
#include "hal/architecture/AVR/MyHwAVR.h"

#include "MySnooze.h"


#define WDTO_SLEEP_FOREVER		(0xFFu)
#define INVALID_INTERRUPT_NUM	(0xFFu)

// debug output
#if defined(MY_DEBUG_VERBOSE_CORE)
#define CORE_DEBUG(x,...)	DEBUG_OUTPUT(x, ##__VA_ARGS__)	//!< debug
#else
#define CORE_DEBUG(x,...)									//!< debug NULL
#endif

//----- external references 

extern volatile unsigned long timer0_millis;	// defined in Arduino core wiring.c

//----- public variables ----------------------------------------------------

volatile uint8_t wokeUpWhy = 0;

//----- local functions -----------------------------------------------------

static uint8_t ADENsave;

/** 
 * @brief call this function once before calling _doPowerDown multiple times 
 */
static inline
void _pre_doPowerDown()
{
	// disable ADC for power saving
	ADENsave = ADCSRA & (1 << ADEN);
	ADCSRA &= ~(1 << ADEN);
}


/**
 * @brief call this function once after calling _doPowerDown multiple times 
 */
static inline
void _post_doPowerDown()
{
	// enable ADC
	ADCSRA |= ADENsave;
}


/** 
 * @brief setup watchdog, call sleep_cpu(), restore watchdog 
 * @param wdto = sleep duration (SLEEP_8S, SLEEP_4S etc) or WDTO_SLEEP_FOREVER
 */
static
void _doPowerDown(const uint8_t wdto)
{
    uint8_t WDTsave = WDTCSR;
	if (wdto != WDTO_SLEEP_FOREVER) {
		wdt_enable(wdto);
		// enable WDT interrupt before system reset
		WDTCSR |= (1 << WDCE) | (1 << WDIE);
	} else {
		// if sleeping forever, disable WDT
		wdt_disable();
	}
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	cli();
	sleep_enable();
#if defined __AVR_ATmega328P__
	sleep_bod_disable();
#endif
	sei();
	// Directly sleep CPU, to prevent race conditions! (see chapter 7.7 of ATMega328P datasheet)
	sleep_cpu();
	sleep_disable();
	cli();
	wdt_reset();
	// enable WDT changes
	WDTCSR |= (1 << WDCE) | (1 << WDE);
	WDTCSR = WDTsave;
	sei();
}


/**
 * @brief   Sleep once using watchdog timer.
 * 
 * @param wdto  sleep duration (SLEEP_8S, SLEEP_4S etc) or WDTO_SLEEP_FOREVER
 * @param ms    sleep duration in milliseconds, used to adjust millis() counter
 * @return      0 if timer expired or !=0 if interrupt 
 */
static
int8_t myPowerDown(const uint8_t wdto, unsigned long ms)
{
	_doPowerDown(wdto);
	if (wokeUpWhy)
		return wokeUpWhy;
	ATOMIC_BLOCK(ATOMIC_FORCEON)
	{
		// adjust variable used by Arduino millis() library function
		timer0_millis += ms;
	}
	return 0;
}


/**
 * @brief Sleep for an extended period of time, may be longer than max watchdog period.
 * One sleep may consist of multiple naps (calls to `myPowerDown()`), in 8s increments, until 
 * desired sleep time is expired or other break condition has occured.
 * After each nap, calls function `tick()` if it is defined, and ends sleep immediately if
 * `tick()` returns !=0
 * 
 * @param ms    Desired sleep duration in milliseconds
 * @return      0 if timer expired or !=0 if interrupt 
 */
static
int8_t myInternalSleep(unsigned long ms)
{
  int8_t why;
	// Let serial prints finish (debug, log etc)
#ifndef MY_DISABLED_SERIAL
	MY_SERIALDEVICE.flush();
#endif

  while (ms >= 8000) {
    if ((why=myPowerDown(WDTO_8S,8000))) return why;
		if (tick && (why = tick())) return why;
    ms -= 8000;
  }
  if (ms >= 4000) {
    if ((why=myPowerDown(WDTO_4S,4000))) return why;
		ms -= 4000;
  }
  if (ms >= 2000) {
    if ((why=myPowerDown(WDTO_2S,2000))) return why;
		ms -= 2000;
  }
  if (ms >= 1000) {
    if ((why=myPowerDown(WDTO_1S,1000))) return why;
		ms -= 1000;
  }
  if (ms >= 500) {
    if ((why=myPowerDown(WDTO_500MS,500))) return why;
		ms -= 500;
  }
  if (ms >= 250) {
    if ((why=myPowerDown(WDTO_250MS,250))) return why;
		ms -= 250;
  }
  if (ms >= 120) {
    if ((why=myPowerDown(WDTO_120MS,120))) return why;
		ms -= 120;
  }
  if (ms >= 60) {
    if ((why=myPowerDown(WDTO_60MS,60))) return why;
		ms -= 60;
  }
  if (ms >= 30) {
    if ((why=myPowerDown(WDTO_30MS,30))) return why;
		ms -= 30;
  }
  if (ms >= 15) {
    if ((why=myPowerDown(WDTO_15MS,15))) return why;
		ms -= 15;
  }
  return tick ? tick() : 0;
}


/** 
  * @brief Sleep, wake up after `ms` ms, or after user interrupt set flag, or after call to tick() returned !=0 .
  */
static
int8_t mySleep( uint32_t ms )
{
  	int8_t why;
	// Disable interrupts until going to sleep, otherwise interrupts occurring between here
	// and sleep might cause the ATMega to not wakeup from sleep as interrupt has already be handled!
	cli();
  	wokeUpWhy = 0;
  	_pre_doPowerDown();

	if (ms>0) {
		// sleep for defined time
		why = myInternalSleep(ms);
	} else {
		// sleep until ext interrupt triggered
		_doPowerDown(WDTO_SLEEP_FOREVER);
    	why = wokeUpWhy;
	}
  	// Clear woke-up-by-interrupt flag, so next sleeps won't return immediately.
	wokeUpWhy = 0;

  	_post_doPowerDown();

  	return why ? why : MY_WAKE_UP_BY_TIMER;
}

//----- public functions

/**
 * @brief  Sleep for a defined time or forever, wake up when interrupt or when tick() returned !=0.
 * Uses watchdog timer to sleep, periodically calls `tick()` function if defined
 * 
 * @param sleepingMS  sleep time in milliseconds, or 0 for 'forever'
 * @param smartSleep  if true, notify gateway before going to sleep
 * @return int8_t     reason for return from sleep, 
 *                    value returned by tick(),
 *                    or MY_WAKE_UP_BY_TIMER,
 *                    or MY_SLEEP_NOT_POSSIBLE
 */
int8_t snooze(const uint32_t sleepingMS, const bool smartSleep)
{
	CORE_DEBUG(PSTR("MCO:SLP:MS=%lu,SMS=%d\n"), sleepingMS, smartSleep);
	uint32_t sleepingTimeMS = sleepingMS;
	// Do not sleep if transport not ready
	if (!isTransportReady()) {
		CORE_DEBUG(PSTR("!MCO:SLP:TNR\n"));	// sleeping not possible, transport not ready
		const uint32_t sleepEnterMS = hwMillis();
		uint32_t sleepDeltaMS = 0;
		while (
			!isTransportReady()
			&& (sleepDeltaMS < sleepingTimeMS)
			&& (sleepDeltaMS < MY_SLEEP_TRANSPORT_RECONNECT_TIMEOUT_MS)
			) {
			_process();
			sleepDeltaMS = hwMillis() - sleepEnterMS;
		}
		// sleep remainder
		if (sleepDeltaMS < sleepingTimeMS) {
			sleepingTimeMS -= sleepDeltaMS;		// calculate remaining sleeping time
			CORE_DEBUG(PSTR("MCO:SLP:MS=%lu\n"), sleepingTimeMS);
		} else {
			// no sleeping time left
			return MY_SLEEP_NOT_POSSIBLE;
		}
	}

	if (smartSleep) {
		// notify controller about going to sleep
		(void)sendHeartbeat();
		wait(MY_SMART_SLEEP_WAIT_DURATION_MS);		// listen for incoming messages
	}

	CORE_DEBUG(PSTR("MCO:SLP:TPD\n"));	// sleep, power down transport
	transportDisable();
	setIndication(INDICATION_SLEEP);

	int8_t result = mySleep(sleepingTimeMS);

	setIndication(INDICATION_WAKEUP);
	CORE_DEBUG(PSTR("MCO:SLP:WUP=%d\n"), result);	// sleep wake-up
	return result;
}
