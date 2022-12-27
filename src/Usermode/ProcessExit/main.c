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

    /*status = SyscallProcessExit(STATUS_SUCCESS);
    LOG_ERROR("The exit function should not return!\n");*/

    //Review Problems - Userprog - 4
    BYTE   Address;
    status = SyscallMemset(&Address,
        1,
        1);
    if (!SUCCEEDED(status))
    {
        LOG_FUNC_ERROR("SyscallMemset", status);
    }

    return STATUS_SUCCESS;
}