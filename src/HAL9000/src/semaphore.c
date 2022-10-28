#include "HAL9000.h"
#include "thread_internal.h"
#include "semaphore.h"


void
SemaphoreInit(
    OUT     PSEMAPHORE      Semaphore,
    IN      DWORD           InitialValue
) {
    memzero(&Semaphore, sizeof(PSEMAPHORE));

    InitializeListHead(&Semaphore->WaitingList);
    LockInit(&Semaphore->SemaphoreLock);

    Semaphore->Value = InitialValue;
}

void
SemaphoreDown(
    INOUT   PSEMAPHORE      Semaphore,
    IN      DWORD           Value
) {

    INTR_STATE oldState;
    INTR_STATE dummyState;
    PTHREAD pCurrentThread = GetCurrentThread();

    ASSERT(NULL != Semaphore);
    ASSERT(NULL != pCurrentThread);

    oldState = CpuIntrDisable();

    LockAcquire(&Semaphore->SemaphoreLock, &dummyState);

    Semaphore->Value = Semaphore->Value - Value;

    if (Semaphore->Value < 0) {
        InsertTailList(&Semaphore->WaitingList, &pCurrentThread->ReadyList);
        ThreadTakeBlockLock();
        LockRelease(&Semaphore->SemaphoreLock, dummyState);
        ThreadBlock();
        LockAcquire(&Semaphore->SemaphoreLock, &dummyState);
    }
    LockRelease(&Semaphore->SemaphoreLock, dummyState);

    CpuIntrSetState(oldState);
}

void
SemaphoreUp(
    INOUT   PSEMAPHORE      Semaphore,
    IN      DWORD           Value
) {
    INTR_STATE oldState; 
    PLIST_ENTRY pEntry;

    ASSERT(NULL != Semaphore);

    pEntry = NULL;

    LockAcquire(&Semaphore->SemaphoreLock, &oldState);
    DWORD oldValue = Semaphore->Value;
    Semaphore->Value = Semaphore->Value + Value;

    if (oldValue < 0) {
        pEntry = RemoveHeadList(&Semaphore->WaitingList);
        if (pEntry != &Semaphore->WaitingList)
        {
            PTHREAD pThread = CONTAINING_RECORD(pEntry, THREAD, ReadyList);

            // wakeup the first waiting thread
            ThreadUnblock(pThread);
        }
    }

    LockRelease(&Semaphore->SemaphoreLock, oldState);
}