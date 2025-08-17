/*                   Documentation section
 *
 * Project name : RTOS Communicating tasks
 * Version : FreeRTOS
 * Authors : Manar Kamal & Sandy Naguib
 * Date : 22/6/2024
 * Course Code : ELC_2080
 */

/*-------------Header files and Macros--------------*/
#include <stdio.h>
#include <stdlib.h>
#include "diag/trace.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#include <time.h>

#define CCM_RAM __attribute__((section(".ccmram"))) //A macro for placing variables in the Cortex-M core-coupled memory
#define totalmessages 1000 //Testing by 10 messages at first
#define Queue_Size 3  //To be changed easily to 10
/*-----------------------------------------------------------*/

/*----------Function Prototypes and global variables------*/
static void SenderTimerCallback( TimerHandle_t xTimer );
static void ReceiverTimerCallback( TimerHandle_t xTimer );

/*-----------------------------------------------------------*/

/* The LED software timer.  This uses vLEDTimerCallback() as its callback
 * function.
 */
BaseType_t xTimer1Started, xTimer2Started;

/*-----------------------------------------------------------*/

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

QueueHandle_t testQueue; //globalQueue
TimerHandle_t senderTimers[3];
TimerHandle_t receiverTimer;
SemaphoreHandle_t senderSemaphores[3]; //Dedicated semaphore for each sender as required
SemaphoreHandle_t receiverSemaphore; //One semaphore for receiver

int sentMessages[3]; //array as counters for sent messages by each task
int blockedMessages[3]; //array as counters for blocked messages by each task
int receivedMessages;

int lowerBounds[] = {50, 80, 110, 140, 170, 200}; //Bounds from where we will generate random periods
int upperBounds[] = {150, 200, 250, 300, 350, 400};
int boundNum = 0; //To determine which bound we use , 0 means we use 50 and 150

int averageTSenders[3]; //Array to track the average send period for each sender task

// Helper function to generate random periods
TickType_t getRandomPeriod(TickType_t lower, TickType_t upper) {
	return (rand() % (upper - lower + 1)) + lower;
}
/*------------Reset Function-------------*/
/*Used to clear the queue and clear counters*/
void ResetSystem2(void)
{
	// Reset counters
	for (int i = 0; i < 3; i++) {
		sentMessages[i] = 0;
		blockedMessages[i] = 0;
		averageTSenders[i] = 0;
	}
	receivedMessages = 0;

	// Clear queue
	xQueueReset(testQueue);
}
/*---------Second Reset function----------*/
/*-----Print current statistics and check if all iterations are done---*/
void ResetSystem1(void) {

	printf("Total sent messages: %d\n", sentMessages[0] + sentMessages[1] + sentMessages[2]);
	printf("Total blocked messages: %d\n", blockedMessages[0] + blockedMessages[1] + blockedMessages[2]);
	for (int i = 0; i < 3; i++)
	{
		printf("Sender %d - Sent: %d, Blocked: %d - Average TSender = %d\n", i, sentMessages[i], blockedMessages[i], averageTSenders[i]/(sentMessages[i] + blockedMessages[i]));
	}
	printf("Receiver - Received: %d\n", receivedMessages);

	boundNum++;
	if (boundNum == 6)
	{
		printf("Game Over\n");
		for (int i = 0; i < 3; i++) {
			xTimerDelete(senderTimers[i], 0);
			vSemaphoreDelete(senderSemaphores[i]);
		}
		xTimerDelete(receiverTimer, 0);
		vQueueDelete(testQueue);
		vSemaphoreDelete(receiverSemaphore);
		vTaskEndScheduler();
		exit(0);
	} else {
		ResetSystem2();
	}
}
/*----------Sender Task------------*/
void SenderTask( void *parameters ) //Function accept any type
{
	int taskNum = (int)parameters;
	char message[20];
	BaseType_t xStatus;
	while(1)
	{
		int period = getRandomPeriod (lowerBounds[boundNum] , upperBounds[boundNum]) ;  //Random period
		xTimerChangePeriod(senderTimers[taskNum], pdMS_TO_TICKS(period), 0);
		xTimerStart(senderTimers[taskNum], 0);
		xSemaphoreTake(senderSemaphores[taskNum], portMAX_DELAY);
		averageTSenders[taskNum] += period;

		snprintf(message, sizeof(message), "Time is %lu", xTaskGetTickCount());
		xStatus = xQueueSend( testQueue, &message, 0 );
		if ( xStatus == pdPASS )
		{
			sentMessages[taskNum]++;
		} else {
			blockedMessages[taskNum] = blockedMessages[taskNum] + 1;
		}
	}
}

static void ReceiverTask( void *parameters )
{
	char message[20];
	BaseType_t status;
	const TickType_t ticksToWait = pdMS_TO_TICKS( 100 );
	while(1)
	{
		xTimerStart(receiverTimer, 0);
		xSemaphoreTake(receiverSemaphore, portMAX_DELAY);
		status = xQueueReceive( testQueue, &message, ticksToWait );
		/* tickToWait the maximum amount of time that the task will
		 * remain in the Blocked state to wait for data to be available */
		if( status == pdPASS )
		{
			receivedMessages++;
			printf("%s\n", message);
			if (receivedMessages == totalmessages)
			{
				ResetSystem1();
			}
		} else {
			if (testQueue == 0)
				printf( "Queue is empty.\r\n" );
			else
				printf( "Could not receive from the queue.\r\n" );
		}
	}
}

int main(int argc, char* argv[])
{
	srand(time(NULL));
	testQueue = xQueueCreate( Queue_Size , sizeof( char[20] ) ); //Queue creation
	if( testQueue != NULL )
	{
		for (int i = 0; i < 3; i++)
		{
			senderSemaphores[i] = xSemaphoreCreateBinary(); //Sender Semaphore initialization
			xSemaphoreGive(senderSemaphores[i]);
			int period = getRandomPeriod (lowerBounds[boundNum] , upperBounds[boundNum]);
			senderTimers[i] = xTimerCreate("SenderTimer", pdMS_TO_TICKS(period), pdFALSE, (void *) i, SenderTimerCallback);
			//The period is just for creating the timer , not related to the timer's total average period. It will be over written in the sender function
			xTaskCreate(SenderTask, "SenderTask", 1000, (void *)i, (i == 2) ? 2 : 1, NULL);
		}

		receiverSemaphore = xSemaphoreCreateBinary();
		xSemaphoreGive(receiverSemaphore);
		receiverTimer = xTimerCreate("ReceiverTimer", 100, pdFALSE, NULL, ReceiverTimerCallback);
		xTaskCreate(ReceiverTask, "ReceiverTask", 1000, NULL, 3, NULL);

		//Just a check for the timer of the senders and receivers
		if(( senderTimers[0] != NULL ) && ( senderTimers[1] != NULL ) && ( senderTimers[2] != NULL ) && ( senderTimers[3] != NULL ) && ( receiverTimer != NULL ))
		{
			ResetSystem2();
			vTaskStartScheduler();
		}
		else printf("Couldn't create the timers.");

	}
	else
	{
		printf( "Could not create the queue.\r\n" );
	}

	return 0;
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------

static void SenderTimerCallback( TimerHandle_t xTimer )
{
	int taskId = (int)pvTimerGetTimerID(xTimer);
	xSemaphoreGive(senderSemaphores[taskId]);
}

static void ReceiverTimerCallback( TimerHandle_t xTimer )
{
	xSemaphoreGive(receiverSemaphore);
}


void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amout of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

void vApplicationTickHook(void) {
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
  /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
  state will be stored. */
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

  /* Pass out the array that will be used as the Idle task's stack. */
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;

  /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
  Note that, as the array is necessarily of type StackType_t,
  configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
