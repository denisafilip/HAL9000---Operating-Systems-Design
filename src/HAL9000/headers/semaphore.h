#pragma once

#include "list.h"
#include "synch.h"


typedef struct _SEMAPHORE
{
	LOCK         SemaphoreLock;

	_Guarded_by_(SemaphoreLock)
		DWORD           Value;

	_Guarded_by_(SemaphoreLock)
		LIST_ENTRY      WaitingList;
} SEMAPHORE, * PSEMAPHORE;


void
SemaphoreInit(
	OUT     PSEMAPHORE      Semaphore,
	IN      DWORD           InitialValue
);


//******************************************************************************
// Function:     SemaphoreDown
// Description:  Decrements the value of semaphore variable by 1. If the new 
//			     value of the semaphore variable is negative, the process 
//				 executing wait is blocked (i.e., added to the semaphore's 
//				 WaitingList). Otherwise, the process continues execution, 
//				 having used a unit of the resource.
// Returns:      void
// Parameter:    INOUT PSEMAPHORE Semaphore
//				 IN	   DWORD      Value
//******************************************************************************
void
SemaphoreDown(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
);

//******************************************************************************
// Function:     SemaphoreUp
// Description:  Increments the value of semaphore variable by 1. After the 
//				 increment, if the pre-increment value was negative (meaning 
//				 there are processes waiting for a resource), it transfers a 
//				 blocked process from the semaphore's WaitingList to the 
//				 ReadyThreadsList.
// Returns:      void
// Parameter:    INOUT PSEMAPHORE Semaphore
//				 IN	   DWORD      Value
//******************************************************************************
void
SemaphoreUp(
	INOUT   PSEMAPHORE      Semaphore,
	IN      DWORD           Value
);
