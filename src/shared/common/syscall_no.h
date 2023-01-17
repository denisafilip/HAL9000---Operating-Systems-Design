#pragma once

typedef enum _SYSCALL_ID
{
    SyscallIdIdentifyVersion,

    // Thread Management
    SyscallIdThreadExit,
    SyscallIdThreadCreate,
    SyscallIdThreadGetTid,
    SyscallIdThreadWaitForTermination,
    SyscallIdThreadCloseHandle,

    // Process Management
    SyscallIdProcessExit,
    SyscallIdProcessCreate,
    SyscallIdProcessGetPid,
    SyscallIdProcessWaitForTermination,
    SyscallIdProcessCloseHandle,

    // Memory management 
    SyscallIdVirtualAlloc,
    SyscallIdVirtualFree,

    //Review Problems - Userprog - 4
    SyscallIdMemset,

    //Review Problems - Userprog - 6
    SyscallIdDisableSyscalls,

    // File management
    SyscallIdFileCreate,
    SyscallIdFileClose,
    SyscallIdFileRead,
    SyscallIdFileWrite,

    SyscallIdProcessGetNumberOfPages,
    SyscallIdReadMemory,

    SyscallIdReserved = SyscallIdFileWrite + 1
} SYSCALL_ID;
