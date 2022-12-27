#include "HAL9000.h"
#include "syscall.h"
#include "gdtmu.h"
#include "syscall_defs.h"
#include "syscall_func.h"
#include "syscall_no.h"
#include "mmu.h"
#include "process_internal.h"
#include "dmp_cpu.h"
#include "thread.h"
#include "thread_internal.h"
#include "vmm.h"
#include "io.h"
#include "iomu.h"

extern void SyscallEntry();

#define SYSCALL_IF_VERSION_KM       SYSCALL_IMPLEMENTED_IF_VERSION

void
SyscallHandler(
    INOUT   COMPLETE_PROCESSOR_STATE    *CompleteProcessorState
    )
{
    SYSCALL_ID sysCallId;
    PQWORD pSyscallParameters;
    PQWORD pParameters;
    STATUS status;
    REGISTER_AREA* usermodeProcessorState;

    ASSERT(CompleteProcessorState != NULL);

    // It is NOT ok to setup the FMASK so that interrupts will be enabled when the system call occurs
    // The issue is that we'll have a user-mode stack and we wouldn't want to receive an interrupt on
    // that stack. This is why we only enable interrupts here.
    ASSERT(CpuIntrGetState() == INTR_OFF);
    CpuIntrSetState(INTR_ON);

    LOG_TRACE_USERMODE("The syscall handler has been called!\n");

    status = STATUS_SUCCESS;
    pSyscallParameters = NULL;
    pParameters = NULL;
    usermodeProcessorState = &CompleteProcessorState->RegisterArea;

    __try
    {
        if (LogIsComponentTraced(LogComponentUserMode))
        {
            DumpProcessorState(CompleteProcessorState);
        }

        // Check if indeed the shadow stack is valid (the shadow stack is mandatory)
        pParameters = (PQWORD)usermodeProcessorState->RegisterValues[RegisterRbp];
        status = MmuIsBufferValid(pParameters, SHADOW_STACK_SIZE, PAGE_RIGHTS_READ, GetCurrentProcess());
        if (!SUCCEEDED(status))
        {
            LOG_FUNC_ERROR("MmuIsBufferValid", status);
            __leave;
        }

        sysCallId = usermodeProcessorState->RegisterValues[RegisterR8];

        LOG_TRACE_USERMODE("System call ID is %u\n", sysCallId);

        // The first parameter is the system call ID, we don't care about it => +1
        pSyscallParameters = (PQWORD)usermodeProcessorState->RegisterValues[RegisterRbp] + 1;

        // Dispatch syscalls
        switch (sysCallId)
        {
        case SyscallIdIdentifyVersion:
            status = SyscallValidateInterface((SYSCALL_IF_VERSION)*pSyscallParameters);
            break;
        // STUDENT TODO: implement the rest of the syscalls
        //Review Problems - Userprog - 1
        case SyscallIdProcessExit:
            status = SyscallProcessExit((STATUS)*pSyscallParameters);
            break;
        case SyscallIdThreadExit:
            status = SyscallThreadExit((STATUS)*pSyscallParameters);
            break;
        case SyscallIdVirtualAlloc:
            status = SyscallVirtualAlloc(
                (PVOID)pSyscallParameters[0],
                (QWORD)pSyscallParameters[1],
                (VMM_ALLOC_TYPE)pSyscallParameters[2],
                (PAGE_RIGHTS)pSyscallParameters[3],
                (UM_HANDLE)pSyscallParameters[4],
                (QWORD)pSyscallParameters[5],
                (PVOID*)pSyscallParameters[6]
            );
            break;
        //Review Problems - Userprog - 2
        case SyscallIdFileWrite:
            status = SyscallFileWrite(
                (UM_HANDLE)pSyscallParameters[0],
                (PVOID)pSyscallParameters[1],
                (QWORD)pSyscallParameters[2],
                (QWORD*)pSyscallParameters[3]
            );
            break;
        case SyscallIdVirtualFree:
            status = SyscallVirtualFree(
                (PVOID)pSyscallParameters[0],
                (QWORD)pSyscallParameters[1],
                (VMM_FREE_TYPE)pSyscallParameters[2]
            );
            break;
        //Review Problems - Userprog - 5
        case SyscallIdProcessCreate:
            status = SyscallProcessCreate(
                (char*)pSyscallParameters[0],
                (QWORD)pSyscallParameters[1],
                (char*)pSyscallParameters[2],
                (QWORD)pSyscallParameters[3],
                (UM_HANDLE*)pSyscallParameters[4]
            );
            break;
        //Review Problems - Userprog - 4
        case SyscallIdMemset:
            status = SyscallMemset(
                (PBYTE)pSyscallParameters[0],
                (DWORD)pSyscallParameters[1],
                (BYTE)pSyscallParameters[2]
            );
            break;
        default:
            LOG_ERROR("Unimplemented syscall called from User-space %d!\n", sysCallId);
            status = STATUS_UNSUPPORTED;
            break;
        }

    }
    __finally
    {
        LOG_TRACE_USERMODE("Will set UM RAX to 0x%x\n", status);

        usermodeProcessorState->RegisterValues[RegisterRax] = status;

        CpuIntrSetState(INTR_OFF);
    }
}

void
SyscallPreinitSystem(
    void
    )
{

}

STATUS
SyscallInitSystem(
    void
    )
{
    return STATUS_SUCCESS;
}

STATUS
SyscallUninitSystem(
    void
    )
{
    return STATUS_SUCCESS;
}

void
SyscallCpuInit(
    void
    )
{
    IA32_STAR_MSR_DATA starMsr;
    WORD kmCsSelector;
    WORD umCsSelector;

    memzero(&starMsr, sizeof(IA32_STAR_MSR_DATA));

    kmCsSelector = GdtMuGetCS64Supervisor();
    ASSERT(kmCsSelector + 0x8 == GdtMuGetDS64Supervisor());

    umCsSelector = GdtMuGetCS32Usermode();
    /// DS64 is the same as DS32
    ASSERT(umCsSelector + 0x8 == GdtMuGetDS32Usermode());
    ASSERT(umCsSelector + 0x10 == GdtMuGetCS64Usermode());

    // Syscall RIP <- IA32_LSTAR
    __writemsr(IA32_LSTAR, (QWORD) SyscallEntry);

    LOG_TRACE_USERMODE("Successfully set LSTAR to 0x%X\n", (QWORD) SyscallEntry);

    // Syscall RFLAGS <- RFLAGS & ~(IA32_FMASK)
    __writemsr(IA32_FMASK, RFLAGS_INTERRUPT_FLAG_BIT);

    LOG_TRACE_USERMODE("Successfully set FMASK to 0x%X\n", RFLAGS_INTERRUPT_FLAG_BIT);

    // Syscall CS.Sel <- IA32_STAR[47:32] & 0xFFFC
    // Syscall DS.Sel <- (IA32_STAR[47:32] + 0x8) & 0xFFFC
    starMsr.SyscallCsDs = kmCsSelector;

    // Sysret CS.Sel <- (IA32_STAR[63:48] + 0x10) & 0xFFFC
    // Sysret DS.Sel <- (IA32_STAR[63:48] + 0x8) & 0xFFFC
    starMsr.SysretCsDs = umCsSelector;

    __writemsr(IA32_STAR, starMsr.Raw);

    LOG_TRACE_USERMODE("Successfully set STAR to 0x%X\n", starMsr.Raw);
}

// SyscallIdIdentifyVersion
STATUS
SyscallValidateInterface(
    IN  SYSCALL_IF_VERSION          InterfaceVersion
)
{
    LOG_TRACE_USERMODE("Will check interface version 0x%x from UM against 0x%x from KM\n",
        InterfaceVersion, SYSCALL_IF_VERSION_KM);

    if (InterfaceVersion != SYSCALL_IF_VERSION_KM)
    {
        LOG_ERROR("Usermode interface 0x%x incompatible with KM!\n", InterfaceVersion);
        return STATUS_INCOMPATIBLE_INTERFACE;
    }

    return STATUS_SUCCESS;
}

// STUDENT TODO: implement the rest of the syscalls

//Review Problems - Userprog - 1
STATUS
SyscallProcessExit(
    IN      STATUS                  ExitStatus
)
{
    LOGL("Exiting a process.\n");
    PPROCESS Process;
    Process = GetCurrentProcess();

    //Review Problems - Userprog - 5
    PPROCESS systemProc = ProcessRetrieveSystemProcess();
    PPROCESS parentProcess = Process->ParentProcess;
    INTR_STATE oldState, oldStateSystem, oldStateParent;
    LIST_ITERATOR it;

    if (parentProcess != NULL) {
        LockAcquire(&parentProcess->ChildProcessesListLock, &oldStateParent);
        RemoveEntryList(&Process->ChildProcess);
        LockRelease(&parentProcess->ChildProcessesListLock, oldStateParent);
    }

    
    LockAcquire(&Process->ChildProcessesListLock, &oldState);
    ListIteratorInit(&Process->ChildProcessesList, &it);

    PLIST_ENTRY pEntry;
    while ((pEntry = ListIteratorNext(&it)) != NULL)
    {
        PPROCESS proc = CONTAINING_RECORD(pEntry, PROCESS, ChildProcess);

        LockAcquire(&systemProc->ChildProcessesListLock, &oldStateSystem);
        InsertTailList(&systemProc->ChildProcessesList, &proc->ChildProcess);
        LockRelease(&systemProc->ChildProcessesListLock, oldStateSystem);

        proc->ParentProcess = systemProc;
    }
    LockRelease(&Process->ChildProcessesListLock, oldState);

    //Review Problems - Userprog - 1
    Process->TerminationStatus = ExitStatus;
    ProcessTerminate(Process);
    return STATUS_SUCCESS; 
}

//Review Problems - Userprog - 1
STATUS
SyscallThreadExit(
    IN  STATUS                      ExitStatus
)
{
    ThreadExit(ExitStatus);
    return STATUS_SUCCESS;
}

STATUS
SyscallVirtualAlloc(
    IN_OPT      PVOID                   BaseAddress,
    IN          QWORD                   Size,
    IN          VMM_ALLOC_TYPE          AllocType,
    IN          PAGE_RIGHTS             PageRights,
    IN_OPT      UM_HANDLE               FileHandle,
    IN_OPT      QWORD                   Key,
    OUT         PVOID* AllocatedAddress
) {
    UNREFERENCED_PARAMETER(FileHandle);
    UNREFERENCED_PARAMETER(Key);

    *AllocatedAddress = VmmAllocRegionEx(
        BaseAddress,
        Size,
        AllocType,
        PageRights,
        FALSE,
        NULL,
        GetCurrentProcess()->VaSpace,
        GetCurrentProcess()->PagingData,
        NULL
    );
    return STATUS_SUCCESS;
}

STATUS
SyscallVirtualFree(
    IN          PVOID                   Address,
    _When_(VMM_FREE_TYPE_RELEASE == FreeType, _Reserved_)
    _When_(VMM_FREE_TYPE_RELEASE != FreeType, IN)
    QWORD                   Size,
    IN          VMM_FREE_TYPE           FreeType
)
{
    VmmFreeRegionEx(
        Address,
        Size,
        FreeType,
        TRUE,
        GetCurrentProcess()->VaSpace,
        GetCurrentProcess()->PagingData
    );
    return STATUS_SUCCESS;
}

//Review Problems - Userprog - 2
STATUS
SyscallFileWrite(
    IN  UM_HANDLE                   FileHandle,
    IN_READS_BYTES(BytesToWrite)
    PVOID                       Buffer,
    IN  QWORD                       BytesToWrite,
    OUT QWORD* BytesWritten
)
{
    if (BytesWritten == NULL) {
        return STATUS_UNSUCCESSFUL;
    }
    if (FileHandle == UM_FILE_HANDLE_STDOUT) {
        *BytesWritten = BytesToWrite;
        LOG("[%s]:[%s]\n", ProcessGetName(NULL), Buffer);
        return STATUS_SUCCESS;
    }

    *BytesWritten = BytesToWrite;
    return STATUS_SUCCESS;
}

//Review Problems - Userprog - 4
STATUS
SyscallMemset(
    OUT_WRITES(BytesToWrite)    PBYTE   Address,
    IN                          DWORD   BytesToWrite,
    IN                          BYTE    ValueToWrite
) {
    if (Address == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    memset(Address, ValueToWrite, BytesToWrite);

    return STATUS_SUCCESS;
}

//Review Problems - Userprog - 5
STATUS
SyscallProcessCreate(
    IN_READS_Z(PathLength)
    char* ProcessPath,
    IN          QWORD               PathLength,
    IN_READS_OPT_Z(ArgLength)
    char* Arguments,
    IN          QWORD               ArgLength,
    OUT         UM_HANDLE* ProcessHandle
) {
    STATUS status = STATUS_SUCCESS;
    if (ProcessPath == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    if (ProcessHandle == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    status = MmuIsBufferValid((PVOID)ProcessPath, PathLength, PAGE_RIGHTS_READ, GetCurrentProcess());
    if (!SUCCEEDED(status))
    {
        return status;
    }

    if (Arguments != NULL) {
        status = MmuIsBufferValid((PVOID)Arguments, ArgLength, PAGE_RIGHTS_READ, GetCurrentProcess());
        if (!SUCCEEDED(status))
        {
            return status;
        }
    }

    //Creating the path relative to the APPLICATIONS directory
    char Path[MAX_PATH];
    const char* SystemDrive = IomuGetSystemPartitionPath();
    snprintf(Path, MAX_PATH, "%s%s\\%s", SystemDrive, "APPLICATIONS", ProcessPath);

    PPROCESS newProcess = NULL;
    if (ArgLength > 0) {
        status = ProcessCreate(Path, Arguments, &newProcess);
    }
    else {
        status = ProcessCreate(Path, NULL, &newProcess);
    }
    if (!SUCCEEDED(status))
    {
        return STATUS_UNSUCCESSFUL;
    }

    if (newProcess == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //Insert created process in list of children processes
    INTR_STATE oldState;
    PPROCESS currentProcess = GetCurrentProcess();
    if (currentProcess == NULL) {
        return STATUS_UNSUCCESSFUL;
    }
    LockAcquire(&currentProcess->ChildProcessesListLock, &oldState);
    InsertTailList(&currentProcess->ChildProcessesList, &newProcess->ChildProcess);
    LockRelease(&currentProcess->ChildProcessesListLock, oldState); \

    newProcess->ParentProcess = currentProcess;

    *ProcessHandle = newProcess->Id;
    return status;
}
