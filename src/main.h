#ifndef _MAIN
#define _MAIN

#include <stdio.h>
#include <stdbool.h>

#if (defined(FUNCONF_USE_DEBUGPRINTF) && (FUNCONF_USE_DEBUGPRINTF)) || (defined(FUNCONF_USE_UARTPRINTF) && (FUNCONF_USE_UARTPRINTF))
    #define CONFIG_DEBUG_ENABLE_LOGS (0)
    #pragma message("---WITH PRINT!---")
#else
    #define CONFIG_DEBUG_ENABLE_LOGS (1)
    #pragma message("---NO PRINT!---")
#endif


#include "log.h"


typedef uint32_t Mem_t;

// : maximum number of tasks (0-254, default=1)
// currently set in platformio.ini
#define N_TASKS 4
// : maxmimum number of message queues (0-254, default=0)
#define N_QUEUES 0
// : maximum number of semaphores (0-254, default=0)
#define N_SEMAPHORES 0
// : maximum number of events (0-254, default=0)
#define N_EVENTS 2
// : should round robin scheduling be used ? (0)
#define ROUND_ROBIN 0

#include "cocoos.h"


// SDA PC1  
// SCL PC2 
// PC3 - hall sensor, active - low
#define HALL PC3
// PC5 - button, active - high
#define BUTTON PC5
/*
For ch32v003F4P6 (20 pin)

Disp SDA    -> 11
Disp SCL    -> 12
Hall sensor -> 13
Button      -> 15

SWIO (PD1)  -> 18 programming
VDD (+3.3v) -> 9
VSS (GND)   -> 7
*/

static int16_t counter = 0;

#define TIMEOUT_GO_TO_SLEEP (FUNCONF_SYSTEM_CORE_CLOCK/960)

//
static uint8_t task_hall_sensor;
static uint8_t task_display;
static uint8_t task_disp_sleep;
static uint8_t task_keyboard;

static Evt_t evt_wheel_sensor;
static Evt_t evt_display_wakeup;

#endif