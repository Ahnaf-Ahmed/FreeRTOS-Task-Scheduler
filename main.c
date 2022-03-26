/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wwrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*
FreeRTOS is a market leading RTOS from Real Time Engineers Ltd. that supports
31 architectures and receives 77500 downloads a year. It is professionally
developed, strictly quality controlled, robust, supported, and free to use in
commercial products without any requirement to expose your proprietary source
code.

This simple FreeRTOS demo does not make use of any IO ports, so will execute on
any Cortex-M3 of Cortex-M4 hardware.  Look for TODO markers in the code for
locations that may require tailoring to, for example, include a manufacturer
specific header file.

This is a starter project, so only a subset of the RTOS features are
demonstrated.  Ample source comments are provided, along with web links to
relevant pages on the http://www.FreeRTOS.org site.

Here is a description of the project's functionality:

The main() Function:
main() creates the tasks and software timers described in this section, before
starting the scheduler.

The Queue Send Task:
The queue send task is implemented by the prvQueueSendTask() function.
The task uses the FreeRTOS vTaskDelayUntil() and xQueueSend() API functions to
periodically send the number 100 on a queue.  The period is set to 200ms.  See
the comments in the function for more details.
http://www.freertos.org/vtaskdelayuntil.html
http://www.freertos.org/a00117.html

The Queue Receive Task:
The queue receive task is implemented by the prvQueueReceiveTask() function.
The task uses the FreeRTOS xQueueReceive() API function to receive values from
a queue.  The values received are those sent by the queue send task.  The queue
receive task increments the ulCountOfItemsReceivedOnQueue variable each time it
receives the value 100.  Therefore, as values are sent to the queue every 200ms,
the value of ulCountOfItemsReceivedOnQueue will increase by 5 every second.
http://www.freertos.org/a00118.html

An example software timer:
A software timer is created with an auto reloading period of 1000ms.  The
timer's callback function increments the ulCountOfTimerCallbackExecutions
variable each time it is called.  Therefore the value of
ulCountOfTimerCallbackExecutions will count seconds.
http://www.freertos.org/RTOS-software-timer.html

The FreeRTOS RTOS tick hook (or callback) function:
The tick hook function executes in the context of the FreeRTOS tick interrupt.
The function 'gives' a semaphore every 500th time it executes.  The semaphore
is used to synchronise with the event semaphore task, which is described next.

The event semaphore task:
The event semaphore task uses the FreeRTOS xSemaphoreTake() API function to
wait for the semaphore that is given by the RTOS tick hook function.  The task
increments the ulCountOfReceivedSemaphores variable each time the semaphore is
received.  As the semaphore is given every 500ms (assuming a tick frequency of
1KHz), the value of ulCountOfReceivedSemaphores will increase by 2 each second.

The idle hook (or callback) function:
The idle hook function queries the amount of free FreeRTOS heap space available.
See vApplicationIdleHook().

The malloc failed and stack overflow hook (or callback) functions:
These two hook functions are provided as examples, but do not contain any
functionality.
*/

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include "stm32f4_discovery.h"
#include <time.h>
#include <stdlib.h>
/* Kernel includes. */
#include "stm32f4xx.h"
#include "../FreeRTOS_Source/include/FreeRTOS.h"
#include "../FreeRTOS_Source/include/queue.h"
#include "../FreeRTOS_Source/include/semphr.h"
#include "../FreeRTOS_Source/include/task.h"
#include "../FreeRTOS_Source/include/timers.h"



/*-----------------------------------------------------------*/
#define mainQUEUE_LENGTH 7

#define idle_task_priority	3
#define default_priority 5
#define running_priority 2

//test plans
#define task_1_Period 95
#define task_2_Period 150
#define task_3_Period 250

#define task_1_Execution 500
#define task_2_Execution 500
#define task_3_Execution 750



/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */
static void First_Task( void *pvParameters );
static void Second_Task( void *pvParameters );
static void Third_Task( void *pvParameters );
static void Idle_Task( void *pvParameters );

xQueueHandle xQueue_released = 0;
xQueueHandle xQueue_completed = 0;

TaskHandle_t xTaskIdleHandle;
TaskHandle_t xTask1Handle;
TaskHandle_t xTask2Handle;
TaskHandle_t xTask3Handle;

enum task_type {PERIODIC, APERIODIC};

typedef struct dd_task {
	TaskHandle_t t_handle;
	task_type type;
	uint32_t task_id;
	uint32_t release_time;
	uint32_t absolute_deadline;
	uint32_t completion_time;
};

typedef struct dd_task_list {//need task list for active, completed, and overdue tasks
	dd_task task;
	dd_task_list *next_task;
};

/**
 * This function receives all of the information necessary to create a new dd_task struct (excluding
 * the release time and completion time). The struct is packaged as a message and sent to a queue
 * for the DDS to receive
 */
void create_dd_task(TaskHandle_t t_handle, task_type type, uint32_t task_id, uint32_t absolute_deadline){
	dd_task d = {
			.t_handle = t_handle,
			.type = type,
			.task_id = task_id,
			.absolute_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(absolute_deadline)
	};
	xQueueSend(xQueue_released, d, pdMD_TO_TICKS(500));

}

void delete_dd_task (uint32_t task_id) {
	xQueueSend(xQueue_completed, task_id, pdMD_TO_TICKS(500));
}

dd_task_list** get_active_dd_task_list(void){

};

dd_task_list** get_complete_dd_task_list(void){

};

dd_task_list** get_overdue_dd_task_list(void){

};

void task_generator(){

}

int main(void)
{
	xTimerHandle xTimerOne;
	xTimerHandle xTimerTwo;
	xTimerHandle xTimerThree;

	// global stuff


	srand(time(NULL));

	/* Create queues used by DD scheduler */
	xQueue_released = xQueueCreate(mainQUEUE_LENGTH, sizeof( dd_task ));
	xQueue_completed = xQueueCreate(mainQUEUE_LENGTH, sizeof( dd_task ));

	/* Add to the registry, for the benefit of kernel aware debugging. */
	vQueueAddToRegistry(xQueue_released, "tasks that need to be scheduled");
	vQueueAddToRegistry(xQueue_completed, "completed tasks");

	//Task generators
	xTimerOne = xTimerCreate("Create Task One", pdMS_TO_TICKS(task_1_Period), pdTRUE, 0, Create_Task_One);
	xTimerTwo = xTimerCreate("Create Task Two", pdMS_TO_TICKS(task_2_Period), pdTRUE, 0, Create_Task_Two);
	xTimerThree = xTimerCreate("Create Task Three", pdMS_TO_TICKS(task_3_Period), pdTRUE, 0, Create_Task_Three);

	//Task scheduler
	xTaskCreate( Scheduling_Task, "DD Task Scheduler", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

	//Default tasks
	xTaskCreate( Idle_Task, "Default Task", configMINIMAL_STACK_SIZE, NULL, idle_task_priority, xTaskIdleHandle);
	xTaskCreate( First_Task, "Task One", configMINIMAL_STACK_SIZE, NULL, default_priority, xTask1Handle);
	xTaskCreate( Second_Task, "Task Two", configMINIMAL_STACK_SIZE, NULL, default_priority, xTask2Handle);
	xTaskCreate( Third_Task, "Task Three", configMINIMAL_STACK_SIZE, NULL, default_priority, xTask3Handle);


//	/* Start the tasks and timer running. */
	if(xTimerStart(xTimerOne, 0) == pdFALSE){
		printf("Timer failed to start because queue already full. \n");
		return -1;
	}

	if(xTimerStart(xTimerTwo, 0) == pdFALSE){
		printf("Timer failed to start because queue already full. \n");
		return -1;
	}

	if(xTimerStart(xTimerThree, 0) == pdFALSE){
		printf("Timer failed to start because queue already full. \n");
		return -1;
	}
	vTaskStartScheduler();

	return 0;
}


/*-----------------------------------------------------------*/

/*	@brief
 *
 * 	@note
 *
 * 	@retval	None
 *
 * */

static void Idle_Task( void *pvParameters ){
	while(1){
	}
}

static void Scheduling_Task( void *pvParameters ){
	dd_task_list active_list;
	dd_task_list completed_list;
	dd_task_list overdue_list;

	while(1){

		//release task
		dd_task released_task;
		if(xQueueRecieve(xQueue_released, released_task, pdMS_TO_TICKS(500))){
			dd_task_list add_to_active_list = {released_task, NULL};

			//if no tasks set to released task
			if(active_list == NULL){
				active_list = add_to_active_list;
			}else{	// traverse list and check deadlines

				dd_task_list current_task = active_list;

				//insert new task at head if it's earlier
				if (released_task.absolute_deadline < current_task.task.absolute_deadline) {
					
					vTaskPrioritySet(current_task.task.t_handle, default_priority);
					vTaskPrioritySet(released_task.t_handle, running_priority);

					add_to_active_list.next_task = current_task;
					active_list = add_to_active_list;
				}else{
					while(1){
						dd_task_list next_task = current_task.next_task;

						// insert new task after current task if earlier than next task
						if(released_task.absolute_deadline < next_task.task.absolute_deadline ){
							add_to_active_list.next_task = next_task;
							current_task.next_task = add_to_active_list;
							break;
						}

						// check if reached end of list
						if(next_task == NULL){
							current_task.next_task = add_to_active_list;
							break;
						}
						current_task = current_task.next_task;
					}//end traversal
				}
			}
		}//released tasks

		//completed tasks
		uint32_t completed_task_id;
		if(xQueueRecieve(xQueue_completed, completed_task_id, pdMS_TO_TICKS(500))){
			dd_task_list current_task = active_list;
			
			if(active_list == NULL){
				//ERROR STATE BAD
				break;
			}

			dd_task_list prev;

			while(1){
				
				//loop through task list until completed found
				if (current_task.task.task_id == completed_task_id){
					current_task.task.completion_time = xTaskGetTickCount();

					//should be first one
					if(prev == NULL){
						active_list = active_list.next_task;
					}else{
						prev.next_task = current_task.next_task
					}

					if(completed_list == NULL){
						completed_list = current_task;
					}else{
						dd_task_list last_task = completed_list;

						while (last_task.next_task != NULL){
							last_task = last_task.next_task;
						}

						last_task.next_task = current_task;
					}
					break;
				}

				prev = current_task;
				current_task = current_task.next_task;
			}
		}//completed tasks

		//overdue checking
		dd_task_list current_task = active_list;
		uint32_t time = xTaskGetTickCount();

		dd_task_list task_to_overdue = NULL;

		if (active_list.task.absolute_deadline < time){
			task_to_overdue = {active_list.task, NULL}
			active_list = active_list.next_task;
		}else{
			while(1) {
				dd_task_list checking = current_task.next_task;
				if(checking.task.absolute_deadline < time){
					task_to_overdue = {checking.task, NULL}
					current_task.next_task = checking.next_task;
					break;
				}
			}
		}

		if(overdue_list == NULL){
			overdue_list = task_to_overdue;
		}else{
			current_task = overdue_list;
			while(1) {
				if (current_task.next_task == NULL){
					current_task.next_task = task_to_overdue;
					break;
				}
				current_task = current_task.next_task;
			}
		}

		//overdue checking
		
	}
}

/*-----------------------------------------------------------*/

/*	@brief Timers to release the user defined tasks periodically
 *
 * 	@note
 *
 * 	@retval	None
 *
 * */
static void Create_Task_One( TimerHandle_t xTimer )
{
	//Call create dd task
	create_dd_task(xTask1Handle, task_type.PERIODIC, 1, task_1_Period);
}

static void Create_Task_Two( TimerHandle_t xTimer )
{
	//Call create dd task
	create_dd_task(xTask2Handle, task_type.PERIODIC, 2, task_2_Period);
}

static void Create_Task_Three( TimerHandle_t xTimer )
{
	//Call create dd task
	create_dd_task(xTask3Handle, task_type.PERIODIC, 3, task_3_Period);
}

/*-----------------------------------------------------------*/

/*	@brief	User defined tasks
 *
 * 	@note	Wrapped as a dd task
 *
 * 	@retval	None
 *
 * */

static void First_Task( void *pvParameters )
{
	while(1) {
		// TODO task might get preempted so we need to save where the execution time left off of
		int executionTime = pdMS_TO_TICKS(task_1_Execution);
		int start = xTaskGetTickCount();
		while(xTaskGetTickCount() - start != executionTime){}
		//complete task
		delete_dd_task(1);
	}
}

static void Second_Task( void *pvParameters )
{
	while(1) {
		int executionTime = pdMS_TO_TICKS(task_2_Execution);
		int start = xTaskGetTickCount();
		while(xTaskGetTickCount() - start != executionTime){}
		//call complete_task
		delete_dd_task(2);
	}
}

static void Third_Task( void *pvParameters )
{
	while(1) {
		int executionTime = pdMS_TO_TICKS(task_3_Execution);
		int start = xTaskGetTickCount();
		while(xTaskGetTickCount() - start != executionTime){}
		//call complete_task
		delete_dd_task(3);
	}
}


/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* The malloc failed hook is enabled by setting
	configUSE_MALLOC_FAILED_HOOK to 1 in FreeRTOSConfig.h.

	Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software 
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.  pxCurrentTCB can be
	inspected in the debugger if the task name passed into this function is
	corrupt. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* The idle task hook is enabled by setting configUSE_IDLE_HOOK to 1 in
	FreeRTOSConfig.h.

	This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amount of FreeRTOS heap that
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
/*-----------------------------------------------------------*/

static void prvSetupHardware( void )
{
	/* Ensure all priority bits are assigned as preemption priority bits.
	http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	NVIC_SetPriorityGrouping( 0 );

	/* TODO: Setup the clocks, etc. here, if they were not configured before
	main() was called. */
}

