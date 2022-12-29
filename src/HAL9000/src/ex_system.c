#include "HAL9000.h"
#include "ex_system.h"
#include "thread_internal.h"
#include "pte.h"
#include "vmm.h"
#include "process.h"
#include "process_internal.h"

void
ExSystemTimerTick(
    void
    )
{
    ThreadTick();

    //Review Problems - VirtualMemory - 6
    PML4 cr3;
    cr3.Raw = (QWORD)__readcr3();

    INTR_STATE oldState;
    LIST_ITERATOR it;
    PPROCESS currentProcess = GetCurrentProcess();

    if (currentProcess != NULL) {
        LockAcquire(&currentProcess->FrameMapLock, &oldState);
        ListIteratorInit(&currentProcess->FrameMappingsHead, &it);

        PLIST_ENTRY pEntry;
        while ((pEntry = ListIteratorNext(&it)) != NULL)
        {
            PFRAME_MAPPING frameMapping = CONTAINING_RECORD(pEntry, FRAME_MAPPING, ListEntry);
            BOOLEAN bAccessed = FALSE;
            BOOLEAN bDirty = FALSE;

            PHYSICAL_ADDRESS pAddr = VmmGetPhysicalAddressEx(cr3,
                (PVOID)frameMapping->VirtualAddress,
                &bAccessed,
                &bDirty);
            if (bAccessed == TRUE || bDirty == TRUE) {
                LOGL("The page from address 0x%x is dirty/accessed!\n", pAddr);
            }
        }

        LockRelease(&currentProcess->FrameMapLock, oldState);
    }
}