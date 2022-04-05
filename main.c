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
#define mainQUEUE_LENGTH 20
#define preemptQUEUE_LENGTH 1

#define idle_task_priority	2
#define default_priority 1
#define running_priority 3
#define scheduler_priority 4

//test plans
#define task_1_Execution 95
#define task_2_Execution 150
#define task_3_Execution 250

#define task_1_Period 250
#define task_2_Period 500
#define task_3_Period 750



/*
 * Task declarations
 */
static void First_Task(void *pvParameters);
static void Second_Task(void *pvParameters);
static void Third_Task(void *pvParameters);
static void Idle_Task(void *pvParameters);
static void Scheduling_Task(void *pvParameters);
static void Monitor_Task(void *pvParameters);

/*
 * Timer Callback declarations
 */
static void Create_Task_One(TimerHandle_t xTimer);
static void Create_Task_Two(TimerHandle_t xTimer);
static void Create_Task_Three(TimerHandle_t xTimer);
static void Adjust_monitor_prio(TimerHandle_t xTimer);

xQueueHandle xQueue_released = 0;
xQueueHandle xQueue_completed = 0;
xQueueHandle xQueue_command = 0;
xQueueHandle xQueue_task_lists = 0;
xQueueHandle xQueue_preempt_1 = 0;
xQueueHandle xQueue_preempt_2 = 0;
xQueueHandle xQueue_preempt_3 = 0;

TaskHandle_t xTaskIdleHandle;
TaskHandle_t xTask1Handle;
TaskHandle_t xTask2Handle;
TaskHandle_t xTask3Handle;
TaskHandle_t xTaskMonitor;

//dd_task typedef could be combined with struct but task_list includes itself so needs to have a separate typedef
typedef struct dd_task_list dd_task_list;
typedef struct dd_task dd_task;
typedef enum task_type task_type;
typedef enum msg_type msg_type;

enum task_type { PERIODIC, APERIODIC };
enum msg_type { RELEASED, COMPLETED, MONITOR };

struct dd_task {
	TaskHandle_t t_handle;
	task_type type;
	uint32_t task_id;
	uint32_t release_time;
	uint32_t absolute_deadline;
	uint32_t completion_time;
};

struct dd_task_list {//need task list for active, completed, and overdue tasks
	dd_task task;
	dd_task_list *next_task;
};

/**
 * This function receives all of the information necessary to create a new dd_task struct (excluding
 * the release time and completion time). The struct is packaged as a message and sent to a queue
 * for the DDS to receive
 */
void create_dd_task(TaskHandle_t t_handle, task_type type, uint32_t task_id, uint32_t absolute_deadline) {
	msg_type cmd = RELEASED;
	dd_task d = {
			.t_handle = t_handle,
			.type = type,
			.task_id = task_id,
			.absolute_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(absolute_deadline)
	};
	xQueueSend(xQueue_released, &d, pdMS_TO_TICKS(500));
	xQueueSend(xQueue_command, &cmd, pdMS_TO_TICKS(500));
}

void delete_dd_task(uint32_t task_id) {
	msg_type cmd = COMPLETED;
	xQueueSend(xQueue_completed, &task_id, pdMS_TO_TICKS(500));
	xQueueSend(xQueue_command, &cmd, pdMS_TO_TICKS(500));
}

//TODO: am I passing the active_list into the xQueueReceive correctly??
//		check for complete and overdue methods as well
void get_active_dd_task_list(dd_task_list ** active_list) {
	xQueueReceive(xQueue_task_lists, active_list, pdMS_TO_TICKS(0));
}

void get_complete_dd_task_list(dd_task_list ** completed_list) {
	xQueueReceive(xQueue_task_lists, completed_list, pdMS_TO_TICKS(0));
}

void get_overdue_dd_task_list(dd_task_list ** overdue_list) {
	xQueueReceive(xQueue_task_lists, overdue_list, pdMS_TO_TICKS(0));
}

int main(void)
{
	xTimerHandle xTimerOne;
	xTimerHandle xTimerTwo;
	xTimerHandle xTimerThree;
	xTimerHandle xTimerMonitor;


	srand(time(NULL));

	/* Create queues used by DD scheduler */
	xQueue_released = xQueueCreate(mainQUEUE_LENGTH, sizeof(dd_task));
	xQueue_completed = xQueueCreate(mainQUEUE_LENGTH, sizeof(uint32_t));
	xQueue_command = xQueueCreate(mainQUEUE_LENGTH, sizeof(msg_type));
	xQueue_task_lists = xQueueCreate(mainQUEUE_LENGTH, sizeof(dd_task_list *));
	xQueue_preempt_1 = xQueueCreate(preemptQUEUE_LENGTH, sizeof(pdMS_TO_TICKS(1)));
	xQueue_preempt_2 = xQueueCreate(preemptQUEUE_LENGTH, sizeof(pdMS_TO_TICKS(1)));
	xQueue_preempt_3 = xQueueCreate(preemptQUEUE_LENGTH, sizeof(pdMS_TO_TICKS(1)));
	/* Add to the registry, for the benefit of kernel aware debugging. */
	vQueueAddToRegistry(xQueue_released, "released queue");
	vQueueAddToRegistry(xQueue_completed, "completed tasks");
	vQueueAddToRegistry(xQueue_command, "command messages");
	vQueueAddToRegistry(xQueue_task_lists, "task list structs");
	vQueueAddToRegistry(xQueue_preempt_1, "Preempt Q 1");
	vQueueAddToRegistry(xQueue_preempt_2, "Preempt Q 2");
	vQueueAddToRegistry(xQueue_preempt_3, "Preempt Q 3");

	//Task generators
	xTimerOne = xTimerCreate("Create Task One", 10, pdTRUE, 0, Create_Task_One);
	xTimerTwo = xTimerCreate("Create Task Two", 10, pdTRUE, 0, Create_Task_Two);
	xTimerThree = xTimerCreate("Create Task Three", 10, pdTRUE, 0, Create_Task_Three);
	xTimerMonitor = xTimerCreate("Monitor task", 1550, pdTRUE, 0, Adjust_monitor_prio);


	//Task scheduler: uses vTaskDelay to block the task to allow dd tasks to run
	xTaskCreate(Scheduling_Task, "Scheduler DD Task", configMINIMAL_STACK_SIZE, NULL, scheduler_priority, NULL);

	//Task monitor: this has the same priority as scheduler and uses vTaskDelay to block the task to allow dd tasks to run
	xTaskCreate(Monitor_Task, "Monitor DD Task ", configMINIMAL_STACK_SIZE, NULL, default_priority, &xTaskMonitor);

	//Default tasks
	xTaskCreate(Idle_Task, "Idle Task", configMINIMAL_STACK_SIZE, NULL, idle_task_priority, &xTaskIdleHandle);
	xTaskCreate(First_Task, "Task One", configMINIMAL_STACK_SIZE, NULL, default_priority, &xTask1Handle);
	xTaskCreate(Second_Task, "Task Two", configMINIMAL_STACK_SIZE, NULL, default_priority, &xTask2Handle);
	xTaskCreate(Third_Task, "Task Three", configMINIMAL_STACK_SIZE, NULL, default_priority, &xTask3Handle);



	//	/* Start the tasks and timer running. */
	if (xTimerStart(xTimerOne, 0) == pdFALSE) {
		printf("Timer failed to start because queue already full. \n");
		return -1;
	}

	if (xTimerStart(xTimerTwo, 0) == pdFALSE) {
		printf("Timer failed to start because queue already full. \n");
		return -1;
	}

	if (xTimerStart(xTimerThree, 0) == pdFALSE) {
		printf("Timer failed to start because queue already full. \n");
		return -1;
	}

	if (xTimerStart(xTimerMonitor, 0) == pdFALSE) {
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

static void Idle_Task(void *pvParameters) {
	while (1) {
	}
}

static void Monitor_Task(void *pvParameters) {
	int iter = 0;
	int num_completed = 0;
	int num_overdue = 0;
	msg_type cmd = MONITOR;
	while (1) {

		uint32_t time = xTaskGetTickCount();
		uint32_t preempt = 0;
		if(xQueueReceive(xQueue_preempt_1, &preempt, 0) == pdFALSE){
			xQueueSend(xQueue_preempt_1, &time, 0);
		}else{
			xQueueSend(xQueue_preempt_1, &preempt, 0);
		}

		if(xQueueReceive(xQueue_preempt_2, &preempt, 0) == pdFALSE){
			xQueueSend(xQueue_preempt_2, &time, 0);
		}else{
			xQueueSend(xQueue_preempt_2, &preempt, 0);
		}

		if(xQueueReceive(xQueue_preempt_3, &preempt, 0) == pdFALSE){
			xQueueSend(xQueue_preempt_3, &time, 0);
		}else{
			xQueueSend(xQueue_preempt_3, &preempt, 0);
		}


		dd_task_list * active_list = NULL;
		printf("\nPrint active dd tasks: \n");
		get_active_dd_task_list(&active_list);
		if (active_list != NULL) {

			dd_task_list * current_node;
			current_node = active_list;
			while (current_node != NULL) {
				dd_task curr_task = current_node->task;
				printf("%d{r: %d, d: %d, c: %d}\n", curr_task.task_id, curr_task.release_time, curr_task.absolute_deadline, curr_task.completion_time);
				current_node = current_node->next_task;
			}

		}else{
			// ERROR
			printf("xQueue_task_list empty\n");
		}


		printf("\nPrint completed dd tasks: \n");
		dd_task_list * completed_list = NULL;
		get_complete_dd_task_list(&completed_list);
		if (completed_list != NULL) {

			dd_task_list * current_node;
			current_node = completed_list;
			while (current_node != NULL) {
				num_completed++;
				dd_task curr_task = current_node->task;
				printf("%d{r: %d, d: %d, c: %d}\n", curr_task.task_id, curr_task.release_time, curr_task.absolute_deadline, curr_task.completion_time);
				current_node = current_node->next_task;
			}

		}else{
			//ERROR
			printf("xQueue_task_list empty\n");
		}

		printf("\nPrint overdue dd tasks: \n");
		dd_task_list * overdue_list = NULL;
		get_overdue_dd_task_list(&overdue_list);
		if (overdue_list != NULL) {

			dd_task_list * current_node;
			current_node = overdue_list;
			while (current_node != NULL) {
				num_overdue++;
				dd_task curr_task = current_node->task;
				printf("%d{r: %d, d: %d, c: %d}\n", curr_task.task_id, curr_task.release_time, curr_task.absolute_deadline, curr_task.completion_time);
				current_node = current_node->next_task;
			}

		}else{
			//ERROR
			printf("xQueue_task_list empty\n");
		}

		printf("Iteration %d \n# Completed %d \n# Overdue %d", ++iter, num_completed, num_overdue);
		num_overdue = 0;
		num_completed = 0;

		vTaskPrioritySet(xTaskMonitor, default_priority);
	}
}

static void Scheduling_Task(void *pvParameters) {
	dd_task_list * active_list = NULL;
	dd_task_list * completed_list = NULL;
	dd_task_list * overdue_list = NULL;

	while (1) {
		msg_type cmd_message;
		//if no command message received we don't need to do anything
		if (xQueueReceive(xQueue_command, &cmd_message, pdMS_TO_TICKS(100)) == pdFALSE) {
			continue;
		}



		uint32_t p_time = xTaskGetTickCount();
		uint32_t preempt = 0;
		if(xQueueReceive(xQueue_preempt_1, &preempt, 0) == pdFALSE){
			xQueueSend(xQueue_preempt_1, &p_time, 0);
		}else{
			xQueueSend(xQueue_preempt_1, &preempt, 0);
		}

		if(xQueueReceive(xQueue_preempt_2, &preempt, 0) == pdFALSE){
			xQueueSend(xQueue_preempt_2, &p_time, 0);
		}else{
			xQueueSend(xQueue_preempt_2, &preempt, 0);
		}

		if(xQueueReceive(xQueue_preempt_3, &preempt, 0) == pdFALSE){
			xQueueSend(xQueue_preempt_3, &p_time, 0);
		}else{
			xQueueSend(xQueue_preempt_3, &preempt, 0);
		}

		uint32_t time = xTaskGetTickCount();

		switch (cmd_message) {
			case RELEASED:
			{
				//release task
				dd_task released_task;
				if (xQueueReceive(xQueue_released, &released_task, 0)) {
					// Clear preempt queue for released task
					if(released_task.task_id == 1){
						xQueueReceive(xQueue_preempt_1, &preempt, 0);
					}else if(released_task.task_id == 2){
						xQueueReceive(xQueue_preempt_2, &preempt, 0);
					}else if(released_task.task_id == 3){
						xQueueReceive(xQueue_preempt_3, &preempt, 0);
					}

					//if no tasks set to released task
					if (active_list == NULL) {

						active_list = (dd_task_list *) malloc(sizeof(dd_task_list ));
						active_list->task = released_task;
						active_list->next_task = NULL;
						if(active_list->task.release_time == 0){
							active_list->task.release_time = time;
						}
						vTaskPrioritySet(active_list->task.t_handle, running_priority);

					}
					else {	// traverse list and check deadlines

					   // get the address of the start of the linked list
						dd_task_list * current_task = active_list;

						//insert new task at head if it's earlier
						if (released_task.absolute_deadline < current_task->task.absolute_deadline) {

							dd_task_list * new_head = (dd_task_list *) malloc(sizeof(dd_task_list ));
							new_head->task = released_task;
							new_head->next_task = active_list;
							active_list = new_head;

							active_list->task.release_time = time;
							vTaskPrioritySet(active_list->task.t_handle, running_priority);
							vTaskPrioritySet(current_task->task.t_handle, default_priority);
						}
						else {
							while (1) {
								//dd_task_list * next_task = current_task->next_task;

								// check if reached end of list
								if (current_task->next_task == NULL) {
									dd_task_list * new_node = (dd_task_list *) malloc(sizeof(dd_task_list ));

									new_node->task = released_task;
									new_node->next_task = NULL;
									current_task->next_task = new_node;
									break;
								}

								// insert new task after current task if earlier than next task
								if (released_task.absolute_deadline < current_task->next_task->task.absolute_deadline) {
									dd_task_list * elem = (dd_task_list *) malloc(sizeof(dd_task_list ));
									elem->next_task = current_task->next_task;
									elem->task = released_task;
									current_task->next_task = elem;
									break;
								}

								current_task = current_task->next_task;
							}//end traversal
						}
					}
				}//released tasks
				break;
			}
			case COMPLETED:
			{
				//completed tasks
				uint32_t completed_task_id;
				if (xQueueReceive(xQueue_completed, &completed_task_id, 0)) {
					// Clear preempt queue for completed task
					if(completed_task_id == 1){
						xQueueReceive(xQueue_preempt_1, &preempt, 0);
					}else if(completed_task_id == 2){
						xQueueReceive(xQueue_preempt_2, &preempt, 0);
					}else if(completed_task_id == 3){
						xQueueReceive(xQueue_preempt_3, &preempt, 0);
					}

					dd_task_list * current_task = active_list;

					if (active_list == NULL) {
						//ERROR STATE BAD
						break;
					}

					dd_task_list * prev = NULL;

					while (1) {
						if(current_task == NULL){
							// completed task is not found
							break;
						}

						//loop through task list until completed found
						if (current_task->task.task_id == completed_task_id) {
							current_task->task.completion_time = xTaskGetTickCount();

							vTaskPrioritySet(current_task->task.t_handle, default_priority);

							//should be first one
							if (prev == NULL) {
								active_list = active_list->next_task;
							}
							else {
								//set previous items next task to be the one after this one
								prev->next_task = current_task->next_task;
							}
							if(active_list != NULL){
								if(active_list->task.release_time == 0){
									active_list->task.release_time = time;
								}
								vTaskPrioritySet(active_list->task.t_handle, running_priority);
							}

							if (completed_list == NULL) {
								current_task->next_task = NULL;
								completed_list = current_task;
							}
							else {
								dd_task_list * last_task = completed_list;

								while (last_task->next_task != NULL) {
									last_task = last_task->next_task;
								}
								current_task->next_task = NULL;
								last_task->next_task = current_task;
							}
							break;
						}

						prev = current_task;
						current_task = current_task->next_task;
					}
				}//completed tasks
				break;
			}
			case MONITOR:
			{
				if (xQueueSend(xQueue_task_lists, &active_list, pdMS_TO_TICKS(500)) == pdFALSE) {
					// Failed to send active lists due to queue command queue too full
					printf("Failed to send active lists\n");
					break;
				}

				if (xQueueSend(xQueue_task_lists, &completed_list, pdMS_TO_TICKS(500)) == pdFALSE) {
					// Failed to send active lists due to queue command queue too full
					printf("Failed to send completed lists\n");
					break;
				}

				if (xQueueSend(xQueue_task_lists, &overdue_list, pdMS_TO_TICKS(500)) == pdFALSE) {
					// Failed to send active lists due to queue command queue too full
					printf("Failed to send overdue lists\n");
					break;
				}
				vTaskPrioritySet(xTaskMonitor, scheduler_priority);
				break;
			}
			default:
			{
				printf("Unrecongized msg_type\n");
				continue;
			}
		}

		// Run overdue checking at the end of every command


		// Check if head of active list is overdue
		if (active_list->task.absolute_deadline < time) {

			dd_task_list * overdue_node = active_list;

			vTaskPrioritySet(active_list->task.t_handle, default_priority);

			// Remove head from active list
			active_list = active_list->next_task;

			overdue_node->next_task = NULL;

			if(active_list != NULL){
				if(active_list->task.release_time == 0){
					active_list->task.release_time = time;
				}
				vTaskPrioritySet(active_list->task.t_handle, running_priority);
			}

			if (overdue_list == NULL) {
				overdue_list = overdue_node;
			}
			else {
				dd_task_list * current_task = overdue_list;
				while (1) {
					if (current_task->next_task == NULL) {
						current_task->next_task = overdue_node;
						break;
					}
					current_task = current_task->next_task;
				}
			}

			dd_task task_to_overdue = overdue_node->task;

			// Clear preempt queue for overdue task
			if(task_to_overdue.task_id == 1){
				xQueueReceive(xQueue_preempt_1, &preempt, 0);
			}else if(task_to_overdue.task_id == 2){
				xQueueReceive(xQueue_preempt_2, &preempt, 0);
			}else if(task_to_overdue.task_id == 3){
				xQueueReceive(xQueue_preempt_3, &preempt, 0);
			}
		}
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
static void Create_Task_One(TimerHandle_t xTimer)
{
	//Call create dd task
	create_dd_task(xTask1Handle, PERIODIC, 1, task_1_Period);
	xTimerChangePeriod(xTimer, pdMS_TO_TICKS(task_1_Period), pdMS_TO_TICKS(10));
}

static void Create_Task_Two(TimerHandle_t xTimer)
{
	//Call create dd task
	create_dd_task(xTask2Handle, PERIODIC, 2, task_2_Period);
	xTimerChangePeriod(xTimer, pdMS_TO_TICKS(task_2_Period), pdMS_TO_TICKS(10));
}

static void Create_Task_Three(TimerHandle_t xTimer)
{
	//Call create dd task
	create_dd_task(xTask3Handle, PERIODIC, 3, task_3_Period);
	xTimerChangePeriod(xTimer, pdMS_TO_TICKS(task_3_Period), pdMS_TO_TICKS(10));
}

static void Adjust_monitor_prio(TimerHandle_t xTimer)
{
	msg_type cmd = MONITOR;
	if(xQueueSend(xQueue_command, &cmd, 0) == pdFALSE) {
		// Failed to send enum to command queue
		printf("Failed to send monitor cmd to command queue\n");
	}
}

/*-----------------------------------------------------------*/

/*	@brief	User defined tasks
 *
 * 	@note	Wrapped as a dd task
 *
 * 	@retval	None
 *
 * */
//TODO: track preemption time left off points
static void First_Task(void *pvParameters)
{
	while (1) {
		uint32_t preempt_start = 0;
		uint32_t total_preempt_time = 0;
		int start = xTaskGetTickCount();

		if(xQueueReceive(xQueue_preempt_1, &preempt_start, 0) == pdTRUE){}
		preempt_start = 0;
		int temp = pdMS_TO_TICKS(task_1_Execution) + start + total_preempt_time;
		while (xTaskGetTickCount() != pdMS_TO_TICKS(task_1_Execution) + start + total_preempt_time) {
			if(xQueueReceive(xQueue_preempt_1, &preempt_start, 0) == pdTRUE){
				total_preempt_time += (xTaskGetTickCount() - preempt_start);
				continue;
			}
		}
		delete_dd_task(1);
	}
}

static void Second_Task(void *pvParameters)
{
	while (1) {
		uint32_t preempt_start = 0;
		uint32_t total_preempt_time = 0;
		int start = xTaskGetTickCount();

		if(xQueueReceive(xQueue_preempt_2, &preempt_start, 0) == pdTRUE){}
		preempt_start = 0;

		while (xTaskGetTickCount() != pdMS_TO_TICKS(task_2_Execution) + start + total_preempt_time) {
			if(xQueueReceive(xQueue_preempt_2, &preempt_start, 0) == pdTRUE){
				total_preempt_time += (xTaskGetTickCount() - preempt_start);
				continue;
			}
		}
		delete_dd_task(2);
	}
}

static void Third_Task(void *pvParameters)
{
	while (1) {
		uint32_t preempt_start = 0;
		uint32_t total_preempt_time = 0;
		int start = xTaskGetTickCount();

		if(xQueueReceive(xQueue_preempt_3, &preempt_start, 0) == pdTRUE){}
		preempt_start = 0;

		while (xTaskGetTickCount() != pdMS_TO_TICKS(task_3_Execution) + start + total_preempt_time) {
			if(xQueueReceive(xQueue_preempt_3, &preempt_start, 0) == pdTRUE){
				total_preempt_time += (xTaskGetTickCount() - preempt_start);
				continue;
			}
		}
		delete_dd_task(3);
	}
}


/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook(void)
{
	/* The malloc failed hook is enabled by setting
	configUSE_MALLOC_FAILED_HOOK to 1 in FreeRTOSConfig.h.

	Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for (;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(xTaskHandle pxTask, signed char *pcTaskName)
{
	(void)pcTaskName;
	(void)pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.  pxCurrentTCB can be
	inspected in the debugger if the task name passed into this function is
	corrupt. */
	for (;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
	volatile size_t xFreeStackSpace;

	/* The idle task hook is enabled by setting configUSE_IDLE_HOOK to 1 in
	FreeRTOSConfig.h.

	This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amount of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if (xFreeStackSpace > 100)
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}
/*-----------------------------------------------------------*/

static void prvSetupHardware(void)
{
	/* Ensure all priority bits are assigned as preemption priority bits.
	http://www.freertos.org/RTOS-Cortex-M3-M4.html */
	NVIC_SetPriorityGrouping(0);

	/* TODO: Setup the clocks, etc. here, if they were not configured before
	main() was called. */
}
