/** iNEMO ADK Accessory firmware
 */

#define BOARD_IS_INEMOV2  1

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"	 
#include "semphr.h"
#include "serial.h"		  

/* Library includes. */
#include "stm32f10x_it.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_spi.h"

#include "Max3421e.h"
#include "Max3421e_constants.h"

#include "inemoutil.h"

/*
 * Configure the clocks, GPIO and other peripherals as required by the demo.
 */
static void prvSetupHardware( void );

xSemaphoreHandle xBinarySemaphore;

u8 revision1;
u8 revision2;
u8 mydata1;
u8 mydata2;


void gpiosInit(void)
{
	GPIO_InitTypeDef gpioInit;

	/* Enable iNemo LED PB9 */
	GPIO_StructInit(&gpioInit);
	gpioInit.GPIO_Pin = GPIO_Pin_9;
	gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
	gpioInit.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &gpioInit);

	/* Enable PB0 for output */
	GPIO_StructInit(&gpioInit);
	gpioInit.GPIO_Pin = GPIO_Pin_0;
	gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
	gpioInit.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &gpioInit);

	/* Enable PC7 for input */
	GPIO_StructInit(&gpioInit);
	gpioInit.GPIO_Pin = GPIO_Pin_7;
	gpioInit.GPIO_Mode = GPIO_Mode_AIN;
	gpioInit.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOC, &gpioInit);

	/* Enable PC3 for output (Slave Select) */
	GPIO_StructInit(&gpioInit);
	gpioInit.GPIO_Pin = GPIO_Pin_3;
	gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
	gpioInit.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOC, &gpioInit);
}

u8 testval;


void spiTask(void* params)
{
	/* This NEEDS to be called in the context of a task, not from main otherwise
	 * the UART will be functional only after a reset... TBI */
	inemoUtilInit();

	/* constructor */
	max3421e();

	/* Power on */
	max3421ePowerOn();


	/* Read revision: 0x90(TX), 0x12 (or 0x48) (RX) */
	revision1 = max3421eRegRd(rREVISION);
	revision2 = max3421eRegRd(rREVISION);
	mydata1 = max3421eRegRd(rPINCTL);
	mydata2 = max3421eRegRd(rPINCTL);

	max3421eRegWr(20<<3, 0xb);
	testval = max3421eRegRd(20<<3);

	while(1){	
		/* LED activity */
		ledOn();
		vTaskDelay(300);
		ledOff();
		vTaskDelay(300);
		print("bellazia ");
	}
}


 void vSetupTimerTest( void )
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	/* Enable timer clocks */
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM2, ENABLE );

	/* Initialise data. */
	TIM_DeInit( TIM2 );
	TIM_TimeBaseStructInit( &TIM_TimeBaseStructure );

	/* Time base configuration for timer 2 - which generates the interrupts. */
	TIM_TimeBaseStructure.TIM_Period = 2000;
	TIM_TimeBaseStructure.TIM_Prescaler = 4000;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit( TIM2, &TIM_TimeBaseStructure );
	TIM_ARRPreloadConfig( TIM2, ENABLE );

	
	/* Enable TIM2 IT */
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQChannel;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init( &NVIC_InitStructure );	
	TIM_ITConfig( TIM2, TIM_IT_Update, ENABLE );

	/* Finally, enable timer. */
	TIM_Cmd( TIM2, ENABLE );
}

int toggle = 0;

void vTimer2IntHandler( void )
{
	/* ACK interrupt */
    TIM_ClearITPendingBit( TIM2, TIM_IT_Update );

	/* Read back the IRQ status to avoid double IRQ race */
	/* See https://my.st.com/public/STe2ecommunities/mcu/Lists/ARM%20CortexM3%20STM32/Flat.aspx?RootFolder=%2Fpublic%2FSTe2ecommunities%2Fmcu%2FLists%2FARM%20CortexM3%20STM32%2FTimer%20update%20event%20interrupt%20retriggering%20after%20exit&FolderCTID=0x01200200770978C69A1141439FE559EB459D758000626BE2B829C32145B9EB5739142DC17E&currentviews=241 */
	/* See https://my.st.com/public/FAQ/Lists/faqlist/DispForm.aspx?ID=144&level=1&objectid=141&type=product&Source=%2fpublic%2fFAQ%2ffaq.aspx%3flevel%3d1%26objectid%3d141%26type%3dproduct */
	TIM_GetITStatus(TIM2, TIM_IT_Update);

	if(toggle++ % 2)
		GPIO_WriteBit(GPIOB, GPIO_Pin_9, Bit_SET);
	else
		GPIO_WriteBit(GPIOB, GPIO_Pin_9, Bit_RESET);
}


int main( void )
{  
 	int err;

  	prvSetupHardware();
	gpiosInit();

	//vSetupTimerTest();

  	err = xTaskCreate(spiTask, (signed portCHAR*) "SPI", 256, NULL, tskIDLE_PRIORITY + 1, NULL );
  	if(err != pdPASS)
  		panic();

	/* Start the scheduler. */
	vTaskStartScheduler();
	
	/* Will only get here if there was not enough heap space to create the
	idle task. */
	panic();

	return 0;
}

	 
static void prvSetupHardware( void )
{
	/* Start with the clocks in their expected state. */
	RCC_DeInit();

	/* Enable HSE (high speed external clock). */
	RCC_HSEConfig( RCC_HSE_ON );

	/* Wait till HSE is ready. */
	while( RCC_GetFlagStatus( RCC_FLAG_HSERDY ) == RESET )
	{
	}

	/* 2 wait states required on the flash. */
	*( ( unsigned portLONG * ) 0x40022000 ) = 0x02;

	/* HCLK = SYSCLK */
	RCC_HCLKConfig( RCC_SYSCLK_Div1 );

	/* PCLK2 = HCLK */
	RCC_PCLK2Config( RCC_HCLK_Div1 );

	/* PCLK1 = HCLK/2 */
#if BOARD_IS_INEMOV2
	RCC_PCLK1Config( RCC_HCLK_Div2 );	  //STMF103	Max 32MHz
#elif BOARD_IS_DISCOVERY
	RCC_PCLK1Config( RCC_HCLK_Div1 );	  //STMF100	Max 24MHz
#else
#error "Please define either BOARD_IS_INEMOV2 or BOARD_IS_DISCOVERY"
#endif
	
	/* PLLCLK = 8MHz * 9 = 72 MHz. */
#if BOARD_IS_INEMOV2
	RCC_PLLConfig( RCC_PLLSource_HSE_Div1, RCC_PLLMul_9 ); //STMF103 @72MHz
#elif BOARD_IS_DISCOVERY
	RCC_PLLConfig( RCC_PLLSource_HSE_Div1, RCC_PLLMul_3 );	 //STMF100 @24MHz
#else
#error "Please define either BOARD_IS_INEMOV2 or BOARD_IS_DISCOVERY"
#endif

	/* Enable PLL. */
	RCC_PLLCmd( ENABLE );

	/* Wait till PLL is ready. */
	while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
	{
	}

	/* Select PLL as system clock source. */
	RCC_SYSCLKConfig( RCC_SYSCLKSource_PLLCLK );

	/* Wait till PLL is used as system clock source. */
	while( RCC_GetSYSCLKSource() != 0x08 )
	{
	}

	/* Enable GPIOA, GPIOB, GPIOC, GPIOD, GPIOE and AFIO clocks */
	RCC_APB2PeriphClockCmd(	RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |RCC_APB2Periph_GPIOC
							| RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO
							| RCC_APB2Periph_ALL , ENABLE );

	/* SPI2 Periph clock enable */
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_SPI2 | RCC_APB1Periph_ALL, ENABLE );


	/* Set the Vector Table base address at 0x08000000 */
	NVIC_SetVectorTable( NVIC_VectTab_FLASH, 0x0 );

	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
	
	/* Configure HCLK clock as SysTick clock source. */
	SysTick_CLKSourceConfig( SysTick_CLKSource_HCLK );

	/* Enable prefetch */
	*((unsigned long*)(0x40022000)) = 0x12;
}

