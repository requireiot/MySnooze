MySnooze
===============================================
extended sleep function for MySensors

## Overview

This library provides one public function that extends the MySensors sleep function:
```C
int8_t snooze(const uint32_t sleepingMS, const bool smartSleep)
```
It has the same signature and same basic behavior as the original MySensors `sleep()` function, with a few differences:

| Topic      | MySensors `sleep()`    | This library, `snooze()` |
|-------     | ---------------------  | ------------------------ |
| `millis()` function | not corrected for sleep duration | reasonably correct after sleep |
| ADC state | always ON after sleep | restored to pre-sleep state |
| code execution during "sleep" | none | periodic function call every 8s |
| interrupts | uses Arduino `attachInterrupt()` and `detachInterrupt()`, defines own ISR | can use any ISR, communication via global variable

Sleeping for a defined time uses the watchdog timer, both in the original and in my library. If you request sleep for 30 minutes, the processor actually wakes up every 8s, and then goes back to sleep. 

The `snooze()` function calls a function `int8_t tick(void)` every 8s, you implement that function, it can only do simple things like polling a pin. No UART actions and no A/D conversions, please. The return value of the function indicates whether sleep should continue (==0), or end now (!=0).

If you don't implement that function, nothing gets called.

During sleep, i.e. once every 8s, the code also checks the global variable `wokeUpWhy`, if an interrupt service routine has set it to !=0, then sleep will end immediately.
