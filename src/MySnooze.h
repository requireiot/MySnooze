/**
 * @file       MySnooze.h
 * 
 * Author         Bernd Waldmann
 * Creation Date: 5-Jun-2017
 * Tabsize		 : 4
 *
 * This Revision: $Id: MySnooze.h 1264 2021-10-07 09:41:42Z  $
 */
 
/*
   Copyright (C) 2017,2021 Bernd Waldmann

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/

   SPDX-License-Identifier: MPL-2.0
*/

/**
	@file bwSleep.h
	@brief new sleep routines for MySensors

    call bw_sleepEx(ms), there are 3 ways to wake up:

    1. after `ms` milliseconds, sleep will end, (standard behavior from MySensors library)
       and return MY_WAKE_UP_BY_TIMER

    2. every 8s or less during sleep, function tick() will be called, if it exists,
       and if it returns a value !=0, then sleep will end, and that value returned

    3. if an application-defined ISR sets global variable `wokeUpWhy`
       to a value != INVALID_INTERRUPT_NUM, then sleep will end, and the value in `wokeUpWhy` returned

    if ms==0, sleep forever (i.e. until interrupt)
    if ms!=0, sleep for defined time, and correct millis() counter as best as possible
*/

#ifndef __BW_SLEEP2_H
#define __BW_SLEEP2_H

//----- new sleep function --------------------------------------------------

// application ISR must set this variable to !=0
extern volatile uint8_t wokeUpWhy;

/**
  * @brief Main sleep function, modified from mysensors.
  * 
  * @param ms    = desired sleep time in milliseconds
  * @param smart = if true, notify controller before going to sleep
  * @return 0 if sleep ended normally, or value returned by tick(), or wokeUpWhy value set by user ISR
  */
int8_t snooze( const uint32_t ms, const bool smart=false );

/**
  * @brief Called at least every 8s during sleep. Must be defiend by application.
  * @return !=0 to wake up
 
  * - don't use ADC in this callback function, it may be disabled
  * - don't use UART in this callback function 
  * (we may go back to sleep before all characters have been sent)
  */
int8_t tick(void) __attribute__((weak));


#endif // __BW_SLEEP2_H
