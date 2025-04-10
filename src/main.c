#define SSD1306_128X32
#define FUNCONF_SYSTEM_CORE_CLOCK 24000000

#include "ch32v003fun.h"
#include <stdio.h>
#include "ssd1306_i2c.h"
#include "ssd1306.h"
#include "ch32v003_flash.h"
#include "main.h"
#include "ticker.h"


// save counter to user option bytes
void maybe_save_counter(void)
{
    if (counter - flash_read_option_byte_DATA_16() > 2) 
    {
        printf("saving counter %d\n", counter);
        __disable_irq();
        // save counter to flash
        flash_unlock();
        flash_unlock_option_bytes();
        flash_write_option_byte_16_bits(counter);
        flash_lock();
        __enable_irq();
        LOGD("COUNTER", "saved!\n");  
    } else {
        LOGD("COUNTER", "not changed a lot so not saving!!\n");
    }
}

// Tasks

// Detect hall signal task
// Increments wheel counter
void Task_HallSensor(void) 
{
    task_open();

    while (true) 
    {
        while(funDigitalRead(HALL) == 1) 
            task_wait( 1 );
        
        while(funDigitalRead(HALL) == 0) 
            task_wait( 1 );
        
        counter++; 

        LOGD("HALL", "sending signal display_wakup\n");
        event_signal( evt_display_wakeup );
        LOGD("HALL", "signal display_wakup sent!\n");
        LOGD("HALL", "sending signal wheel sensor\n");
        event_signal( evt_wheel_sensor );
        LOGD("HALL", "event wheel pushed\n");
        task_wait( 2 );
    }
    task_close();
}

// SSD1306-based OLED in I2C interface mode to pins PC1 (SDA) and PC2 (SCL)
// Display task, shows `counter`
void Task_DisplayCounter(void)
{
    char buffer[8];

    ssd1306_i2c_init();
    ssd1306_init();		
    ssd1306_setbuf(0);  

    task_open();

    while (true) 
    {
        sprintf(buffer, "%06d", counter);
        buffer[6]= 0;
        ssd1306_drawstr_sz(0, 16, buffer, 1, fontsize_16x16);
        ssd1306_refresh();	
        event_wait(evt_wheel_sensor);
        LOGD("DISPL", "Event display update received!\n");
        task_wait( 200 );
    }

    task_close();
}

// Wait for a time and set display to sleep
void Task_DisplaySleep(void) 
{
    task_open();
    
    ssd1306_cmd(SSD1306_DISPLAYON);
    static bool display_on = true;

    while (true) 
    { 
        LOGD("SLEEP", "Display sleep task waiting for event or timeout!\n");           
        event_wait_timeout(evt_display_wakeup, TIMEOUT_GO_TO_SLEEP); // wait event or some timeout

        if (event_signaling_taskId_get(evt_display_wakeup) == NO_TID) // if fired timeout
        {
            // turn off display
            ssd1306_cmd(SSD1306_DISPLAYOFF);
            display_on = false;
            LOGD("SLEEP", "Timeout! Display go to sleep...\n");
            // maybe save counter
            maybe_save_counter();
            __WFE(); // SLEEP
            // waiting reset
            event_wait(evt_display_wakeup);           
        } else {
            LOGD("SLEEP", "Event evt_display_wakeup catched!\n");
        }

        if (display_on == false) 
        {
        // turn on display
            ssd1306_cmd(SSD1306_DISPLAYON);
            display_on = true;            
            LOGD("SLEEP", "Display waked up!\n");
        }
    }

    task_close();
}

// Keyboard task, detects short and long presses
void Task_Keyboard(void) 
{
    task_open();

    static uint32_t button_press_time;
    button_press_time = systick_millis;
    static bool button_pressed = false;

    while(true) 
    {
        task_wait( 10 );

        bool button_state = funDigitalRead(BUTTON); // true - pressed
        if (button_state && !button_pressed) 
        {
            button_press_time = systick_millis;
            button_pressed = true;
        }
        else if (!button_state && button_pressed) {
            // Кнопка отпущена, вычисляем длительность нажатия
            uint32_t button_release_time = systick_millis;
            uint32_t press_duration = button_release_time - button_press_time;

            if (press_duration > 100) 
            {
                // Определяем тип нажатия
                if (press_duration > 5000)
                {
                    LOGD("KEYBD", "pressed > 10 sec\n");
                    // reset counter
                    counter = 0;
                    maybe_save_counter();  
                    event_signal( evt_wheel_sensor ); // fake event to update display
                } else {
                    LOGD("KEYBD", "pressed short\n");
                }
                event_signal( evt_display_wakeup );
            }            
            button_pressed = false;
        }
    }

    task_close();
}

//---------------------------------
// Init  and scheduler
int main(void) 
{
	// 48MHz internal clock
	SystemInit();
    funGpioInitAll();
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;

    funPinMode(HALL, FUN_INPUT);
    funPinMode(BUTTON, FUN_INPUT);

    // Sleep
    // AFIO is needed for EXTI
	RCC->APB2PCENR |= RCC_AFIOEN;

	// assign pin 3,5 interrupt from portD (0b11) to EXTI channel 2
    #pragma GCC diagnostic ignored "-Wshift-count-overflow"
	AFIO->EXTICR |= (uint32_t)((0b11 << (HALL*2)) | (0b11 << (BUTTON*2)));

	// enable line2 interrupt event
	EXTI->EVENR |= EXTI_Line5 | EXTI_Line3;
	EXTI->FTENR |= EXTI_Line3; // Falling hall 1->0
    EXTI->RTENR |= EXTI_Line5; // Raising button 0->1
	// select standby on power-down
	PWR->CTLR |= PWR_CTLR_PDDS;
	// peripheral interrupt controller send to deep sleep
	PFIC->SCTLR |= (1 << 2);

    // interrupt timer init
    systick_init();

    // To calc flash options
    flash_set_latency();
    flash_lock();

	// init i2c and oled
	Delay_Ms( 200 );	// give OLED some more time
	
    LOGD("MAIN", "\ninitializing i2c oled...");

    counter = flash_read_option_byte_DATA_16();
    if (counter < 0)  counter = 0; 
    LOGD("MAIN", "\nreaded counter: %d\n", counter);

    // Initialize cocoOS 
	os_init();

    evt_wheel_sensor = event_create();
    evt_display_wakeup = event_create();

    LOGD("MAIN", "creating task hall\n");
    task_hall_sensor = task_create( Task_HallSensor,  0, 1, 0, 0, 0 );
    LOGD("MAIN", "creating task keyboard\n");  
    task_keyboard = task_create( Task_Keyboard, 0, 2, 0, 0, 0 );
    LOGD("MAIN", "creating task display sleep\n");  
    task_disp_sleep = task_create( Task_DisplaySleep, 0, 3, 0, 0, 0 );
    LOGD("MAIN", "creating task display\n");    
    task_display = task_create( Task_DisplayCounter, 0, 4, 0, 0, 0 );
    
    LOGD("MAIN", "os starting\n\n");
    os_start();
    return 0;
}