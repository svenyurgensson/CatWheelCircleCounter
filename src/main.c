// what type of OLED - uncomment just one
//#define SSD1306_64X32
#define SSD1306_128X32
//#define SSD1306_128X64

#include "ch32v003fun.h"
#include <stdio.h>
#include "ssd1306_i2c.h"
#include "ssd1306.h"
#include "ULWOS2.h"
#include "ch32v003_flash.h"

ULWOS2_DEFINE_SIGNAL(RESUME_FROM_SLEEP);

static int16_t counter = 0;
static bool EVENT_RESUME = false;
// each 10th count will be saved
#define COUNTS_BEFORE_SAVES (10)

tULWOS2Timer ULWOS2_getMilliseconds(void)
{
    return SysTick->CNT / DELAY_MS_TIME;
}   

// Threads

// PC3 connected to HallSensor
// increments wheel counter
void ThreadHallSensor(void) 
{
    funPinMode(PC3, FUN_INPUT);
    ULWOS2_THREAD_START();
    EVENT_RESUME = false;
    
    while(1)
    {
        while(funDigitalRead(PC3) == 1) {
            ULWOS2_THREAD_YIELD();
        }
        while(funDigitalRead(PC3) == 0) {
            ULWOS2_THREAD_YIELD();
        }
        counter++; 

        if (counter % COUNTS_BEFORE_SAVES == 0) {
            printf("\nsaving counter %d", counter);
            // save counter to flash
            flash_unlock();
            flash_unlock_option_bytes();
            flash_write_option_byte_16_bits(counter);
            flash_lock();
        }
        EVENT_RESUME = true;
    }
}

// SSD1306-based OLED in I2C interface mode to pins PC1 (SDA) and PC2 (SCL)
//
void ThreadDisplayCounter(void)
{
    ULWOS2_THREAD_START();
    char buffer[8];

	if (!ssd1306_i2c_init())
	{
		ssd1306_init();		
        ssd1306_setbuf(0);  
        uint16_t prev_counter = counter;

        while (1)
		{
            sprintf(buffer, "%06d\0", counter);
            ssd1306_drawstr_sz(0,16, buffer, 1, fontsize_16x16);
            ssd1306_refresh();	
            prev_counter = counter;
            while (prev_counter == counter) {
                ULWOS2_THREAD_YIELD();
            }		            
		} 
    }
}

void ThreadDisplaySleep(void) 
{
    ULWOS2_THREAD_START();

    while (true) {
begin:
        printf("Go display to work!\n");
        // turn on display
        ssd1306_cmd(SSD1306_DISPLAYON);
        static uint16_t i;
        for (i = 0; i < 500; i++) {
            ULWOS2_THREAD_SLEEP_MS(60);
            if (EVENT_RESUME == true) {
                EVENT_RESUME = false;
                goto begin;
            }
        }
        // turn off display
        ssd1306_cmd(SSD1306_DISPLAYOFF);
        printf("\nGo display to sleep!");

        // waiting reset
        while (EVENT_RESUME == false) {
            ULWOS2_THREAD_YIELD();
        }
    }
}

// Init  and scheduler
int main()
{
	// 48MHz internal clock
	SystemInit();

    // To calc flash options
    flash_set_latency();
    flash_lock();

	// init i2c and oled
	Delay_Ms( 200 );	// give OLED some more time
    SetupDebugPrintf();
    while( !DebugPrintfBufferFree() );
	printf("initializing i2c oled...");

    counter = flash_read_option_byte_DATA_16();
    if (counter < 0) counter = 0; 
    printf("\ncounter %d", counter);

	ULWOS2_INIT();
    ULWOS2_THREAD_CREATE(ThreadHallSensor, 5);
    ULWOS2_THREAD_CREATE(ThreadDisplaySleep, 5);
    ULWOS2_THREAD_CREATE(ThreadDisplayCounter, 5);

    ULWOS2_START_SCHEDULER();
    return 0;
}