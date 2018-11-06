/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/ledc.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/spi_master.h"
#include "rom/cache.h"

//GPIO 
#define TEST_GPIO_PIN_OUTPUT	(5)
#define DELAY_1S_CNT			(1000)
//LEDC
#define LED_GPIO_PIN_OUTPUT		(2)
#define LEDC_FRQ				(5000)
#define LEDC_DUTY				(5000)
#define LEDC_FADE_TIME			(1000)   // Fade 1000mS-1s 
//PWM
#define GPIO_PWM0A_OUT 			(15)  	 // Set GPIO 15 as PWM0A
#define PWM_FEQ					(1000)   // 1KHz
//ADC1
#define DEFAULT_VREF    		(1100)   // Use adc2_vref_to_gpio() to obtain a better estimate
#define ADC1_SAMPLES_TIMES   	(64)     // Multisampling
static const adc_unit_t unit = ADC_UNIT_1;					// ADC1
static const adc_channel_t ad1_channel = ADC1_CHANNEL_6;    // ADC1 GPIO34 
static const adc_atten_t atten = ADC_ATTEN_DB_11;			// Attenuation level--11dB full-scale voltage 3.9V
static esp_adc_cal_characteristics_t *adc_chars;			// ADC print type
//Timerx
#define TIMER_DIVIDER          	(16)  								//  Hardware timer clock divider--/16
#define TIMER_SCALE           	(TIMER_BASE_CLK / TIMER_DIVIDER)  	// convert counter value to seconds--xS
#define TIMER_INTERVAL0_SEC   	(0.001) 							// 1ms IRQ
#define TEST_WITH_RELOAD      	(1)   							    // ISR will be done with auto reload
typedef struct {
    int type;  							// the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;
//SPI 
#define GPIO_MISO 				(33)
#define GPIO_MOSI 				(25)
#define GPIO_SCLK  				(26)
#define GPIO_CS   				(12)
#define SPI_TRANS_SIZES 		(16)
spi_device_handle_t spi_handle;

//-------------------------------------TEST handle------------------------------------------------------//
// TEST GPIO initial
void TEST_Gpio_initial(void)
{
	gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIOx
    io_conf.pin_bit_mask = (1ULL << TEST_GPIO_PIN_OUTPUT);
    //disable pull-down mode
    io_conf.pull_down_en = 1;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);	
	gpio_set_level(TEST_GPIO_PIN_OUTPUT,0);
}

//-------------------------------------LEDC handle------------------------------------------------------//
// LEDC PWM Acion
void LEDC_action_Flash_Task(void *arg)
{	
// -----------------------LEDC PWM initial----------------------------------------------
// Configure Timer----timer0
	ledc_timer_config_t ledc_timer = {
		.duty_resolution = LEDC_TIMER_13_BIT, 	   // resolution of PWM duty--13bits
		.freq_hz 		 = LEDC_FRQ,               // frequency of PWM signal--5kHz
		.speed_mode 	 = LEDC_HIGH_SPEED_MODE,   // timer mode--high speed
		.timer_num 		 = LEDC_TIMER_0            // timer source of channel 0~3--timer0
    };
	ledc_timer_config(&ledc_timer);
// Configure Channel 	
	ledc_channel_config_t ledc_channel = {
		.channel    = LEDC_CHANNEL_0,		  // Sel LEDC channel 0~7--LEDC channel 0
        .duty       = 0,					  // LEDC channel duty--0
        .gpio_num   = LED_GPIO_PIN_OUTPUT,	  // LEDC output gpio_num--GPIO2
        .speed_mode = LEDC_HIGH_SPEED_MODE,	  // timer mode--high speed
        .timer_sel  = LEDC_TIMER_0			  // Sel timer source of channel 0~3--timer0
    };	
	ledc_channel_config(&ledc_channel);	
// Initialize fade service.
    ledc_fade_func_install(0);

//---------------------------TASK---------------------------------------------------------	
	while(1)
	{
// LEDC fade duty up  	
	//printf("1. LEDC fade up to duty = %d\n", LEDC_DUTY);
	ledc_set_fade_with_time(ledc_channel.speed_mode,
							ledc_channel.channel, LEDC_DUTY, LEDC_FADE_TIME);
	ledc_fade_start(ledc_channel.speed_mode,
                    ledc_channel.channel, LEDC_FADE_NO_WAIT);					// LEDC fade function will return immediately---LEDC_FADE_NO_WAIT
	vTaskDelay(LEDC_FADE_TIME / portTICK_PERIOD_MS);							// 1s
	
// LEDC fade duty down 	
	//printf("2. LEDC fade down to duty = 0\n");
	ledc_set_fade_with_time(ledc_channel.speed_mode,
							ledc_channel.channel, 0, LEDC_FADE_TIME);
	ledc_fade_start(ledc_channel.speed_mode,
                    ledc_channel.channel, LEDC_FADE_NO_WAIT);					// LEDC fade function will return immediately---LEDC_FADE_NO_WAIT
	vTaskDelay(2*LEDC_FADE_TIME / portTICK_PERIOD_MS);							// 2s
/*	
// LEDC set duty witchout fade
	printf("3. LEDC set duty = %d without fade\n", LEDC_DUTY);
	ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, LEDC_DUTY);
    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
    vTaskDelay(DELAY_1S_CNT / portTICK_PERIOD_MS);								// 1s
	
// LEDC set duty 0 witchout fade
	printf("4. LEDC set duty = 0 without fade\n");
	ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
	vTaskDelay(DELAY_1S_CNT / portTICK_PERIOD_MS);								// 1s
*/
	}
}

//-------------------------------------PWM handle------------------------------------------------------//
// PWM output initial
void PWM_initial(void)
{
	mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM0A_OUT);
	
	mcpwm_config_t pwm_config;
    pwm_config.frequency = PWM_FEQ;    						//frequency = 1KHz
    pwm_config.cmpr_a = 0;       							//duty cycle of PWMxA = 0%
    //pwm_config.cmpr_b = 50.0;       						//duty cycle of PWMxb = 50.0%
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;				// MCPWM duty cycle mode--MCPWM_DUTY_MODE_0 high time
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);   //Configure PWM0A 
}

// PWM Action Task
void PWM_output_action_Task(void)
{
	PWM_initial();
	
	mcpwm_set_duty(MCPWM_UNIT_0,MCPWM_TIMER_0,MCPWM_OPR_A,50);	// duty cycle of PWMxA = 50%
	
	while(1)
	{
		vTaskDelay(DELAY_1S_CNT / portTICK_PERIOD_MS);			// 1s
	}
}

//-------------------------------------ADC handle------------------------------------------------------//
// ADC1 initial
void ADC1_initial(void)
{
	adc1_config_width(ADC_WIDTH_BIT_12);						// ADC1--12Bit
	adc1_config_channel_atten(ad1_channel,atten);					
}

//ADC1 get AD Action Task
void ADC1_get_action_Task(void)
{
	uint32_t adc_reading = 0;
	uint32_t voltage = 0;
	
	ADC1_initial();
	
//Characterize ADC --print 
    //adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    //esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
	
	while(1)
	{
		for (int i = 0; i < ADC1_SAMPLES_TIMES; i++)				//  Avg ADC1 value
		{
			adc_reading += adc1_get_raw(ad1_channel); 
		}
		adc_reading /= ADC1_SAMPLES_TIMES;
		
		voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
		printf("AD1: %d\tVoltage: %dmV\n", adc_reading, voltage);
		
        vTaskDelay(DELAY_1S_CNT / portTICK_PERIOD_MS);				// 1s
	}
}

//-------------------------------------TIME0/1 handle----------------------------------------//
// Grop timerx ISR handle 
void IRAM_ATTR timer_group0_isr(void *para)
{
    int timer_idx = (int) para;
	static uint32_t flag = 0;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {				// Timer0 
        TIMERG0.int_clr_timers.t0 = 1;
		flag = ~flag;
		gpio_set_level(TEST_GPIO_PIN_OUTPUT,flag);	
		
    } else if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_1) {		// Timer1
        //evt.type = TEST_WITH_RELOAD;
        TIMERG0.int_clr_timers.t1 = 1;
    } 

    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;
}

// Timer0 initial--Grop 0 timer0/1
static void Timerx_initial(int timer_idx, bool auto_reload, double timer_interval_sec)
{
/* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

/* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr, 
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}

//----------------------------------------------SPI handle-----------------------------------------------------
//SPI initial
void SPI_initial(void)
{
    esp_err_t ret;

//Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=GPIO_MOSI,
        .miso_io_num=GPIO_MISO,
        .sclk_io_num=GPIO_SCLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };

//Configuration for the SPI device on the other side of the bus
    spi_device_interface_config_t devcfg={
        .command_bits=0,
        .address_bits=0,
        .dummy_bits=0,
        .clock_speed_hz=5000000,
        .duty_cycle_pos=128,        //50% duty cycle
        .mode=0,
        .spics_io_num=GPIO_CS,
        .cs_ena_posttrans=3,        //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .queue_size=3
    };
	
//Initialize the SPI bus and add the device we want to send stuff to.
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    //assert(ret==ESP_OK);
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi_handle);
    //assert(ret==ESP_OK);
}

//SPI action Task
void SPI_action_Task(void)
{
	int n=0;
    char sendbuf[128] = {0};
    char recvbuf[128] = {0};
    spi_transaction_t spih;
	
	for(n=0;n<129;n++) {sendbuf[n]=1;}
    //memset(&spih, 0, sizeof(spih));
	SPI_initial();
	spih.length=sizeof(sendbuf)*8;
    spih.tx_buffer=sendbuf;
    spih.rx_buffer=recvbuf;
	
	while(1)
	{
        //Wait for slave to be ready for next byte before sending
        spi_device_transmit(spi_handle, &spih);
		
		vTaskDelay(DELAY_1S_CNT / portTICK_PERIOD_MS);				
	}
}

//-------------------------------------------Main fun handle---------------------------------------------------
void app_main()
{
	TEST_Gpio_initial();															// TEST pin initial
	Timerx_initial(TIMER_0, TEST_WITH_RELOAD, TIMER_INTERVAL0_SEC);					// Grop0 Timer0 initial
	
	xTaskCreate(LEDC_action_Flash_Task, "LEDC_action_Flash_Task", 4096, NULL, 5, NULL);
	//xTaskCreate(PWM_output_action_Task, "PWM_output_action_Task", 4096, NULL, 5, NULL);
	//xTaskCreate(ADC1_get_action_Task, "ADC1_get_action_Task", 4096, NULL, 5, NULL);
	xTaskCreate(SPI_action_Task, "SPI_action_Task", 4096, NULL, 5, NULL);
}
