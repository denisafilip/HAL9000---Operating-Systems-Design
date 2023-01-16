#include "common_lib.h"
#include "syscall_if.h"
#include "um_lib_helper.h"

STATUS
__main(
    DWORD       argc,
    char**      argv
)
{
    STATUS status;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    QWORD numberOfThreads;
 
    status = SyscallGetNumberOfThreadsInInterval(0, 60000000, &numberOfThreads);
    if (!SUCCEEDED(status))
    {
        LOG_ERROR("SyscallGetNumberOfThreadsInInterval should have succeeded!!!\n");
    }
    else {
        LOGL("Number of threads in interval: %d.\n", numberOfThreads);
    }

    //status = SyscallFileClose(0x73213213);
    /*if (SUCCEEDED(status))
    {
        LOG_ERROR("SyscallFileClose should have failed!!!\n");
    }*/

    return STATUS_SUCCESS;
}